#ifndef IEC_DRIVER_H
#define IEC_DRIVER_H

#include "util.hpp"
//#include <wiringPi.h>


//
// Title        : MMC2IEC - IEC_driver module
// Author       : Lars Pontoppidan
// Date         :
// Version      : 0.5
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
	enum { ATN_CMD_MAX_LENGTH = 40 };
	// default device number listening unless explicitly stated in ctor:
	enum { DEFAULT_IEC_DEVICE = 8 };

	typedef struct {
		ATNCommand code;
		char str[ATN_CMD_MAX_LENGTH];
		uchar strlen;
	} ATNCmd;


	IEC(uchar deviceNumber = DEFAULT_IEC_DEVICE);
	virtual ~IEC() {}

	// Initialise iec driver
	//
	bool init();

	// Checks if CBM is sending an attention message. If this is the case,
	// the message is recieved and stored in atn_cmd.
	//
	ATNCheck checkATN(ATNCmd& cmd);

	// Sends a byte. The communication must be in the correct state: a load command
	// must just have been recieved. If something is not OK, FALSE is returned.
	//
	bool send(uchar data);

	// Same as IEC_send, but indicating that this is the last byte.
	//
	bool sendEOI(uchar data);

	// A special send command that informs file not found condition
	//
	bool sendFNF();

	// Recieves a byte
	//
	uchar receive();

	uchar deviceNumber() const;
	void setDeviceNumber(const uchar deviceNumber);
	IECState state() const;

private:
	uchar timeoutWait(uchar waitBit, bool whileHigh);
	uchar receiveByte(void);
	bool sendByte(uchar data, bool signalEOI);
	bool turnAround(void);
	bool undoTurnAround(void);

	// false = LOW, true == HIGH
	inline bool readPIN(uchar pinNumber)
	{
		Q_UNUSED(pinNumber);
		return false;
//    pinMode(pinNumber, INPUT);
//    return digitalRead(pinNumber) ? true : false;
	}

	inline bool readATN()
	{
		return readPIN(m_atnPin);
	}

	inline bool readDATA()
	{
		return readPIN(m_dataPin);
	}

	inline bool readCLOCK()
	{
		return readPIN(m_clockPin);
	}

	// true == PULL == HIGH, false == RELEASE == LOW
	inline void writePIN(uchar pinNumber, bool state)
	{
		Q_UNUSED(pinNumber);
		Q_UNUSED(state);
//    pinMode(pinNumber, OUTPUT);
//    digitalWrite(pinNumber, state ? HIGH : LOW);
	}

	inline void writeATN(bool state)
	{
		writePIN(m_atnPin, state);
	}

	inline void writeDATA(bool state)
	{
		writePIN(m_dataPin, state);
	}

	inline void writeCLOCK(bool state)
	{
		writePIN(m_clockPin, state);
	}

	// communication must be reset
	uchar m_state;
	uchar m_deviceNumber;

	int m_atnPin;
	int m_dataPin;
	int m_clockPin;
};

#endif
