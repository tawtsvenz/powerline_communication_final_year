
/*
   this code belongs taw tsv.
   Written for final year project
   Title: Powerline Communication Modem
   Supervised by: Dr. T. Marisa
   Electrical Engineering Department
   University of Zimbabwe
*/

/*contains functions definitions and variables to make code cleaner and leaner*/

//Define the pins we're gonna be using for comms in hardware.
const char CD_PD = 6;
const char REG_DATA = 3;
const char RXTX = 5;
const char RSTO = 8;
const char UART_SPI = 9;
const char WD = 4;
const char SSdummy = 2;
//spi pins
//const char SS = 10;   //real ss pin being toggled using pin 2 above
//const char MOSI = 11; //rxd
//const char MISO = 12; //txd
//const char SCK = 13;  //CLR_T

//special codes
//sent to pc when setup is complete and transfers can be done
const char* SETUP_COMPLETE = "#101\r\n";
//sent to PC when modem is found connected
const char* MODEM_BOOTED = "#102\r\n";
//sent to PC when modem is not connected
const char* WAITING_FOR_MODEM_BOOT = "#103\r\n";
//sent to PC when control register is set
const char* CR_SET = "#104\r\n";
//sent to PC when no response is received after sending data to mains
const char* NO_RESPONSE = "#105\r\n";
//sent when a general read from mains does not return anything
const char* NO_MESSAGE = "#106\r\n";
//sent over mains to acknowledge message received
const char* ACK = "#107\r\n";

//speed of serial communication with pc
const unsigned int serialBaudrate = 57600;
//number of characters received over serial comms with pc
const char mainsBufferSize = 120;
const char serialBufferSize = mainsBufferSize - 16;
//size of control register in st7540. 3 bytes or 24 bits
const char controlRegSize = 3;

//default values to write to control register
volatile char crdefault[controlRegSize] = {0b11010010, 0b00100010, 0b00010011};
volatile char control_register[controlRegSize] = {0};
//to store string read from PC.
//+1 to allow space for '\0' null character needed to terminate string
//when string length equal to bufferSize.
//initialize both mains buffers to zero length strings
char mainsReceiveBuffer[mainsBufferSize + 1] = {0};
//to store string to be sent to mains
char mainsSendBuffer[mainsBufferSize + 1] = {0};

//initialize both serial buffers to zero length strings
char serialReceiveBuffer[mainsBufferSize + 1] = {0};

//for spi interrupt service routine
volatile char* buf;
volatile char datacount; //number of bytes to receive;
//set this flag to false to start transfer and check it periodically to
//detect if transfer is done.
volatile bool transferDone = true;

/*Modes of communication with ST7540

  Index Mode          REG_DATA  RxTx
    0   Transmission    0         0
    1   Reception       0         1
    2   CR Read         1         1
    3   CR Write        1         0
*/
const char WRITE_MAINS = 0;
const char READ_MAINS = 1;
const char READ_CR = 2;
const char WRITE_CR = 3;
//current comms mode
volatile char transferMode = READ_MAINS;
//list of modes and values of REG_DATA and RxTx for each mode
const char modesList[4][2] = { {LOW, LOW}, {LOW, HIGH}, 
                                  {HIGH, HIGH}, {HIGH, LOW}};

void printControlRegister() {
  for (char x = 0; x < 3; x++) {
    char b = control_register[x];
    for (char y = 0; y < 8; y++) {
      Serial.print(bitRead(b, y));
    }
    Serial.print(" ");
  }
  Serial.println();
}

uint8_t getArrayBit(char* arr, int bitNum) {
  //return the bit at bitNum of arr
  return bitRead(arr[bitNum / 8], (bitNum % 8));
}

void printArrayBits(uint8_t *arr, unsigned int from, unsigned int to) {
  //print the bits of the array starting at 'from' to 'to' inclusive.
  unsigned int count = 0;
  for (unsigned int y = from; y <= to; y++) {
    Serial.print(getArrayBit(arr, y));
    count++;
    if (count >= 8) {
      count = 0;
      Serial.print(" ");
    }
  }
  Serial.println();
}

signed int getStartOfSequence(char* arr, int arrlen, char* subseq, int subseqlen) {
  //return the end index of the sequence or -1 if sequence not found.
  //arrlen and subseqlen are in bits.
  //brute force search.
  if (subseqlen > arrlen) return -1; //not a subsequence
  bool found = false;
  int startIndex;
  for (startIndex = 0; startIndex < arrlen - subseqlen; startIndex++) {
    found = true;
    int secondIndex;
    for (secondIndex = 0; secondIndex < subseqlen; secondIndex++) {
      if (getArrayBit(subseq, secondIndex) != getArrayBit(arr, startIndex + secondIndex)) {
        found = false;
        break;
      }
    }
    if (found) break;
  }
  return (found) ? startIndex : -1;
}

void leftShiftArray(uint8_t *arr, int len, int num) {
  //left shift arr by num bits. arr is len bytes long.
  uint8_t firstbit;
  uint8_t lastbit;
  for (int j = 0; j < num; j++) {
    lastbit = bitRead(arr[0], 0);
    arr[0] >>= 1;
    for (int i = 1; i < len; i++) {
      firstbit = bitRead(arr[i], 0);
      arr[i - 1] = (firstbit == 1) ? bitSet(arr[i - 1], 7) : bitClear(arr[i - 1], 7);
      arr[i] >>= 1;
    }
    arr[len - 1] = (lastbit == 1) ? bitSet(arr[len - 1], 7) : bitClear(arr[len - 1], 7);
  }
}

void _toggleWatchdog() {
  //This toggles the WD pin.
  //It must be called regularly to prevent modem ic
  //from resetting itself from time to time
  // thinking that the host is dead.
  //toggle every 100 ms
  digitalWrite(WD, digitalRead(WD) ^ 1 );
}

void readWrite() {
  //performs the spi transfer according to the transferMode set.
  
  //set transfer mode for st7540
  digitalWrite(REG_DATA, modesList[transferMode][0]);
  digitalWrite(RXTX, modesList[transferMode][1]);

  //make arduino slave
  digitalWrite(SSdummy, LOW);
  transferDone = false; //set the flag to start transfer
  while (!transferDone) delay(1); //wait for transfer to complete
  //make arduino master
  digitalWrite(SSdummy, HIGH);

  //set default mode of reception from mains
  digitalWrite(REG_DATA, LOW);
  digitalWrite(RXTX, HIGH);
}

void readControlRegister() {
  //read 24 bits from control register
  //clear control register array
  for (char x = 0; x < controlRegSize; x++) control_register[x] = 0;
  buf = control_register;
  datacount = controlRegSize;
  transferMode = READ_CR;

  readWrite();
}

void writeControlRegister() {
  //write 24 bits to control register
  //write control register array to st7540
  buf = control_register;
  datacount = controlRegSize;
  transferMode = WRITE_CR;

  readWrite();
}

void setDefaultSettings() {
  //wait for modem to wake up
  //RSTO pin becomes 1 when modem wakes up
  Serial.print(WAITING_FOR_MODEM_BOOT);
  while (!digitalRead(RSTO));
  Serial.print(MODEM_BOOTED);
  
  while (true) {
    //check if modem already set to default settings
    readControlRegister();
    bool isSet = true;
    for (char i = 0; i < controlRegSize; i++) {
      if (control_register[i] != crdefault[i]) {
        isSet = false;
        break;
      }
    }
    if (isSet) {
      Serial.print(CR_SET);
      return;
    }

    //set default values to control register.
    for (char i = 0; i < controlRegSize; i++) {
      control_register[i] = crdefault[i];
    }
    writeControlRegister();
  }
}

// SPI interrupt routine
ISR (SPI_STC_vect) {
  if (transferDone || digitalRead(SSdummy) == HIGH) return;

  if (transferMode == READ_MAINS || transferMode == READ_CR) {
    //read over SPI
    buf[datacount - 1] = SPDR;
    for (signed int x = datacount - 2; x >= 0; x--) {
      buf[x] = SPI.transfer(0);
    }
  } else if (transferMode == WRITE_MAINS || transferMode == WRITE_CR) {
    //write over SPI
    for (signed int x = datacount - 1; x >= 0; x--) {
      SPI.transfer(buf[x]);
    }
  }
  transferDone = true;
}// end of interrupt routine SPI_STC_vect


void setup() {
  //initialise serial comms with PC
  Serial.begin(serialBaudrate);
  delay(100);

  //setup i/o pins
  pinMode(CD_PD, INPUT);
  pinMode(REG_DATA, OUTPUT);
  pinMode(RXTX, OUTPUT);
  pinMode(RSTO, INPUT);
  pinMode(UART_SPI, OUTPUT);
  pinMode(WD, OUTPUT);

  //set up for spi communication
  digitalWrite(UART_SPI, LOW);

  //set reception mode
  digitalWrite(REG_DATA, LOW);
  digitalWrite(RXTX, HIGH);

  //watchdog toggling timer
  Timer1.initialize(500000); //500ms
  Timer1.attachInterrupt(_toggleWatchdog);

  //setup as spi slave
  // turn on SPI in slave mode
  SPCR |= _BV(SPE);
  // have to send on master in, *slave out*
  pinMode(MISO, OUTPUT);
  //use this pin to control whether arduino is slave or not
  //since st7540 doesnt have ss pin.
  //HIGH makes arduino master while low makes arduino slave
  pinMode(SSdummy, OUTPUT);
  //make arduino master
  digitalWrite(SSdummy, HIGH);
  SPI.attachInterrupt();
  setDefaultSettings();
  Serial.print(SETUP_COMPLETE);
}

