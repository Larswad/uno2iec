//
// Title        : MMC2IEC - interface implementation
// Author       : Lars Pontoppidan
// Version      : 0.7
// Target MCU   : AtMega32(L) at 8 MHz
//
// CREDITS:
// --------
// The MMC2IEC application is inspired from Jan Derogee's 1541-III project for
// PIC: http://jderogee.tripod.com/
// This code is a complete reimplementation though, which includes some new
// features and excludes others.
//
// DESCRIPTION:
// The interface connects all the loose ends in MMC2IEC.
//
// Commands from the IEC communication are interpreted, and the appropriate data
// from either FAT, a D64 or T64 image is send back.
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//


#include <string.h>
#include "global_defines.h"
#include "interface.h"

#ifdef CONSOLE_DEBUG
#include "log.h"
#endif


namespace {
// atn command buffer struct
IEC::ATNCmd cmd;
char serCmdIOBuf[80];

// The previous cmd is copied to this string:
char oldCmdStr[IEC::ATN_CMD_MAX_LENGTH];

const char errorStr0[] = "00,OK";
const char errorStr1[] = "21,READ ERROR";
const char errorStr2[] = "26,WRITE PROTECT ON";
const char errorStr3[] = "33,SYNTAX ERROR";
const char errorStr4[] = "62,FILE NOT FOUND";
const char errorStr5[] = "63,FILE EXISTS";
const char errorStr6[] = "73,MMC2IEC DOS V0.8";
const char errorStr7[] = "74,DRIVE NOT READY";
const char errorStr8[] = "75,RPI SERIAL ERR.";
const char *error_table[ErrCount] = { errorStr0, errorStr1, errorStr2, errorStr3, errorStr4, errorStr5, errorStr6, errorStr7, errorStr8 };

const char errorEnding[] = ",00,00";
} // unnamed namespace


Interface::Interface(IEC& iec)
	: m_iec(iec)
{
	reset();

	// TODO: Say initial startup hello here. Scroll on max7219 or something.
	//
} // ctor


void Interface::reset(void)
{
	m_openState = O_NOTHING;
	m_queuedError = ErrIntro;

	//	if(sdReset()) {
	//		fatReset();
	//		fatInit();
	//	}

	//	if((fatGetStatus() bitand FAT_STATUS_OK) and sdCardOK) {
	//		interface_state = I_FAT;
	//	}
	m_interfaceState = IS_NATIVE;
} // reset


void Interface::sendStatus(void)
{
	byte i;
	// Send error string
	const char* str = (const char*)error_table[m_queuedError];

	while((i = *(str++)))
		m_iec.send(i);

	// Send common ending string ,00,00
	for(i = 0; i < sizeof(errorEnding) - 1; ++i)
		m_iec.send(errorEnding[i]);

	// ...and last byte in string as with EOI marker.
	m_iec.sendEOI(errorEnding[i]);
} // sendStatus


const char pstr_file_err[] = "ERROR: FILE FORMAT";

void send_file_err(void (*send_line)(word lineNo, byte len, char* text))
{
	char buffer[19];

	memcpy(buffer, pstr_file_err, 18);
	(send_line)(0, 18, buffer);
} // send_file_err


////////////////////////////////////////////////////////////////////////////////
//
// P00 file format implementation
//
/* TODO: Move as P00 format to PI, recode.
bool P00_reset(char *s)
{
	byte i;

	// Check filetype, and skip past 26 bytes
	if((fatFgetc() == 'C') and (fatFgetc() == '6') and (fatFgetc() == '4')) {

		for(i = 0; i < 23; i++)
			fatFgetc();

		if(!fatFeof())
			// All is ok
			return true;
	}

	return false;
} // P00_reset
*/

/* TODO: Move as P00 format to PI, recode.
bool close_to_fat(void)
{
	// Close and back to fat
	m_interfaceState = IS_NATIVE;
	fatFclose();
	return true;
} // close_to_fat
*/

////////////////////////////////////////////////////////////////////////////////
//
// Dummy functions for unused function pointers
//

byte dummy_1(char c)
{
	return true;
}

byte dummy_2(char *s)
{
	return true;
}

byte dummy_cmd(char *s)
{
	return ErrWriteProtectOn;
}

byte dummy_no_newfile(char *s)
{
	return false;
}


////////////////////////////////////////////////////////////////////////////////
//
// File formats definition
//

typedef  void(*PFUNC_SEND_LINE)(short line_no, unsigned char len, char *text);
typedef          void(*PFUNC_SEND_LISTING)(PFUNC_SEND_LINE send_line);
typedef          void(*PFUNC_VOID_VOID)(void);
typedef unsigned char(*PFUNC_UCHAR_VOID)(void);
typedef          char(*PFUNC_CHAR_VOID)(void);
typedef unsigned char(*PFUNC_UCHAR_CHAR)(char);
typedef          void(*PFUNC_VOID_CSTR)(char *s);
typedef unsigned char(*PFUNC_UCHAR_CSTR)(char *s);

#define FILE_FORMATS 5

struct file_format_struct {
	char extension[3];         // DOS extension of file format
	PFUNC_UCHAR_CSTR   init;   // intitialize driver, cmd_str is parameter,
	// must return true if OK
	PFUNC_SEND_LISTING send_listing; // Send listing function.
	PFUNC_UCHAR_CSTR   cmd;    // command channel command, must return ERR NUMBER!
	PFUNC_UCHAR_CSTR   open;   // open file
	PFUNC_UCHAR_CSTR   newfile;// create new empty file
	PFUNC_CHAR_VOID    getc;   // get char from open file
	PFUNC_UCHAR_VOID   eof;    // returns true if EOF
	PFUNC_UCHAR_CHAR   putc;   // write char to open file, return false if failure
	PFUNC_UCHAR_VOID   close;  // close file
};

/*
const struct file_format_struct file_format[FILE_FORMATS] = {
	{{0,0,0}, &dummy_2, &fat_send_listing, &fat_cmd,
	 &fatFopen, &fat_newfile, &fatFgetc, &fatFeof, &fatFputc, &fatFclose},

	{{'D','6','4'}, &D64_reset, &D64_send_listing, &dummy_cmd,
	 &D64_fopen, &dummy_no_newfile, &D64_fgetc, &D64_feof, &dummy_1, &D64_fclose},

	{{'M','2','I'}, &M2I_init, &M2I_send_listing, &M2I_cmd,
	 &M2I_open, &M2I_newfile, &M2I_getc, &M2I_eof, &M2I_putc, &M2I_close},

	{{'T','6','4'}, &T64_reset, &T64_send_listing, &dummy_cmd,
	 &T64_fopen, &dummy_no_newfile, &T64_fgetc, &T64_feof, &dummy_1, &T64_fclose},

	{{'P','0','0'}, &P00_reset, 0, &dummy_cmd,
	 &dummy_2, &dummy_no_newfile, &fatFgetc, &fatFeof, &dummy_1, &close_to_fat}
};
*/



////////////////////////////////////////////////////////////////////////////////
//
// Interface implementation
//


// Parse LOAD command, open either file/directory/d64/t64
//
/*
void Interface::openFile(const struct file_format_struct *pff)
{
	byte i;

	// fall back result
	m_openState = O_NOTHING;

	// Check double back arrow first
	if((cmd.str[0] == 95) and (cmd.str[1] == 95)) {
		// reload sdcard and send info
		m_interfaceState = IS_FAIL;
		m_openState = O_INFO;
	}
	else if(!sdCardOK or !(fatGetStatus() & FAT_STATUS_OK)) {
		// User tries to open stuff and there is a problem. Status is fail
		m_queuedError = ErrDriveNotReady;
		m_interfaceState = IS_FAIL;
	}
	else if(cmd.str[0] == '$') {
		// Send directory listing of current dir
		m_openState = O_DIR;
	}
	else if(cmd.str[0] == 95) {    // back arrow sign on CBM
		// One back arrow, exit current file format or cd..
		if(m_interfaceState == IS_NATIVE) {
			if(fatCddir(".."))
				m_openState = O_DIR;
		}
		else if(m_interfaceState not_eq IS_FAIL) {
			// We are in some other state, exit to FAT and show current dir
			m_interfaceState = IS_NATIVE;
			m_openState = O_DIR;
		}
	}
	else {

		// It was not a special command, remove eventual CBM dos prefix

		if(removeFilePrefix()) {
			// @ detected, save with replace:
			m_openState = O_SAVE_REPLACE;
		}
		else {
			// open file depending on interface state

			if(m_interfaceState == IS_NATIVE) {
				// Exchange 0xFF with tilde to allow shortened long filenames
				for(i = 0; i < cmd.strlen; i++) {
					if(cmd.str[i] == 0xFF)
						cmd.str[i] = '~';
				}

				// Try if cd works, then open file, then give up
				if(fatCddir(cmd.str)) {
					m_openState = O_DIR;
				}
				else if(fatFopen(cmd.str)) {
					// File opened, investigate filetype

					for(i = 1; i < NumInterfaceStates; i++) {
						pff = &(file_format[i]);
						if(memcmp(&(cmd.str[cmd.strlen - 3]), pff->extension, 3) == 0)
							break;
					}

					if(i < NumInterfaceStates) {
						// file extension matches, change interface state
						// call new format's reset
						if(((PFUNC_UCHAR_CSTR)(pff->init))(cmd.str)) {
							m_interfaceState = i;
							// see if this format supports listing
							if(pff->send_listing == NULL)
								m_openState = O_FILE;
							else
								m_openState = O_DIR;
						}
						else {
							// Error initializing driver, back to fat
							m_interfaceState = IS_NATIVE;
							m_openState = O_FILE_ERR;
						}
					}
					else {
						// No specific file format for this file, stay in FAT and send as .PRG
						m_openState = O_FILE;
					}

				}
				else {
					// File not found
					m_queuedError = ErrFileNotFound;
				}
			}
			else if(m_interfaceState not_eq IS_FAIL) {

				// Call file format's open command
				i = ((PFUNC_UCHAR_CSTR)(pff->open))(cmd.str);

				if(i)
					m_openState = O_FILE;
				else // File not found
					m_queuedError = ErrFileNotFound;
			}
		}
	}

	// Backup cmd string
	strcpy(oldCmdStr, cmd.str);
} // openFile
*/

// send basic line callback
void Interface::sendLineCallback(byte len, char* text)
{
	byte i;

	// Increment next line pointer
	// note: minus two here because the line number is included in the array already.
	m_basicPtr += len + 5 - 2;

	// Send that pointer
	m_iec.send(m_basicPtr bitand 0xFF);
	m_iec.send(m_basicPtr >> 8);

	// Send line number
//	m_iec.send(lineNo bitand 0xFF);
//	m_iec.send(lineNo >> 8);

	// Send line contents
	for(i = 0; i < len; i++)
		m_iec.send(text[i]);

	// Finish line
	m_iec.send(0);
} // sendLineCallback


void Interface::sendListing()
{
	noInterrupts();
	// Reset basic memory pointer:
	m_basicPtr = C64_BASIC_START;

	// Send load address
	m_iec.send(C64_BASIC_START bitand 0xff);
	m_iec.send((C64_BASIC_START >> 8) bitand 0xff);
	interrupts();
	// This will be slightly tricker: Need to specify the line sending protocol between raspberry and Arduino.
	// Call the listing function
	byte resp;
	do {
		Serial.write('L'); // initiate request.
		Serial.readBytes(serCmdIOBuf, 2);
		resp = serCmdIOBuf[0];
		if('L' == resp) { // PI will give us something else if we're at last line to send.
			// get the length as one byte: This is kind of specific: For listings we allow 256 bytes length. Period.
			byte len = serCmdIOBuf[1];
			byte actual = Serial.readBytes(serCmdIOBuf, len);
			if(len == actual) {
				// send the bytes directly to CBM!
				noInterrupts();
				sendLineCallback(len, serCmdIOBuf);
				interrupts();
			}
			else {
				resp = 'E'; // just to end the pain. We're out of sync or somthin'
				sprintf(serCmdIOBuf, "Expected: %d chars, got %d.", len, actual);
				Log(Error, FAC_IFACE, serCmdIOBuf);
			}
		}
		else {
			if('l' not_eq resp) {
				sprintf(serCmdIOBuf, "Ending at char: %d.", resp);
				Log(Error, FAC_IFACE, serCmdIOBuf);
				Serial.readBytes(serCmdIOBuf, sizeof(serCmdIOBuf));
				Log(Error, FAC_IFACE, serCmdIOBuf);
			}
		}
	} while('L' == resp); // keep looping for more lines as long as we got an 'L' indicating we haven't reached end.

	// So here the idea:
	// 1. Ask pi for line, "L"
	// 2. For first call, pi will fetch all the lines from current disk system (with callback interfaces) and then
	// collect them in a StringList.
	// Then it will send the first line in the QStringList as "L<byte_with_length><bytes>, and remove it from top of stringlist.
	// 3. We will keep asking for next line, if no more lines coming from Pi, then it will answer with little
	// l instead, and no subsequent bytes (sender)(&send_line_callback);

// experiment: Does it work?
//	noInterrupts();
//	sendLineCallback(10, 11, "PRINT \"HEJ\"");
//	sendLineCallback(20, 7, "GOTO 10");
//	interrupts();

	// End program
	noInterrupts();
	m_iec.send(0);
	m_iec.sendEOI(0);
	interrupts();
} // sendListing


void Interface::sendFile()
{
	// Send file bytes, such that the last one is sent with EOI.
	byte resp;
	do {
		Serial.write('R'); // ask for a byte
		byte len = Serial.readBytes(serCmdIOBuf, 2); // read the ack type ('B' or 'E')
		if(2 not_eq len)
			break;
		resp = serCmdIOBuf[0];
		len = serCmdIOBuf[1];
		if('B' == resp or 'E' == resp) {
			byte actual = Serial.readBytes(serCmdIOBuf, len);
			if(actual not_eq len) {
				Log(Error, FAC_IFACE, "Less than expected bytes, stopping.");
				break;
			}
			bool success = true;
			// so we get some bytes, send them to CBM.
			for(byte i = 0; success and i < len; ++i) { // End if sending to CBM fails.
				noInterrupts();
				if(resp == 'E' and i == len - 1)
					success = m_iec.sendEOI(serCmdIOBuf[i]); // indicate end of file.
				else
					success = m_iec.send(serCmdIOBuf[i]);
				interrupts();
			}
		}
		else if('E' not_eq resp)
			Log(Error, FAC_IFACE, "Got unexpected command response char.");
	} while(resp == 'B'); // keep asking for more as long as we don't get the 'E' or something else (indicating out of sync).

} // sendFile


void Interface::saveFile()
{
	// Recieve bytes until a EOI is detected
	do {
		byte c = m_iec.receive();
		// indicate to PI host that we want to write a byte.
		Serial.write('W');
		// and then we send the byte itself.
		Serial.write(c);
	} while(!(m_iec.state() bitand IEC::eoiFlag) and !(m_iec.state() bitand IEC::errorFlag));
} // saveFile


void Interface::handler(void)
{
//	const struct file_format_struct *pff;

	//  m_iec.setDeviceNumber(8);

	// old code for resetting if something's gone bad with SD card detection etc.
	//#ifdef SDCARD_DETECT
	//	if(!SDCARD_DETECT) {
	//		// No SD card, reset state
	//		BUSY_LED_OFF();
	//		DIRTY_LED_OFF();
	//		interface_state = IFail;
	//		sdCardOK = FALSE;
	//	}
	//	else {
	//		if(!sdCardOK) {
	//			// Limit reset cycle
	//			delay(50);

	//			// Try reseting SD card
	//			BUSY_LED_ON();
	//			Interface_reset();
	//			BUSY_LED_OFF();

	//		}
	//	}
	//#endif

	IEC::ATNCheck retATN = IEC::ATN_IDLE;
	if(m_iec.checkRESET()) {
		Log(Information, FAC_IFACE, "GOT RESET, INITIAL STATE");
		reset();
	}
	else {
		noInterrupts();
		retATN = m_iec.checkATN(cmd);
		interrupts();
	}

	if(retATN == IEC::ATN_ERROR) {
#ifdef CONSOLE_DEBUG
		Log(Error, FAC_IFACE, "ATNCMD: IEC_ERROR!");
#endif
		return;
	}

	// Did anything happen from the host side?
	if(retATN not_eq IEC::ATN_IDLE) {
		// A command is recieved
		//BUSY_LED_ON();

		//		if(IS_FAIL == m_interfaceState)
		//			pff = NULL;
		//		else
		//			pff = &(file_format[m_interfaceState]);

		// Make cmd string null terminated
		cmd.str[cmd.strlen] = '\0';
#ifdef CONSOLE_DEBUG
//		{
//			sprintf(serCmdIOBuf, "ATNCMD code:%d cmd: %s (len: %d) retATN: %d", cmd.code, cmd.str, cmd.strlen, retATN);
//			Log(Information, FAC_IFACE, serCmdIOBuf);
//		}
#endif

		// lower nibble is the channel.
		byte chan = cmd.code bitand 0x0F;

		// check upper nibble, the command itself.
		switch(cmd.code bitand 0xF0) {
			case IEC::ATN_CODE_OPEN:
				handleATNCmdCodeOpen(cmd);
			break;


			case IEC::ATN_CODE_DATA:  // data channel opened
				if(retATN == IEC::ATN_CMD_TALK)
					handleATNCmdCodeDataTalk(chan);
				else if(retATN == IEC::ATN_CMD_LISTEN)
					handleATNCmdCodeDataListen();
				break;

			case IEC::ATN_CODE_CLOSE:
				// TODO: handle close with PI.
//				if(0 not_eq pff) {
//					((PFUNC_UCHAR_VOID)(pff->close))();
//				}
				break;
		}

		//BUSY_LED_OFF();
	}
} // handler


void Interface::handleATNCmdCodeOpen(IEC::ATNCmd& cmd)
{
	sprintf(serCmdIOBuf, "O%u|%s\r", cmd.code bitand 0xF, cmd.str);
	// NOTE: PI side handles BOTH file open command AND the command channel command (from the cmd.code).
	Serial.print(serCmdIOBuf);
	// Note: the pi response handling can be done LATER! We're in quick business with the CBM here!
} // handleATNCmdCodeOpen


void Interface::handleATNCmdCodeDataTalk(byte chan)
{
	byte lengthOrResult;
	boolean wasSuccess = false;
	if(lengthOrResult = Serial.readBytes(serCmdIOBuf, 3)) {
		// process response into m_queuedError.
		// Response: ><code in binary><CR>
		if('>' == serCmdIOBuf[0] and 3 == lengthOrResult) {
			lengthOrResult = serCmdIOBuf[1];
			wasSuccess = true;
		}
		else
			Log(Error, FAC_IFACE, serCmdIOBuf);
	}
	if(CMD_CHANNEL == chan) {
		m_queuedError = wasSuccess ? lengthOrResult : ErrSerialComm;
		// Send status message
		sendStatus();
		// go back to OK state, we have dispatched the error to IEC host now.
		m_queuedError = ErrOK;
	}
	else {
		m_openState = wasSuccess ? lengthOrResult : O_NOTHING;

		switch(m_openState) {
		case O_INFO:
			// Reset and send SD card info
			reset();
			// TODO: interface with PI (file system media info).
			sendListing();
			break;

		case O_FILE_ERR:
			// TODO: interface with pi for error info.
			sendListing(/*&send_file_err*/);
			break;

		case O_NOTHING: /*or (0 == pff)*/
			// Say file not found
			m_iec.sendFNF();
			break;

		case O_FILE:
			// Send program file
			// TODO: interface with PI before file sending TO CBM takes place.
			sendFile();
			break;

		case O_DIR:
			// Send listing
			// TODO: interface with pi to get current file system listing.
			sendListing(/*(PFUNC_SEND_LISTING)(pff->send_listing)*/);
			break;
		}
	}
//	Log(Information, FAC_IFACE, serCmdIOBuf);
} // handleATNCmdCodeDataTalk


void Interface::handleATNCmdCodeDataListen()
{
	// We are about to save stuff
	if(0 == 0 /*pff*/) {
		// file format functions unavailable, save dummy
		saveFile(/*&dummy_1*/);
		m_queuedError = ErrDriveNotReady;
	}
	else {
		// Check conditions before saving
		boolean writeSuccess = true;
		if(m_openState not_eq O_SAVE_REPLACE) {
			// Not a save with replace, if file exists its an error
			if(m_queuedError not_eq ErrFileNotFound) {
				m_queuedError = ErrFileExists;
				writeSuccess = false;
			}
		}

		if(writeSuccess) {
			// No overwrite problem, try to create a file to save in:
			//writeSuccess = ((PFUNC_UCHAR_CSTR)(pff->newfile))(oldCmdStr);
			if(!writeSuccess)
				// unable to save, just say write protect
				m_queuedError = ErrWriteProtectOn;
			else
				m_queuedError = ErrOK;

		}

		// TODO: saveFile to RP.
		if(writeSuccess)
			saveFile(/*(PFUNC_UCHAR_CHAR)(pff->putc)*/);
		else
			saveFile(/*&dummy_1*/);
	}
} // handleATNCmdCodeDataListen
