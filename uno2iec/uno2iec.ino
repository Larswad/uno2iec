#include "global_defines.h"
#include "log.h"
#include "iec_driver.h"
#include "interface.h"

#ifdef USE_LED_DISPLAY
#include <max7219.h>

static Max7219* pMax;
// Put ANYTHING cool in here you want to scroll as an initial text on your MAX7219!
static const byte PROGMEM myText[] = "   WELCOME TO UNO2IEC, 1541 ON ARDUINO BY LARS WADEFALK IN 2015...    ";
// Note: This is the pin configuration for the MAX7219 display, if used (not IEC related).
static const byte PROGMEM MAX_INPIN = 11, MAX_LOADPIN = 13, MAX_CLOCKPIN = 12;
#endif

// Pin 13 has a LED connected on most Arduino boards.
const byte ledPort = 13;
const byte numBlinks = 4;
const char connectionString[] PROGMEM = "connect_arduino:%u\r";
const char okString[] PROGMEM = "OK>";

static void waitForPeer();

// The global IEC handling singleton:
static IEC iec(8);
static Interface iface(iec);

static ulong lastMillis = 0;

void setup()
{
#ifdef CONFIG_HC06_BLUETOOTH
	// Note: This ONLY works if device is not paired and connected. So be sure the host is not connected to the HC06
	// when configuring the baud rate. Also important to use the latest configured baud rate before running commands below.
	// When device is shipped it is set to 9600. When compiled and uploaded this code, power off the BT device completely
	// so that the configuring is done from power-up state.
//	COMPORT.begin(115200);

	// Use this to change the bluetooth name of the device.
//	COMPORT.write("AT+NAMEARDUINO");
//	delay(1000);
	// Use this to set new pin code, default is 1234.
	//COMPORT.write("AT+PIN1234");
	//delay(1000);
	// Then the baudrate which is a value between 1 and C. Recommended settings are 38400 or 57600.
	// Here is the list: 1:1200, 2:2400, 3:4800, 4:9600, 5:19200, 6:38400, 7:57600, 8:115200, 9:230400, A:460800, B:921600, C:1382400
//	COMPORT.write("AT+BAUD7");
//	delay(1000);
//	COMPORT.end();
	//for(;;);
#endif

	// Initialize serial and wait for port to open:
	COMPORT.begin(DEFAULT_BAUD_RATE);
	COMPORT.setTimeout(SERIAL_TIMEOUT_MSECS);

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
	pMax->resetScrollText_p(myText);
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
		pMax->resetScrollText_p(myText);
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
	unsigned deviceNumber, atnPin, clockPin, dataPin, resetPin, srqInPin,
			hour, minute, second, year, month, day;

	// initialize the digital LED pin as an output.
	pinMode(ledPort, OUTPUT);

	boolean connected = false;
	while(not connected) {
		// empty all avail. in buffer.
		while(COMPORT.available())
			COMPORT.read();
		sprintf_P(tempBuffer, connectionString, CURRENT_UNO2IEC_PROTOCOL_VERSION);
		//strcpy_P(tempBuffer, connectionString);
		COMPORT.print(tempBuffer);
		COMPORT.flush();
		// Indicate to user we are waiting for connection.
		for(byte i = 0; i < numBlinks; ++i) {
			digitalWrite(ledPort, HIGH);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
			digitalWrite(ledPort, LOW);   // turn the LED on (HIGH is the voltage level)
			delay(500 / numBlinks / 2);               // wait for a second
		}
		strcpy_P(tempBuffer, okString);
		connected = COMPORT.find(tempBuffer);
	} // while(not connected)

	// Now read the whole configuration string from host, ends with CR. If we don't get THIS string, we're in a bad state.
	if(COMPORT.readBytesUntil('\r', tempBuffer, sizeof(tempBuffer))) {
		sscanf_P(tempBuffer, (PGM_P)F("%u|%u|%u|%u|%u|%u|%u-%u-%u.%u:%u:%u"),
						 &deviceNumber, &atnPin, &clockPin, &dataPin, &resetPin, &srqInPin,
						 &year, &month, &day, &hour, &minute, &second);

		// we got the config from the HOST.
		iec.setDeviceNumber(deviceNumber);
		iec.setPins(atnPin, clockPin, dataPin, srqInPin, resetPin);
		iface.setDateTime(year, month, day, hour, minute, second);
	}
	registerFacilities();

	// We're in business.
	sprintf_P(tempBuffer, (PGM_P)F("CONNECTED, READY FOR IEC DATA WITH CBM AS DEV %u."), deviceNumber);
	Log(Success, 'M', tempBuffer);
	COMPORT.flush();
	sprintf_P(tempBuffer, (PGM_P)F("IEC pins: ATN:%u CLK:%u DATA:%u RST:%u SRQIN:%u"), atnPin, clockPin, dataPin,
						resetPin, srqInPin);
	Log(Information, 'M', tempBuffer);
	sprintf_P(tempBuffer, (PGM_P)F("Arduino time set to: %04u-%02u-%02u.%02u:%02u:%02u"), year, month, day, hour, minute, second);
	Log(Information, 'M', tempBuffer);

    Log(Information, 'M', "WILLY WAS HERE");
} // waitForPeer
