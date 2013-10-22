#ifndef INTERFACE_H
#define INTERFACE_H

#include "iec_driver.h"
#ifdef USE_LED_DISPLAY
#include <max7219.h>
#endif

#include "cbmdefines.h"

/*
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
*/

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

class Interface
{
public:
	Interface(IEC& iec);
	virtual ~Interface() {}

	// The handler returns the current IEC state, see the iec_driver.hpp for possible states.
	byte handler(void);

#ifdef USE_LED_DISPLAY
	void setMaxDisplay(Max7219* pDisplay);
#endif

private:
	void reset(void);
	void saveFile();
	void sendFile();
	void sendListing(/*PFUNC_SEND_LISTING sender*/);
	void sendStatus(void);
	bool removeFilePrefix(void);
	void sendLine(byte len, char* text);

	// handler helpers.
	void handleATNCmdCodeOpen(IEC::ATNCmd &cmd);
	void handleATNCmdCodeDataTalk(byte chan);
	void handleATNCmdCodeDataListen();
	void handleATNCmdClose();

	// our iec low level driver:
	IEC& m_iec;
	// This var is set after an open command and determines what to send next
	byte m_openState;			// see OpenState
	byte m_queuedError;

	// send listing pointer in basic memory:
	volatile word m_basicPtr;
	// atn command buffer struct
	IEC::ATNCmd& m_cmd;
#ifdef USE_LED_DISPLAY
	Max7219* m_pDisplay;
#endif
};

#endif
