#!/usr/bin/env python3

from PySide2.QtCore import QThread
from queue import Queue
import time
import serial,  serial.tools.list_ports


JOBCODE = 0;  DATA = 1;  TIMEOUT = 2;
    
#jobcodes used to identify the kind of operation needed for a job.
RECV = "receive"
SEND = "send"

#special codes
#sent to pc when setup is complete and transfers can be done
SETUP_COMPLETE = "#101"
#sent to PC when modem is found connected
MODEM_BOOTED = "#102"
#sent to PC when modem is not connected
WAITING_FOR_MODEM_BOOT = "#103"
#sent to PC when control register is set
CR_SET = "#104"
#sent to PC when no response is received after sending data to mains
NO_RESPONSE = "#105"
#sent when a general read from mains does not return anything
NO_MESSAGE = "#106"
#sent over mains to acknowledge message received
ACK = "#107"
    
class Communicator(QThread):
    """For sending and receiving data via USB serial by the user."""
    """Encapsulates operations into simple send and receive methods"""
    
    def __init__(self,  guiObject):
        QThread.__init__(self)
        
        #list of jobs to do. handled as a queue. Each entry in the form a tuple
        #with (code, data, handler, timeout) for a send operation 
        self._jobs = Queue()
        self.guiObject = guiObject
        self._ser = None

        self.start()
        
    def send(self,  data, responseHandler, timeout = 1):
        """Sends data over USB serial asynchronously and returns immediately."""
        """Must use handler to get information about success of operation."""
        """Timeout is the number of seconds the object should try to send """
        """data before giving up and sending back an error, """
        """returning a None represents an error. """
        """ResponseHandler is an object that must implement the method
        handleResponse"""
        packet = [None for x in range(20)]
        packet[JOBCODE] = SEND
        packet[DATA] = data
        packet[TIMEOUT] = timeout
        self._jobs.put(packet)
        print("Job added: " + str(packet[:3]))
        
    def receive(self, responseHandler, timeout = 1):
        """Returns data sent from USB serial within the specified """
        """timeout seconds"""
        packet = [None for x in range(20)]
        packet[JOBCODE] = RECV
        packet[TIMEOUT] = timeout
        self._jobs.put(packet)
        print("Job added: " + str(packet[:3]))
        
    def run(self):
        
        print("Serial comms process is now running in the background...")
        while True:
            if (self._ser is None or not self._ser.is_open):
                #wait for port to be opened by user in gui
                time.sleep(0.05)
                continue
            if not self._jobs.empty():
                print("Processing job...")
                packet = self._jobs.get()
                jobcode = packet[0]
                if jobcode == SEND:
                    self._send(packet)
            #try to receive something
            self._recv(None)
            time.sleep(0.05) #rest for a bit
        
    """Raw code for sending and receiving via USB serial without any """
    """error handling implemented."""
    """the Packet represents an item in the queue of jobs to be done, """
    """currently implemented as a tuple as mentioned above."""
    def _send(self, packet):
        data = bytearray(packet[DATA],  'ascii')
        print("Sending data...")
        try:
            self._ser.write(data)
            print("Data written")
            #get response
            mybytes = self._ser.readline()
            line = str(mybytes, "ascii")
            count = 5
            while line.find(NO_MESSAGE) >=0:
                print(line)
                #read again 5 times waiting for proper response
                mybytes = self._ser.readline()
                line = str(mybytes, "ascii")
                count -= 1
                if count < 0:
                    #no response to message
                    self.guiObject.responseSignal.emit(NO_RESPONSE)
                    return
            print("Response is: " + line)
            self.guiObject.responseSignal.emit(line)
        except Exception as e:
           self.guiObject.errorSignal.emit(str(e))
        print("Done")
        
    def _recv(self,  packet):
        print("Receiving data...")
        try:
            line = str(self._ser.readline(), "ascii")
            if line.find(NO_MESSAGE) >=0:
                return
            self.guiObject.responseSignal.emit(line)
        except Exception as e:
            self.guiObject.errorSignal.emit(str(e))
        print("Done")
        
    def _detect_ports(self):
        """detects ports connected to arduino and returns a list of them"""
        ports = list(serial.tools.list_ports.comports())
        arduinos = []
        for p in ports:
            #detect if port is arduinos
            #if "ACM" in p[1]:
            arduinos.append(p[0])
        return arduinos
        
        
if __name__ == "__main__":
    pass
