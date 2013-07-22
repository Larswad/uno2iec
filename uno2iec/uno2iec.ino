#include "log.h"
#include "iec_driver.h"
#include "interface.h"
#include "global_defines.h"

#ifdef USE_LED_DISPLAY
#include <max7219.h>

Max7219* pMax;
// Put ANYTHING in here you want to scroll as an initial text on your MAX7219!
static byte myText[] = "   WELCOME TO THE NEW PORT OF MMC2IEC ARDUINO BY LARS WADEFALK IN 2013...    ";
#endif

#define DEFAULT_BAUD_RATE 115200

// Pin 13 has a LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] = "CONNECT\r";
const char okString[] = "OK";

const byte INPIN = 11, LOADPIN = 13, CLOCKPIN = 12;

void waitForPeer();

// The global IEC handling singleton:
IEC iec(8);
Interface iface(iec);

static unsigned long lastMillis = 0;
static unsigned long now;

void setup()
{
	// Initialize serial and wait for port to open:
	Serial.begin(DEFAULT_BAUD_RATE);
	Serial.setTimeout(2000);
	// Now we're ready to wait for the PI to respond to our connection attempts.
	// initial connection handling.
	waitForPeer();

	// set all digital pins in a defined state.
	iec.init();

	lastMillis = millis();

#ifdef USE_LED_DISPLAY
	pMax = new Max7219(INPIN, LOADPIN, CLOCKPIN);
	pMax->resetScrollText(myText);
	iface.setMaxDisplay(pMax);
#endif
} // setup


void loop()
{
#ifdef DEBUGLINES
	// This is just for testing that the IEC lines are working properly (soldering, connections etc.)
	// Use either the first or second code line below, for outputs it will every second toggle the DATA and CLOCK.
	// If testing as input, it will log the states to the host every second.
	iec.testOUTPUTS();
	//iec.testINPUTS();
#else
	iface.handler();
#endif

#ifdef USE_LED_DISPLAY
	now = millis();
	if(now - lastMillis >= 50) {
		pMax->doScrollLeft();
		lastMillis = now;
	}
#endif
} // loop


// Establish connection to the media host.
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
	} // while(!connected)

	char dateTime[30];
	if(Serial.readBytesUntil('\r', dateTime, sizeof(dateTime))) {
		// TODO: parse date time and set clock.
	}
	registerFacilities();
	// We're in business.
	Log(Success, 'M', "CONNECTED TO PEER, READY FOR IEC COMMUNICATION WITH CBM.");
} // waitForPeer
