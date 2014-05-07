#ifndef IEC_DRIVER_H
#define IEC_DRIVER_H

#include <Arduino.h>
#include "global_defines.h"
#include "cbmdefines.h"


class IEC
{
public:

	enum IECState {
		noFlags   = 0,
		eoiFlag   = (1 << 0),   // might be set by Iec_receive
		atnFlag   = (1 << 1),   // might be set by Iec_receive
		errorFlag = (1 << 2)  // If this flag is set, something went wrong and
	};

	// Return values for checkATN:
	enum ATNCheck {
		ATN_IDLE = 0,       // Nothing recieved of our concern
		ATN_CMD = 1,        // A command is recieved
		ATN_CMD_LISTEN = 2, // A command is recieved and data is coming to us
		ATN_CMD_TALK = 3,   // A command is recieved and we must talk now
		ATN_ERROR = 4,      // A problem occoured, reset communication
		ATN_RESET = 5				// The IEC bus is in a reset state (RESET line).
	};

	// IEC ATN commands:
	enum ATNCommand {
		ATN_CODE_LISTEN = 0x20,
		ATN_CODE_TALK = 0x40,
		ATN_CODE_DATA = 0x60,
		ATN_CODE_CLOSE = 0xE0,
		ATN_CODE_OPEN = 0xF0,
		ATN_CODE_UNLISTEN = 0x3F,
		ATN_CODE_UNTALK = 0x5F
	};

	// ATN command struct maximum command length:
	enum {
		ATN_CMD_MAX_LENGTH = 40
	};
	// default device number listening unless explicitly stated in ctor:
	enum {
		DEFAULT_IEC_DEVICE = 8
	};

	typedef struct _tagATNCMD {
		byte code;
		byte str[ATN_CMD_MAX_LENGTH];
		byte strLen;
	} ATNCmd;

	IEC(byte deviceNumber = DEFAULT_IEC_DEVICE);
	~IEC()
	{ }

	// Initialise iec driver
	//
	boolean init();

	// Checks if CBM is sending an attention message. If this is the case,
	// the message is recieved and stored in atn_cmd.
	//
	ATNCheck checkATN(ATNCmd& cmd);

	// Checks if CBM is sending a reset (setting the RESET line high). This is typicall
	// when the CBM is reset itself. In this case, we are supposed to reset all states to initial.
	boolean checkRESET();

	// Sends a byte. The communication must be in the correct state: a load command
	// must just have been recieved. If something is not OK, FALSE is returned.
	//
	boolean send(byte data);

	// Same as IEC_send, but indicating that this is the last byte.
	//
	boolean sendEOI(byte data);

	// A special send command that informs file not found condition
	//
	boolean sendFNF();

	// Recieves a byte
	//
	byte receive();

	byte deviceNumber() const;
	void setDeviceNumber(const byte deviceNumber);
	void setPins(byte atn, byte clock, byte data, byte srqIn, byte reset);
	IECState state() const;

#ifdef DEBUGLINES
	unsigned long m_lastMillis;
	void testINPUTS();
	void testOUTPUTS();
#endif

private:
	byte timeoutWait(byte waitBit, boolean whileHigh);
	byte receiveByte(void);
	boolean sendByte(byte data, boolean signalEOI);
	boolean turnAround(void);
	boolean undoTurnAround(void);

	// false = LOW, true == HIGH
	inline boolean readPIN(byte pinNumber)
	{
		// To be able to read line we must be set to input, not driving.
		pinMode(pinNumber, INPUT);
		return digitalRead(pinNumber) ? true : false;
	}

	inline boolean readATN()
	{
		return readPIN(m_atnPin);
	}

	inline boolean readDATA()
	{
		return readPIN(m_dataPin);
	}

	inline boolean readCLOCK()
	{
		return readPIN(m_clockPin);
	}

	inline boolean readRESET()
	{
		return !readPIN(m_resetPin);
	}

//	inline boolean readSRQIN()
//	{
//		return readPIN(m_srqInPin);
//	}

	// true == PULL == HIGH, false == RELEASE == LOW
	inline void writePIN(byte pinNumber, boolean state)
	{
		pinMode(pinNumber, state ? OUTPUT : INPUT);
		digitalWrite(pinNumber, state ? LOW : HIGH);
	}

	inline void writeATN(boolean state)
	{
		writePIN(m_atnPin, state);
	}

	inline void writeDATA(boolean state)
	{
		writePIN(m_dataPin, state);
	}

	inline void writeCLOCK(boolean state)
	{
		writePIN(m_clockPin, state);
	}

	// communication must be reset
	byte m_state;
	byte m_deviceNumber;

	byte m_atnPin;
	byte m_dataPin;
	byte m_clockPin;
	byte m_srqInPin;
	byte m_resetPin;
};

#endif
