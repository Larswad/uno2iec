#ifndef IEC_DRIVER_H
#define IEC_DRIVER_H

#include <arduino.h>
#include "global_defines.h"

//
// Title        : MMC2IEC - IEC_driver module
// Author       : Lars Wadefalk
// Date         :
// Version      : n/a
// Target MCU   : AtMega328P at 16 MHz
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
		ATN_ERROR = 4       // A problem occoured, reset communication
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
		char str[ATN_CMD_MAX_LENGTH];
		byte strlen;
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

	// true == PULL == HIGH, false == RELEASE == LOW
	inline void writePIN(byte pinNumber, boolean state)
	{
		pinMode(pinNumber, state ? OUTPUT : INPUT);
		// Don't set the pin itself to HIGH, DDR drives the line from host.
//		digitalWrite(pinNumber, state ? HIGH : LOW);
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
};

#endif
