#include "global_defines.h"
#include "log.h"
#include "iec_driver.h"
#include "interface.h"
#include "stdio.h"

const int LED_PIN = D4; // IO2
const byte numBlinks = 4;
const char connectionString[] PROGMEM = "connect_arduino:%u\r\n";
const char okString[] PROGMEM = "OK>";

static void waitForPeer();

// The global IEC handling singleton:
static IEC iec(8);
static Interface iface(iec);

static ulong lastMillis = 0;

void setup()
{
  // Initialize serial and wait for port to open:
  COMPORT.begin(DEFAULT_BAUD_RATE);
  COMPORT.setTimeout(SERIAL_TIMEOUT_MSECS);

  // Initialize the digital LED pin as an output.
  pinMode(LED_PIN, OUTPUT);

  //COMPORT.printf("D0: %d, D5: %d, D6:%d, D7:%d, D8:%d\r\n", D0, D5, D6, D7, D8);

#ifndef DEBUGLINES
  // Now we're ready to wait for the PI to respond to our connection attempts.
  // initial connection handling.
  waitForPeer();
#endif

  // set all digital pins in a defined state.
  iec.init();

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
  if (IEC::ATN_RESET == iface.handler()) {

    // Wait for it to get out of reset.
    while (IEC::ATN_RESET == iface.handler());
  }
#endif
} // loop


// Establish initial connection to the media host.
static void waitForPeer()
{
  char tempBuffer[80];
  unsigned deviceNumber, atnPin, clockPin, dataPin, resetPin, srqInPin,
           hour, minute, second, year, month, day;

	// initialize the digital LED pin as an output.
	pinMode(LED_PIN, OUTPUT);
	
	boolean connected = false;
	while (not connected) {
		// empty all avail. in buffer.
		while (COMPORT.available())
			COMPORT.read();
		sprintf_P(tempBuffer, connectionString, CURRENT_UNO2IEC_PROTOCOL_VERSION);
		//strcpy_P(tempBuffer, connectionString);
		COMPORT.print(tempBuffer);
		COMPORT.flush();
		// Indicate to user we are waiting for connection.
		for (byte i = 0; i < numBlinks; ++i) {
			digitalWrite(LED_PIN, LOW);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
			digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
		}
		strcpy_P(tempBuffer, okString);
		connected = COMPORT.find(tempBuffer);
	} // while(not connected)

	// Now read the whole configuration string from host, ends with CR. If we don't get THIS string, we're in a bad state.
	// OK>8|16|14|12|15|2|2020-04-08.14:56:16
	if (COMPORT.readBytesUntil('\r', tempBuffer, sizeof(tempBuffer))) {
		sscanf(tempBuffer, "%u|%u|%u|%u|%u|%u|%u-%u-%u.%u:%u:%u",
			&deviceNumber, &atnPin, &clockPin, &dataPin, &resetPin, &srqInPin,
			&year, &month, &day, &hour, &minute, &second);
/*
	    deviceNumber = 8;
	    atnPin = D0;
	    clockPin = D5;
	    dataPin = D6;
	    srqInPin = D3;  
	    resetPin = D8;

	    year = 2020;
	    month = 4;
	    day = 2;
	    hour = 0;
	    minute = 0;
	    second = 0;
*/
		// we got the config from the HOST.
		iec.setDeviceNumber(deviceNumber);
		iec.setPins(atnPin, clockPin, dataPin, srqInPin, resetPin);
		iface.setDateTime(year, month, day, hour, minute, second);
	}
	registerFacilities();

	// We're in business.
	sprintf_P(tempBuffer, "CONNECTED, READY FOR IEC DATA WITH CBM AS DEV %u.\r\n", deviceNumber);
	Log(Success, 'M', tempBuffer);
	COMPORT.flush();
	sprintf_P(tempBuffer, "IEC pins: ATN:%d CLK:%d DATA:%d RST:%d SRQIN:%d LED:%d\r\n", atnPin, clockPin, dataPin,
		resetPin, srqInPin, LED_PIN);
	Log(Information, 'M', tempBuffer);
	sprintf_P(tempBuffer, "Arduino time set to: %04d-%02d-%02d.%02d:%02d:%02d\r\n", year, month, day, hour, minute, second);
	Log(Information, 'M', tempBuffer);
} // waitForPeer
