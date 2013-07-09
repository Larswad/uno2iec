//
// Title        : MMC2IEC - IEC_driver module
// Author       : Lars Pontoppidan
// Version      : 0.7
// Target MCU   : AtMega32(L) at 8 MHz
//
// CREDITS:
// This code is inspired from Jan Derogee's 1541-III project for PIC:
// http://jderogee.tripod.com/
// Although this code is a complete reimplementation.
//
// DESCRIPTION:
// The IEC_driver module implements lowlevel IEC communication such as recieving
// commands, sending and recieving bytes.
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//


#include "iec_driver.h"

/******************************************************************************
 *                                                                             *
 *                                TIMING SETUP                                 *
 *                                                                             *
 ******************************************************************************/


// IEC protocol timing consts:
#define TIMING_BIT          70  // bit clock hi/lo time     (us)
#define TIMING_NO_EOI       20  // delay before bits        (us)
#define TIMING_EOI_WAIT     200 // delay to signal EOI      (us)
#define TIMING_EOI_THRESH   20  // threshold for EOI detect (*10 us approx)
#define TIMING_STABLE_WAIT  20  // line stabilization       (us)
#define TIMING_ATN_PREDELAY 50  // delay required in atn    (us)
#define TIMING_ATN_DELAY    100 // delay required after atn (us)
#define TIMING_FNF_DELAY    100 // delay after fnf?         (us)

// Version 0.5 equivalent timings: 70, 5, 200, 20, 20, 50, 100, 100

// TIMING TESTING:
//
// The consts: 70,20,200,20,20,50,100,100 has been tested without debug print
// to work stable on my (Larsp)'s DTV at 700000 < F_CPU < 9000000
// using a 32 MB MMC card
//

/******************************************************************************
 *                                                                             *
 *                               IMPLEMENTATION                                *
 *                                                                             *
 *                               Local functions                               *
 *                                                                             *
 ******************************************************************************/

// IEC signal macros:

// Releasing a signal is done by setting DDR input
// Pull down is done by setting DDR output. (Port value is set low by init)
//#define IEC_ATN_REL()    IEC_DDR and_eq compl IEC_BIT_ATN
//#define IEC_ATN_PULL()   IEC_DDR or_eq IEC_BIT_ATN
//#define IEC_CLOCK_REL()  IEC_DDR and_eq compl IEC_BIT_CLOCK
//#define IEC_CLOCK_PULL() IEC_DDR or_eq IEC_BIT_CLOCK
//#define IEC_DATA_REL()   IEC_DDR and_eq compl IEC_BIT_DATA
//#define IEC_DATA_PULL()  IEC_DDR or_eq IEC_BIT_DATA

//#define IEC_ATN    (IEC_PIN bitand IEC_BIT_ATN)
//#define IEC_CLOCK  (IEC_PIN bitand IEC_BIT_CLOCK)
//#define IEC_DATA   (IEC_PIN bitand IEC_BIT_DATA)

#define TIMEOUT  65000

IEC::IEC(byte deviceNumber) :
	m_state(noFlags), m_deviceNumber(deviceNumber),
	m_atnPin(10), m_dataPin(9), m_clockPin(8)
{
	// wiringPiPin 3 == BroadcomPin 22
	// wiringPiPin 4 == BroadcomPin 23
	// wiringPiPin 5 == BroadcomPin 24
	//  init();
} // ctor


byte IEC::timeoutWait(byte waitBit, boolean whileHigh)
{
	word t = 0;
	boolean c;

	while(t < TIMEOUT) {
		// Check the waiting condition:
		c = readPIN(waitBit);

		if(whileHigh)
			c = not c;

		if(c)
			return false;

		delayMicroseconds(2); // The aim is to make the loop at least 3 us
		t++;
	}

	// If down here, we have had a timeout.
	// Release lines and go to inactive state with error flag
	writeCLOCK(false);
	writeDATA(false);

	m_state = errorFlag;

	// Wait for ATN release, problem might have occured during attention
	while(!readATN());

	// Note: The while above is without timeout. If ATN is held low forever,
	//       the C64 is out in the woods and needs a reset anyways.

	return true;
} // timeoutWait


// IEC Recieve byte standard function
//
// Returns data recieved
// Might set flags in iec_state
//
// TODO: m_iec might be better returning bool and returning read byte as reference in order to indicate any error.
byte IEC::receiveByte(void)
{
	m_state = noFlags;

	// Wait for talker ready
	if(timeoutWait(m_clockPin, false))
		return 0;

	// Say we're ready
	writeDATA(false);

	// Record how long CLOCK is high, more than 200 us means EOI
	byte n = 0;
	while(readCLOCK() and (n < 20)) {
		delayMicroseconds(10);  // this loop should cycle in about 10 us...
		n++;
	}

	if(n >= TIMING_EOI_THRESH) {
		// EOI intermission
		m_state or_eq eoiFlag;

		// Acknowledge by pull down data more than 60 us
		writeDATA(true);
		delayMicroseconds(TIMING_BIT);
		writeDATA(false);

		// but still wait for clk
		if(timeoutWait(m_clockPin, true))
			return 0;
	}

	// Sample ATN
	if(false == readATN())
		m_state or_eq atnFlag;

	byte data = 0;
	// Get the bits, sampling on clock rising edge:
	for(n = 0; n < 8; n++) {
		data >>= 1;
		if(timeoutWait(m_clockPin, false))
			return 0;
		data or_eq (readDATA() ? (1 << 7) : 0);
		if(timeoutWait(m_clockPin, true))
			return 0;
	}

	// Signal we accepted data:
	writeDATA(true);

	return data;
} // receiveByte


// IEC Send byte standard function
//
// Sends the byte and can signal EOI
//
boolean IEC::sendByte(byte data, boolean signalEOI)
{
	// Listener must have accepted previous data
	if(timeoutWait(m_dataPin, true))
		return false;

	// Say we're ready
	writeCLOCK(false);

	// Wait for listener to be ready
	if(timeoutWait(m_dataPin, false))
		return false;

	if(signalEOI) {
		// Signal eoi by waiting 200 us
		delayMicroseconds(TIMING_EOI_WAIT);

		// get eoi acknowledge:
		if(timeoutWait(m_dataPin, true))
			return false;

		if(timeoutWait(m_dataPin, false))
			return false;
	}

	delayMicroseconds(TIMING_NO_EOI);

	// Send bits
	for(byte n = 0; n < 8; n++) {
		writeCLOCK(true);
		// set data
		if(data bitand 1)
			writeDATA(false);
		else
			writeDATA(true);

		delayMicroseconds(TIMING_BIT);
		writeCLOCK(false);
		delayMicroseconds(TIMING_BIT);

		data >>= 1;
	}

	writeCLOCK(true);
	writeDATA(false);

	// Line stabilization delay
	delayMicroseconds(TIMING_STABLE_WAIT);

	// Wait for listener to accept data
	if(timeoutWait(m_dataPin, true))
		return false;

	return true;
} // sendByte


// IEC turnaround
boolean IEC::turnAround(void)
{
	// Wait until clock is released
	if(timeoutWait(m_clockPin, false))
		return false;

	writeDATA(false);
	delayMicroseconds(TIMING_BIT);
	writeCLOCK(true);
	delayMicroseconds(TIMING_BIT);

	return true;
} // turnAround


// this routine will set the direction on the bus back to normal
// (the way it was when the computer was switched on)
boolean IEC::undoTurnAround(void)
{
	writeDATA(true);
	delayMicroseconds(TIMING_BIT);
	writeCLOCK(false);
	delayMicroseconds(TIMING_BIT);

	// wait until the computer releases the clock line
	if(timeoutWait(m_clockPin, true))
		return false;

	return true;
} // undoTurnAround


/******************************************************************************
 *                                                                             *
 *                               Public functions                              *
 *                                                                             *
 ******************************************************************************/

// This function checks and deals with atn signal commands
//
// If a command is recieved, the cmd-string is saved in cmd. Only commands
// for *this* device are dealt with.
//
// Return value, see IEC::ATNCheck definition.
IEC::ATNCheck IEC::checkATN(ATNCmd& cmd)
{
	ATNCheck ret = ATN_IDLE;
	byte i = 0;

	if(not readATN()) {
		// Attention line is active, go to listener mode and get message

		writeDATA(true);
		writeCLOCK(false);

		delayMicroseconds(TIMING_ATN_PREDELAY);

		// Get first ATN byte, it is either LISTEN or TALK
		ATNCommand c = (ATNCommand)receiveByte();
		if(m_state bitand errorFlag)
			return ATN_ERROR;

		if(c == (ATN_CODE_LISTEN bitor m_deviceNumber)) {
			// Okay, we will listen.

			// Get the first cmd byte, the cmd code
			c = (ATNCommand)receiveByte();
			if (m_state bitand errorFlag)
				return ATN_ERROR;

			cmd.code = c;

			if((c bitand 0xF0) == ATN_CODE_DATA) {
				// A heapload of data might come now, client handles this
				ret = ATN_CMD_LISTEN;
			}
			else if(c not_eq ATN_CODE_UNLISTEN) {
				// Some other command. Record the cmd string until UNLISTEN is send
				for(;;) {
					c = (ATNCommand)receiveByte();
					if(m_state bitand errorFlag)
						return ATN_ERROR;

					if((m_state bitand atnFlag) and (c == ATN_CODE_UNLISTEN))
						break;

					if(i >= ATN_CMD_MAX_LENGTH) {
						// Buffer is going to overflow, this is an error condition
						return ATN_ERROR;
					}
					cmd.str[i++] = c;
				}
				ret = ATN_CMD;
			}
		}
		else if (c == (ATN_CODE_TALK bitor m_deviceNumber)) {
			// Okay, we will talk soon, record cmd string while ATN is active

			// First byte is cmd code
			c = (ATNCommand)receiveByte();
			if(m_state bitand errorFlag)
				return ATN_ERROR;
			cmd.code = c;

			while(not readATN()) {
				if(readCLOCK()) {
					c = (ATNCommand)receiveByte();
					if(m_state bitand errorFlag)
						return ATN_ERROR;

					if(i >= ATN_CMD_MAX_LENGTH) {
						// Buffer is going to overflow, this is an error condition
						return ATN_ERROR;
					}
					cmd.str[i++] = c;
				}
			}

			// Now ATN has just been released, do bus turnaround
			if(!turnAround())
				return ATN_ERROR;

			// We have recieved a CMD and we should talk now:
			ret = ATN_CMD_TALK;

		}
		else {
			// Either the message is not for us or insignificant, like unlisten.
			delayMicroseconds(TIMING_ATN_DELAY);
			writeDATA(false);
			writeCLOCK(false);

			// Wait for ATN to release and quit
			while(not readATN());
		}
	}
	else {
		// No ATN, release lines
		writeDATA(false);
		writeCLOCK(false);
	}

	// some delay is required before more ATN business
	delayMicroseconds(TIMING_ATN_DELAY);

	cmd.strlen = i;
	return ret;
} // checkATN


// IEC_receive receives a byte
//
byte IEC::receive()
{
	return receiveByte();
} // receive


// IEC_send sends a byte
//
boolean IEC::send(byte data)
{
	return sendByte(data, false);
} // send


// Same as IEC_send, but indicating that this is the last byte.
//
boolean IEC::sendEOI(byte data)
{
	if(sendByte(data, true)) {
		// As we have just send last byte, turn bus back around
		if(undoTurnAround())
			return true;
	}

	return false;
} // sendEOI


// A special send command that informs file not found condition
//
boolean IEC::sendFNF()
{
	// Message file not found by just releasing lines
	writeDATA(false);
	writeCLOCK(false);

	// Hold back a little...
	delayMicroseconds(TIMING_FNF_DELAY);

	return true;
} // sendFNF


// Set all IEC_signal lines in the correct mode
//
boolean IEC::init()
{
	// make sure the output states are initially LOW.
	pinMode(m_atnPin, OUTPUT);
	pinMode(m_dataPin, OUTPUT);
	pinMode(m_clockPin, OUTPUT);
	digitalWrite(m_atnPin, false);
	digitalWrite(m_dataPin, false);
	digitalWrite(m_clockPin, false);

	// initial pin modes in GPIO.
	pinMode(m_atnPin, INPUT);
	pinMode(m_dataPin, INPUT);
	pinMode(m_clockPin, INPUT);

	// Set port low, we don't need internal pullup
	// and DDR input such that we release all signals
	//  IEC_PORT and_eq compl(IEC_BIT_ATN bitor IEC_BIT_CLOCK bitor IEC_BIT_DATA);
	//  IEC_DDR and_eq compl(IEC_BIT_ATN bitor IEC_BIT_CLOCK bitor IEC_BIT_DATA);

	m_state = noFlags;
	return true;
} // init


byte IEC::deviceNumber() const
{
	return m_deviceNumber;
} // deviceNumber


void IEC::setDeviceNumber(const byte deviceNumber)
{
	m_deviceNumber = deviceNumber;
} // setDeviceNumber


IEC::IECState IEC::state() const
{
	return static_cast<IECState>(m_state);
} // state

