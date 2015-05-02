#include "iec_driver.h"
//#include "log.h"

using namespace CBM;

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


// The IEC bus pin configuration on the Arduino side
// NOTE: Deprecated: Only startup values used in ctor, this will be defined from host side.
#define DEFAULT_ATN_PIN 5
#define DEFAULT_DATA_PIN 3
#define DEFAULT_CLOCK_PIN 4
#define DEFAULT_SRQIN_PIN 6
#define DEFAULT_RESET_PIN 7

// See timeoutWait below.
#define TIMEOUT  65000

IEC::IEC(byte deviceNumber) :
	m_state(noFlags), m_deviceNumber(deviceNumber),
	m_atnPin(DEFAULT_ATN_PIN), m_dataPin(DEFAULT_DATA_PIN),
	m_clockPin(DEFAULT_CLOCK_PIN), /*m_srqInPin(DEFAULT_SRQIN_PIN),*/ m_resetPin(DEFAULT_RESET_PIN)
#ifdef DEBUGLINES
,m_lastMillis(0)
#endif
{
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
	while(not readATN());

	// Note: The while above is without timeout. If ATN is held low forever,
	//       the CBM is out in the woods and needs a reset anyways.

	return true;
} // timeoutWait


// IEC Recieve byte standard function
//
// Returns data recieved
// Might set flags in iec_state
//
// FIXME: m_iec might be better returning bool and returning read byte as reference in order to indicate any error.
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
		// FIXME: Make this like sd2iec and may not need a fixed delay here.

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
		// FIXME: Here check whether data pin goes low, if so end (enter cleanup)!

		writeCLOCK(true);
		// set data
		writeDATA((data bitand 1) ? false : true);

		delayMicroseconds(TIMING_BIT);
		writeCLOCK(false);
		delayMicroseconds(TIMING_BIT);

		data >>= 1;
	}

	writeCLOCK(true);
	writeDATA(false);

	// FIXME: Maybe make the following ending more like sd2iec instead.

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
		// Attention line is active, go to listener mode and get message. Being fast with the next two lines here is CRITICAL!
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

			// If the command is DATA and it is not to expect just a small command on the command channel, then
			// we're into something more heavy. Otherwise read it all out right here until UNLISTEN is received.
			if((c bitand 0xF0) == ATN_CODE_DATA and (c bitand 0xF) not_eq CMD_CHANNEL) {
				// A heapload of data might come now, too big for this context to handle so the caller handles this, we're done here.
				//Log(Information, FAC_IEC, "LDATA");
				ret = ATN_CMD_LISTEN;
			}
			else if(c not_eq ATN_CODE_UNLISTEN) {
				// Some other command. Record the cmd string until UNLISTEN is sent
				for(;;) {
					c = (ATNCommand)receiveByte();
					if(m_state bitand errorFlag)
						return ATN_ERROR;

					if((m_state bitand atnFlag) and (ATN_CODE_UNLISTEN == c))
						break;

					if(i >= ATN_CMD_MAX_LENGTH) {
						// Buffer is going to overflow, this is an error condition
						// FIXME: here we should propagate the error type being overflow so that reading error channel can give right code out.
						return ATN_ERROR;
					}
					cmd.str[i++] = c;
				}
				ret = ATN_CMD;
			}
		}
		else if (c == (ATN_CODE_TALK bitor m_deviceNumber)) {
			// Okay, we will talk soon, record cmd string while ATN is active
			// First byte is cmd code, that we CAN at least expect. All else depends on ATN.
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
						// FIXME: here we should propagate the error type being overflow so that reading error channel can give right code out.
						return ATN_ERROR;
					}
					cmd.str[i++] = c;
				}
			}

			// Now ATN has just been released, do bus turnaround
			if(not turnAround())
				return ATN_ERROR;

			// We have recieved a CMD and we should talk now:
			ret = ATN_CMD_TALK;

		}
		else {
			// Either the message is not for us or insignificant, like unlisten.
			delayMicroseconds(TIMING_ATN_DELAY);
			writeDATA(false);
			writeCLOCK(false);
			//			{
			//				char buffer[20];
			//				sprintf(buffer, "NOTUS: %u", c);
			//				Log(Information, FAC_IEC, buffer);
			//			}

			// Wait for ATN to release and quit
			while(not readATN());
			//Log(Information, FAC_IEC, "ATNREL");
		}
	}
	else {
		// No ATN, keep lines in a released state.
		writeDATA(false);
		writeCLOCK(false);
	}

	// some delay is required before more ATN business can take place.
	delayMicroseconds(TIMING_ATN_DELAY);

	cmd.strLen = i;
	return ret;
} // checkATN


boolean IEC::checkRESET()
{
	//	return false;
	//	// hmmm. Is this all todo?
	return readRESET();
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

#ifdef RESET_C64
	pinMode(m_resetPin, OUTPUT);
	digitalWrite(m_resetPin, false);	// only early C64's could be reset by a slave going high.
#endif

	// initial pin modes in GPIO.
	pinMode(m_atnPin, INPUT);
	pinMode(m_dataPin, INPUT);
	pinMode(m_clockPin, INPUT);
	pinMode(m_resetPin, INPUT);

#ifdef DEBUGLINES
	m_lastMillis = millis();
#endif

	// Set port low, we don't need internal pullup
	// and DDR input such that we release all signals
	//  IEC_PORT and_eq compl(IEC_BIT_ATN bitor IEC_BIT_CLOCK bitor IEC_BIT_DATA);
	//  IEC_DDR and_eq compl(IEC_BIT_ATN bitor IEC_BIT_CLOCK bitor IEC_BIT_DATA);

	m_state = noFlags;
	return true;
} // init

#ifdef DEBUGLINES
void IEC::testINPUTS()
{
	unsigned long now = millis();
	// show states every second.
	if(now - m_lastMillis >= 1000) {
		m_lastMillis = now;
		char buffer[80];
		sprintf(buffer, "Lines, ATN: %s CLOCK: %s DATA: %s",
						(readATN() ? "HIGH" : "LOW"), (readCLOCK() ? "HIGH" : "LOW"), (readDATA() ? "HIGH" : "LOW"));
		Log(Information, FAC_IEC, buffer);
	}
} // testINPUTS


void IEC::testOUTPUTS()
{
	static bool lowOrHigh = false;
	unsigned long now = millis();
	// switch states every second.
	if(now - m_lastMillis >= 1000) {
		m_lastMillis = now;
		char buffer[80];
		sprintf(buffer, "Lines: CLOCK: %s DATA: %s", (lowOrHigh ? "HIGH" : "LOW"), (lowOrHigh ? "HIGH" : "LOW"));
		Log(Information, FAC_IEC, buffer);
		writeCLOCK(lowOrHigh);
		writeDATA(lowOrHigh);
		lowOrHigh xor_eq true;
	}
} // testOUTPUTS
#endif


byte IEC::deviceNumber() const
{
	return m_deviceNumber;
} // deviceNumber


void IEC::setDeviceNumber(const byte deviceNumber)
{
	m_deviceNumber = deviceNumber;
} // setDeviceNumber


void IEC::setPins(byte atn, byte clock, byte data, byte srqIn, byte reset)
{
	m_atnPin = atn;
	m_clockPin = clock;
	m_dataPin = data;
	m_resetPin = reset;
	m_srqInPin = srqIn;
} // setPins


IEC::IECState IEC::state() const
{
	return static_cast<IECState>(m_state);
} // state
