#include "log.h"
#include "iec_driver.h"
#include "interface.h"

#define DEFAULT_BAUD_RATE 115200

// Pin 13 has a LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] = "CONNECT";
const char okString[] = "OK";

void waitForPeer();

// The global IEC handling singleton:
IEC iec(8);
Interface iface(iec);

void setup()
{
	// Initialize serial and wait for port to open:
	Serial.begin(DEFAULT_BAUD_RATE);

	// initialize the digital pin as an output.
	pinMode(ledPort, OUTPUT);

	// set all digital pins in a defined state.
	iec.init();

	// Now we're ready to wait for the PI to respond to our connection attempts.
	// initial connection handling.
	waitForPeer();
} // setup


void loop()
{
	// put your main code here, to run repeatedly:
	iface.handler();
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
