#ifndef INTERFACE_H
#define INTERFACE_H

#include "IEC_driver.h"

typedef enum {
	ErrOK,
	ErrReadError,
	ErrWriteProtectOn,
	ErrSyntaxError,
	ErrFileNotFound,
	ErrFileExists,
	ErrIntro,
	ErrDriveNotReady,
	ErrSerialComm,		// something went sideways with serial communication to the file server.
	ErrCount
} IOErrorMessage;


enum  {
	IS_FAIL = 0xFF, // IFail: SD card or fat not ok
	IS_NATIVE = 0,			// Regular file system file state
	// states 1 -> NumInterfaceStates are also valid, representing what's open
	IS_D64 = 1,
	IS_T64 = 2,
	IS_M2I = 3,
	IS_PRG = 4,
	NumInterfaceStates
};

enum OpenState {
	O_NOTHING,			// Nothing to send / File not found error
	O_INFO,					// User issued a reload sd card
	O_FILE,					// A program file is opened
	O_DIR,					// A listing is requested
	O_FILE_ERR,			// Incorrect file format opened
	O_SAVE_REPLACE	// Save-with-replace is requested
};

// The base pointer of basic.
const word C64_BASIC_START = 0x0801;

#define CMD_CHANNEL 15

class Interface
{
public:
	Interface(IEC& iec);
	virtual ~Interface() {}

	void handler(void);

private:
	void reset(void);
	void saveFile();
	void sendFile();
	void sendListing(/*PFUNC_SEND_LISTING sender*/);
	void sendStatus(void);
	bool removeFilePrefix(void);
	void sendLineCallback(short lineNo, byte len, char* text);

	// handler helpers.
	void handleATNCmdCodeOpen(IEC::ATNCmd &cmd);
	void handleATNCmdCodeDataTalk(byte chan);
	void handleATNCmdCodeDataListen();

	// our iec low level driver:
	IEC& m_iec;
	// What operation state and media type we are dealing with
	byte m_interfaceState; // see InterfaceState
	// This var is set after an open command and determines what to send next
	byte m_openState;			// see OpenState
	byte m_queuedError;

	// send listing pointer in basic memory:
	volatile word m_basicPtr;
};

#endif
