//want to send data over mains coupling
/*
   this code belongs taw tsv.
   Written for final year project
   Title: Powerline Communication Modem
   Supervised by: Dr. T. Marisa
   Electrical Engineering Department
   University of Zimbabwe
*/

#include <SPI.h>
#include "TimerOne.h"
#include "main_code.h"

void loop() {
  if (receiveStringFromPC(20) == true) {
    //send message to mains
    sendToMains(serialReceiveBuffer, strlen(serialReceiveBuffer));
    //wait for response
    if (receiveFromMains(500) == true) {
      Serial.print(mainsReceiveBuffer);
    } else {
      Serial.print(NO_RESPONSE);
    }
  }
  //check for general message
  if (receiveFromMains(500) == true) {
    Serial.print(mainsReceiveBuffer);
    //send back acknowledgement of reception
    sendToMains(ACK, strlen(ACK));
  } else {
    Serial.print(NO_MESSAGE);
  }
}

//used to detect start of message
char startSeq[] = {0b10101010, 0b01010101};
char offset = 6; //number of bytes appended to beginning of message

void sendToMains(char* arr, char len) {
  //append leading bits and put message in buffer
  for (int x = 0; x < offset; x += 2) {
    mainsSendBuffer[x] = startSeq[0];
    mainsSendBuffer[x + 1] = startSeq[1];
  }
  for (int x = 0; x < len; x++) {
    mainsSendBuffer[x + offset] = arr[x];
  }
  mainsSendBuffer[len + offset] = 0;

  for (int x = len + offset + 1; x < mainsBufferSize; x += 2) {
    mainsSendBuffer[x] = startSeq[1];
    mainsSendBuffer[x + 1] = startSeq[1];
  }

  buf = mainsSendBuffer;
  datacount = mainsBufferSize;
  transferMode = WRITE_MAINS;

  readWrite();
}

bool receiveFromMains(int timeout) {
  //returns false if nothing received in given timeout milliseconds.
  //message is store in buffer array mainsReceiveBuffer

  //clear arr
  for (int x = 0; x < mainsBufferSize; x++) mainsReceiveBuffer[x] = 0;

  //check if anything to receive for given timeout using
  //carrier detect line which goes low when carrier freq detected
  unsigned int timeElapsed = 0;
  while (digitalRead(CD_PD)) {
    delay(5);
    timeElapsed += 5;
    if (timeElapsed >= timeout) return false;
  }

  //something to receive available
  buf = mainsReceiveBuffer;
  datacount = mainsBufferSize;
  transferMode = READ_MAINS;

  readWrite();

  //align bits and remove starting sequence to give real message received.
  int totalBits = 8 * mainsBufferSize;
  int seqSize = 16;
  signed int index = getStartOfSequence(mainsReceiveBuffer, totalBits, startSeq, seqSize);
  if (index >= 0) index += seqSize; //point at start of message
  else return false;
  leftShiftArray(mainsReceiveBuffer, mainsBufferSize, index);
  for (int x = 0; x < mainsBufferSize - offset; x++) {
    if ((mainsReceiveBuffer[0] == startSeq[0]) && (mainsReceiveBuffer[1] == startSeq[1])) {
      leftShiftArray(mainsReceiveBuffer, mainsBufferSize, 16);
    } else break;
  }
  return true;
}

void sendStringToPC() {
  //array must end with null character '\0'
  //to be a valid string array'
  Serial.println(mainsReceiveBuffer);
}

bool receiveStringFromPC(char timeout) {
  //try to receive data from pc within timeout milliseconds
  //return true if string received and false otherwise.
  //store received string in serialReceiveBuffer
  unsigned int timeElapsed = 0;
  while (Serial.available() < 1) {
    delay(5);
    timeElapsed += 5;
    if (timeElapsed > timeout) return false;
  }
  char i = 0;
  while (Serial.available() > 0 && i < serialBufferSize) {
    serialReceiveBuffer[i] = (char) Serial.read();
    i++;
    delay(1);
  }
  serialReceiveBuffer[i] = 0; //ensure string ends with null character
  return true;
}
