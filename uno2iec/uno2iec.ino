#include "global_defines.h"
#include "log.h"
#include "iec_driver.h"
#include "interface.h"

#ifdef USE_LED_DISPLAY
#include <max7219.h>

static Max7219* pMax;
// Put ANYTHING cool in here you want to scroll as an initial text on your MAX7219!
static const byte myText[] = "   WELCOME TO THE NEW PORT OF MMC2IEC ARDUINO BY LARS WADEFALK IN 2013...    ";
// Note: This is the pin configuration for the MAX7219 display, if used (not IEC related).
static const byte PROGMEM MAX_INPIN = 11, MAX_LOADPIN = 13, MAX_CLOCKPIN = 12;
#endif

// Pin 13 has a LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] = "CONNECT\r";
const char okString[] = "OK>";


static void waitForPeer();

// The global IEC handling singleton:
static IEC iec(8);
static Interface iface(iec);

static ulong lastMillis = 0;

void setup()
{
	// Initialize serial and wait for port to open:
	Serial.begin(DEFAULT_BAUD_RATE);
	Serial.setTimeout(SERIAL_TIMEOUT_MSECS);

#ifdef USE_LED_DISPLAY
	pMax = new Max7219(MAX_INPIN, MAX_LOADPIN, MAX_CLOCKPIN);
	// indicate on display that we are waiting for connection.
	pMax->setToCharacter('C');
	iface.setMaxDisplay(pMax);
#endif

	// Now we're ready to wait for the PI to respond to our connection attempts.
	// initial connection handling.
	waitForPeer();

	// set all digital pins in a defined state.
	iec.init();

#ifdef USE_LED_DISPLAY
	pMax->resetScrollText(myText);
#endif

	lastMillis = millis();
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
	if(IEC::ATN_RESET == iface.handler()) {

#ifdef USE_LED_DISPLAY
		pMax->resetScrollText(myText);
		// Indicate that IEC is in reset state.
		pMax->setToCharacter('R');
#endif

		// Wait for it to get out of reset.
		while(IEC::ATN_RESET == iface.handler());
	}
#endif

#ifdef USE_LED_DISPLAY
	ulong now = millis();
	if(now - lastMillis >= 50) {
		pMax->doScrollLeft();
		lastMillis = now;
	}
#endif
} // loop


// Establish initial connection to the media host.
static void waitForPeer()
{
	char tempBuffer[80];
	unsigned deviceNumber, atnPin, clockPin, dataPin, resetPin;

	// initialize the digital LED pin as an output.
	pinMode(ledPort, OUTPUT);

	boolean connected = false;
	while(!connected) {
		// empty all avail. in buffer.
		while(Serial.available())
			Serial.read();
		Serial.print(connectionString);
		Serial.flush();
		// Indicate to user we are waiting for connection.
		for(byte i = 0; i < numBlinks; ++i) {
			digitalWrite(ledPort, HIGH);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
			digitalWrite(ledPort, LOW);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
		}
		connected = Serial.find((char*)okString);
	} // while(!connected)

	// Now read whole configuration string from host, ends with CR. If we don't get THIS string, we're in a bad state.
	if(Serial.readBytesUntil('\r', tempBuffer, sizeof(tempBuffer))) {
		sscanf(tempBuffer, "%u|%u|%u|%u|%u", &deviceNumber, &atnPin, &clockPin, &dataPin, &resetPin);
		// we got the config from the HOST.
		iec.setDeviceNumber(deviceNumber);
		iec.setPins(atnPin, clockPin, dataPin, resetPin);
	}
	registerFacilities();

	// We're in business.
	sprintf(tempBuffer, "CONNECTED, READY FOR IEC DATA WITH CBM AS DEV %u.", deviceNumber);
	Log(Success, 'M', tempBuffer);
	sprintf(tempBuffer, "IEC pins: ATN:%u CLK:%u DATA:%u RST:%u", atnPin, clockPin, dataPin, resetPin);
	Log(Information, 'M', tempBuffer);
} // waitForPeer
