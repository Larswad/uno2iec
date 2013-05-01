// Peer commands:
// Send debug string
// D<severityId, facilityId, debug string><CR>
// Register facility with abbreviating character and corresponding string to expand to on peer side.
// !<abbreviationChar, string><CR>
// Init file from LOAD"",8 command.
// 'I'
// Read byte(s) from current file of current (last selected) file system type. Current byte read/write byte number size determines number of bytes in sequence.
// The host returns 'B' followed by the byte(s) being read, or answers with 'E' if end of file has been reached.
// 'R' 
// Write byte(s) to current file of current (last selected) file system type. Current byte read/write byte number size determines number of bytes in sequence.
// 'W<byte..n>' 
#include "log.h"
#include "iec_driver.h"

#define BAUD_RATE 115200
 // Pin 13 has an LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] = "CONNECT";
const char okString[] = "OK";

void waitForPeer();
IEC iec(8);

void setup()
{

  // Initialize serial and wait for port to open:
  Serial.begin(BAUD_RATE); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  // initialize the digital pin as an output.
  pinMode(ledPort, OUTPUT);     
  
  waitForPeer();

  iec.init();
} // setup


void loop()
{
// put your main code here, to run repeatedly: 
//  static Interface iface(iec);
//  iface.handler();
} // loop


void waitForPeer()
{
  boolean connected = false;
  Serial.setTimeout(1000);
  while(!connected) {
    // empty all avail. in buffer.
    while(Serial.available())
      Serial.read();
    Serial.println(connectionString); 
    Serial.flush();
    for(int i = 0; i < numBlinks; ++i) {
      digitalWrite(ledPort, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(500 / numBlinks / 2);               // wait for a second
      digitalWrite(ledPort, LOW);   // turn the LED on (HIGH is the voltage level)      
      delay(500 / numBlinks / 2);               // wait for a second
    }
    connected = Serial.find((char*)okString);
  }
  registerFacilities();
  Log(Success, 'M', "CONNECTED TO PEER.");
  Log(Information, 'M', "READY FOR IEC COMMUNICATION.");
} // waitForPeer

