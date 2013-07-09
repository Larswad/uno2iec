#include "log.h"
#include "iec_driver.h"
#include "interface.h"
#include <max7219.h>

#define DEFAULT_BAUD_RATE 115200

// Pin 13 has a LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] = "CONNECT\r";
const char okString[] = "OK";

const byte INPIN = 11, LOADPIN = 13, CLOCKPIN = 12;

static byte myText[] = "   HEJSAN ALLA GLADA BARNEN, DETTA E LARS MED SIN NYA MAX7219 SCROLL...    ";

void waitForPeer();

// The global IEC handling singleton:
IEC iec(8);
Interface iface(iec);
Max7219* pMax;
unsigned long lastMillis = 0;

void setup()
{
	// Initialize serial and wait for port to open:
	Serial.begin(DEFAULT_BAUD_RATE);

	// Now we're ready to wait for the PI to respond to our connection attempts.
	// initial connection handling.
	waitForPeer();

	// set all digital pins in a defined state.
	iec.init();

	lastMillis = millis();

	pMax = new Max7219(INPIN, LOADPIN, CLOCKPIN);
	pMax->resetScrollText(myText);
} // setup


void loop()
{
	iface.handler();

	unsigned long now = millis();
	if(now - lastMillis >= 50) {
		pMax->doScrollLeft();
		lastMillis = now;
	}
} // loop


void waitForPeer()
{
	// initialize the digital pin as an output.
	pinMode(ledPort, OUTPUT);

	boolean connected = false;
	Serial.setTimeout(1000);
	while(!connected) {
		// empty all avail. in buffer.
		while(Serial.available())
			Serial.read();
		Serial.print(connectionString);
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
