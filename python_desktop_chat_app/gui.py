#!/usr/bin/env python3

import sys, serial, comms
from comms import Communicator

from PySide2.QtWidgets import QApplication, QWidget,  QVBoxLayout, QHBoxLayout,  QScrollArea,  QLineEdit, QPushButton,  QMessageBox, QLayout,  QInputDialog, QLabel
from PySide2.QtCore import Signal, Qt

class Gui(QWidget):
    
    responseSignal = Signal(str)
    progressSignal = Signal(str)
    errorSignal = Signal(str)
    
    def __init__(self):
        QWidget.__init__(self)
        
        #GUI layout
        self.setWindowTitle("PLC Modem Demo App")
        self.setGeometry(200, 200, 500,  500)
        self.setMaximumSize(600,  600)
        self.setMinimumSize(400,  200)
    
        self.scrollArea = QScrollArea()
        containerWidget = QWidget(self.scrollArea)
        self.scrollArea.setWidget(containerWidget)
        containerWidget.scrollarea = self.scrollArea
        self.messagesLayout = QVBoxLayout()
        self.messagesLayout.setSizeConstraint(QLayout.SizeConstraint.SetMinAndMaxSize)
        self.messagesLayout.setAlignment(Qt.AlignTop)
        containerWidget.setLayout(self.messagesLayout)
        self.messagesLabel = QLabel()
        self.messagesLayout.addWidget(self.messagesLabel)
        
        vlayout = QVBoxLayout()
        vlayout.addWidget(self.scrollArea)
        hlayout = QHBoxLayout()
        self.messageEditor = QLineEdit("")
        self.messageEditor.setPlaceholderText("Type message")
        self.messageEditor.returnPressed.connect(self.sendMessage)
        self.messageEditor.setMaxLength(32)
        hlayout.addWidget(self.messageEditor)
        self.sendButton = QPushButton("Send")
        self.sendButton.setMaximumHeight(25)
        self.sendButton.clicked.connect(self.sendMessage)
        hlayout.addWidget(self.sendButton)
        vlayout.addLayout(hlayout)
        
        setPortlayout = QHBoxLayout()
        self.setPortButton = QPushButton("Choose Modem")
        self.setPortButton.setMaximumHeight(25)
        self.setPortButton.clicked.connect(self.checkPort)
        setPortlayout.addWidget(self.setPortButton)
        self.portLabel = QLabel()
        self.portLabel.setMaximumHeight(50)
        setPortlayout.addWidget(self.portLabel)
        vlayout.addLayout(setPortlayout)
        
        self.setLayout(vlayout)
        
        #connect signals to slots
        self.responseSignal.connect(lambda data: self.onResponse(data))
        self.progressSignal.connect(lambda data: self.onProgress(data))
        self.errorSignal.connect(lambda data: self.onError(data))
        
        #initialise the background thread
        self.commsObject = Communicator(self)
    
    def sendMessage(self):
        if (not self.checkPort(forceNew = False)): return
            
        message = self.messageEditor.text()
        self.sendButton.setFocus()
        if (len(message) == 0): return
        self.messageEditor.setText("")
        self.messageEditor.setPlaceholderText("Type message")
        if not message.endswith("\n"): message += "\n"
        self.commsObject.send(message,  self)
        trail = str(self.messagesLabel.text())
        message = trail + "@: " + message.rstrip()
        self.messagesLabel.setText(message)
        
    def checkPort(self, forceNew = True):
        port = None
        ports = self.commsObject._detect_ports()
        ports.sort()
        try:
            #check if serial port is available and open
            if (forceNew and self.commsObject._ser):
                self.commsObject._ser.close()
                self.commsObject._ser = None
            if (not self.commsObject._ser):
                port, ok = QInputDialog.getItem(self, 'Input Combo Dialog', 'Choose Arduino port: ', ports)
                if ok:
                    self.commsObject._ser = serial.Serial(str(port), baudrate = 57600)
                    print("Port chosen " + str(port))
                else:
                    self.onError("Error opening port. Try again or choose another port !!!")
                    return False
            elif self.commsObject._ser.name not in ports:
                #port not listed but open. so rogona rabviswa
                raise serial.SerialException
            else :
                if not self.commsObject._ser.is_open:
                    print("Port not open. Opening port " + self.commsObject._ser.name)
                    self.commsObject._ser.open()
                else:
                    print("Port is still open. Port is " + self.commsObject._ser.name)
        except serial.SerialException as e:
            self.portLabel.setText("")
            if self.commsObject._ser:
                self.commsObject._ser = None
            self.onError("Error opening port. Try again or choose another port !!!")
            print(str(e))
            return False
        self.portLabel.setText(self.commsObject._ser.name)
        return True
        
    def onProgress(self, data):
        """slot for when there is progress in a background thread job"""
        pass
        
    def onResponse(self, data):
        """handle a response event from background thread"""
        trail = str(self.messagesLabel.text())
        if data.find(comms.SETUP_COMPLETE) >= 0 :
            message = trail + "<b>Setup yaita,  wakugona kutransfeya now.</b><br/><br/><br/>"
        elif data.find(comms.MODEM_BOOTED) >= 0:
            message = trail + "<b>Modem ramuka.</b><br/>"
        elif data.find(comms.WAITING_FOR_MODEM_BOOT) >= 0:
            message = trail + "<b>Tamirira modem rimuke ...</b><br/>"
        elif data.find(comms.CR_SET) >= 0:
            message = trail + "<b>Masettings aiswa muControl register.</b><br/>"
        elif data.find(comms.NO_RESPONSE) >= 0:
            message = trail.rstrip() + "  <sup>*</sup><br/>"
        elif  data.find(comms.ACK) >= 0:
            message = trail.rstrip() + "  <sup>**</sup><br/>"
        else:
            message = trail + "<br/><center>@: " + data + "</center><br/>"
        self.messagesLabel.setText(message)
        
    def onError(self,  data):
        """when error message is sent"""
        button = QPushButton("OK")
        QMessageBox.warning(button,  "Warning",  data)
        
    
if __name__ == "__main__":
    gui = None
    try:
        myApp = QApplication(sys.argv)
        gui = Gui()
        gui.show()
        myApp.exec_()
        sys.exit(0)
    except SystemExit:
        #kill background thread
        if gui:
            if gui.commsObject._ser:
                gui.commsObject._ser.close()
            gui.commsObject.quit()
    except Exception as e:
        print(e)
        if gui: 
            if gui.commsObject._ser:
                gui.commsObject._ser.close()
            gui.commsObject.quit()

    
