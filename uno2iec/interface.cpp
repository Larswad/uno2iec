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
/*
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
*/
#include "interface.h"

#ifdef CONSOLE_DEBUG
#include "log.h"
#endif


// atn command buffer struct
IEC::ATNCmd cmd;

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
const char *error_table[ErrCount] = { errorStr0, errorStr1, errorStr2, errorStr3, errorStr4, errorStr5, errorStr6, errorStr7};

const char errorEnding[] = ",00,00";

Interface::Interface(IEC& iec)
	: m_iec(iec)
{
	reset();

	// Say hello, flash busy led
	//  BUSY_LED_ON();
	//  BUSY_LED_SETDDR();
	//  ms_spin(20);
	//  BUSY_LED_OFF();
} // ctor


void Interface::reset(void)
{
	m_openState = O_NOTHING;
	m_queuedError = ErrIntro;

	////	if(sdReset()) {
	////		fatReset();
	////		fatInit();
	////	}

	//	if((fatGetStatus() bitand FAT_STATUS_OK) and sdCardOK) {
	//		interface_state = I_FAT;
	//	}
	m_interfaceState = IS_NATIVE;
} // reset


void Interface::sendStatus(void)
{
	byte i;
	// Send error string
	const char *str = (const char *)error_table[m_queuedError];

	while((i = *(str++)))
		m_iec.send(i);

	// Send common ending string ,00,00
	for(i = 0; i < 5; i++)
		m_iec.send(errorEnding[i]);

	m_iec.sendEOI(errorEnding[i]);
} // sendStatus


////////////////////////////////////////////////////////////////////////////////
//
// Send SD info function

const char strSDState1[] = "ERROR: SD/MMC";
const char strSDState2[] = "ERROR: FILE SYSTEM";
const char strSDState3[] = "FAT16 OK";
const char strSDState4[] = "FAT32 OK";

const char pstr_file_err[] = "ERROR: FILE FORMAT";

void send_sdinfo(void (*send_line)(short line_no, unsigned char len, char *text))
{
	char buffer[19];
	unsigned char c;

	c = fatGetStatus();

	if(!sdCardOK) {
		// SD Card not ok
		memcpy(buffer, strSDState1, 13);
		c = 13;
	}
	else if(!(c & FAT_STATUS_OK)) {
		// File system not ok
		memcpy(buffer, strSDState2, 18);
		c = 18;
	}
	else {
		// Everything OK, print fat status
		if(c & FAT_STATUS_FAT32)
			memcpy(buffer, strSDState4, 8);
		else
			memcpy(buffer, strSDState3, 8);

		c = 8;
	}
	(send_line)(0, c, buffer);
} // send_sdinfo


void send_file_err(void (*send_line)(word lineNo, byte len, char* text))
{
	char buffer[19];

	memcpy(buffer, pstr_file_err, 18);
	(send_line)(0, 18, buffer);
} // send_file_err


////////////////////////////////////////////////////////////////////////////////
//
// FAT interfacing functions
//
const char pstr_dir[] = " <DIR>";

void fat_send_listing(void (*send_line)(short line_no, unsigned char len, char *text))
{
	unsigned short s;
	unsigned char i;
	char buffer[24];

	struct diriterator di;
	struct direntry *de;

	// Prepare buffer
	memset(buffer, ' ', 3);
	buffer[3] = '"';   // quotes

	// Iterate through directory
	de = fatFirstDirEntry(fatGetCurDirCluster(), &di);

	while(de not_eq NULL) {
		if(*de->deName == SLOT_EMPTY)
			break; // there are no more direntries

		if((*de->deName not_eq SLOT_DELETED) and
			 (de->deAttributes not_eq ATTR_LONG_FILENAME)) {
			if(de->deAttributes == ATTR_VOLUME) {
				// Print volume label line. This will be 11 chars
				(send_line)(0, 11, de->deName);
			}
			else {
				// Regular file/directory
				if(de->deAttributes & ATTR_DIRECTORY) {
					// Its a dir
					memcpy(&(buffer[4]), de->deName, 8);    // name
					buffer[12] = ' ';                       // space
					memcpy(&(buffer[13]), de->deName+8, 3); // ext
					buffer[16] = 0x22;                      // quote
					memcpy_P(&(buffer[17]), pstr_dir, 6);   // line end

					(send_line)(0, 23, buffer);
				}
				else {
					// Its a file, calc file size in kB:
					s = (de->deFileSize + (1 << 10) - 1) >> 10;

					// Calc number of spaces required
					if(s > 9999) {
						s = 9999;
						i = 0;
					}
					else if(s >= 1000)
						i = 0;
					else if(s >= 100)
						i = 1;
					else if(s >= 10)
						i = 2;
					else
						i = 3;

					memcpy(&(buffer[4]), de->deName, 8);    // name
					buffer[12] = '.';                       // dot
					memcpy(&(buffer[13]), de->deName + 8, 3); // ext
					buffer[16] = '"';                      // quote

					(send_line)(s, 14 + i, &(buffer[3 - i]));
				}
			}
		}

		de = fatNextDirEntry(&di);
	}

	// Was this a natural ending??
	if(sdCardOK == FALSE) {
		// say  "ERROR: SD/MMC"
		memcpy_P(buffer, strSDState1, 13);
		(send_line)(0, 13, buffer);
	}
}

// Fat parsing command channel
//
unsigned char fat_cmd(char *c)
{

	// Get command letter and argument

	char cmd_letter;
	char *arg, *p;
	char ret = ERR_OK;

	cmd_letter = cmd.str[0];

	arg = strchr(cmd.str,':');

	if(arg not_eq NULL) {
		arg++;

		if(cmd_letter == 'S') {
			// Scratch a file
			if(!fatRemove(arg))
				ret = ERR_FILE_NOT_FOUND;
		}
		else if(cmd_letter == 'R') {
			// Rename, find =  and replace with 0 to get cstr

			p = strchr(arg, '=');

			if(p not_eq NULL) {
				*p = 0;
				p++;
				if(!fatRename(p, arg)) {
					ret = ERR_FILE_NOT_FOUND;
				}
			}
		}
		else if(cmd_letter == 'N') {
			// Format new disk creates an M2I file
			ret = M2I_newdisk(arg);

			if(ret == ERR_OK) {
				// It worked, go to M2I state
				interface_state = I_M2I;
			}

		}
		else {
			ret = ErrSyntaxError;
		}
	}

	return ret;
}


unsigned char fat_newfile(char *filename)
{
	// Remove and create file
	fatRemove(filename);
	return fatFcreate(filename);
}


////////////////////////////////////////////////////////////////////////////////
//
// P00 file format implementation
//

unsigned char P00_reset(char *s)
{
	unsigned char i;

	// Check filetype, and skip past 26 bytes
	if((fatFgetc() == 'C') and (fatFgetc() == '6') and (fatFgetc() == '4')) {

		for(i = 0; i < 23; i++)
			fatFgetc();

		if(!fatFeof())
			// All is ok
			return TRUE;
	}

	return FALSE;
}

unsigned char close_to_fat(void)
{
	// Close and back to fat
	m_interfaceState = IS_NATIVE;
	fatFclose();
	return TRUE;
}

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

const struct file_format_struct file_format[FILE_FORMATS] = {
	{{0,0,0}, &dummy_2, &fat_send_listing, &fat_cmd,
	 &fatFopen, &fat_newfile, &fatFgetc, &fatFeof, &fatFputc, &fatFclose},
	{{'D','6','4'}, &D64_reset, &D64_send_listing, &dummy_cmd,
	 &D64_fopen, &dummy_no_newfile, &D64_fgetc, &D64_feof, &dummy_1, &D64_fclose},
	{{'M','2','I'}, &M2I_init, &M2I_send_listing, &M2I_cmd,
	 &M2I_open, &M2I_newfile, &M2I_getc, &M2I_eof, &M2I_putc, &M2I_close},
	{{'T','6','4'}, &T64_reset, &T64_send_listing, &dummy_cmd,
	 &T64_fopen, &dummy_no_newfile, &T64_fgetc, &T64_feof, &dummy_1, &T64_fclose},
	{{'P','0','0'}, &P00_reset, NULL, &dummy_cmd,
	 &dummy_2, &dummy_no_newfile, &fatFgetc, &fatFeof, &dummy_1, &close_to_fat}
};




////////////////////////////////////////////////////////////////////////////////
//
// Interface implementation
//


// Parse LOAD command, open either file/directory/d64/t64
//
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
	else if(cmd.str[0] == 95) {    // back arrow sign on C64
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


// send basic line callback
void Interface::sendLineCallback(short lineNo, byte len, char* text)
{
	byte i;

	// Increment next line pointer
	m_basicPtr += len + 5;

	// Send that pointer
	m_iec.send(m_basicPtr bitand 0xFF);
	m_iec.send(m_basicPtr >> 8);

	// Send line number
	m_iec.send(lineNo bitand 0xFF);
	m_iec.send(lineNo >> 8);

	// Send line contents
	for(i = 0; i < len; i++)
		m_iec.send(text[i]);

	// Finish line
	m_iec.send(0);
} // sendLineCallback


void Interface::sendListing(/*PFUNC_SEND_LISTING sender*/)
{
	// Reset basic memory pointer:
	m_basicPtr = C64_BASIC_START;

	// Send load address
	m_iec.send(C64_BASIC_START bitand 0xff);
	m_iec.send((C64_BASIC_START >> 8) bitand 0xff);

	// This will be slightly tricker: Need to specify the line sending protocol between raspberry and Arduino.
	// Call the listing function
	//	(sender)(&send_line_callback);

	// End program
	m_iec.send(0);
	m_iec.sendEOI(0);
} // sendListing


void Interface::sendFile()
{
	// Send file bytes, such that the last one is sent with EOI:
	Serial.write('R');
	byte s = Serial.read();
	byte c = Serial.read();
	while('B' == s) {
		if(!m_iec.send(c))
			break;
		Serial.write('R');
		s = Serial.read();
		c = Serial.read();
	}

	// old code.
	//	c = (getc)();

	//	while(!(eof)()) {
	//		if(!m_iec.send(c))
	//			break;
	//		c = (getc)();
	//	}

	m_iec.sendEOI(c);
} // sendFile


void Interface::saveFile()
{
	// Recieve bytes until a EOI is detected
	do {
		byte c = m_iec.receive();
		Serial.write('W');
		Serial.write(c);
		//		(putc)(c);
	} while(!(m_iec.state() bitand IEC::eoiFlag) and !(m_iec.state() bitand IEC::errorFlag));
} // saveFile


void Interface::handler(void)
{
	const struct file_format_struct *pff;

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

	IEC::ATNCheck ret = m_iec.checkATN(cmd);

	if(ret == IEC::ATN_ERROR) {
#ifdef CONSOLE_DEBUG
		Log(Error, FAC_IFACE, "ATNCMD: IEC_ERROR!");
#endif
		return;
	}

	// Did anything happen from the host side?
	if(ret not_eq IEC::ATN_IDLE) {
		// A command is recieved
		//BUSY_LED_ON();

#ifdef CONSOLE_DEBUG
		{
			char buffer[60];
			sprintf("ATNCMD: %c %s %c", cmd.Code, cmd.str, ret);
			Log(Information, FAC_IFACE, buffer);
		}
#endif

		//		if(IS_FAIL == m_interfaceState)
		//			pff = NULL;
		//		else
		//			pff = &(file_format[m_interfaceState]);

		// Make cmd string null terminated
		cmd.str[cmd.strlen] = '\0';

		// lower nibble is the channel.
		byte chan = cmd.code bitand 0x0F;

		// check upper nibble, the command itself.
		char serCmdBuf[40];
		byte lenRead;

		switch(cmd.code bitand 0xF0) {
			case IEC::ATN_CODE_OPEN:
				sprintf(strBuf, "O%u|%s\r", cmd.code bitand 0xF, serCmdBuf);
				Serial.println(strBuf);
				lenRead = Serial.readBytesUntil('\r', serCmdBuf, sizeof(serCmdBuf));
				if(lenRead) {
					// TODO: process rpi response into m_queuedError.

				}
				else // very specific error: The serial comm with the host failed.
					m_queuedError = ErrSerialComm;
				break;

			case IEC::ATN_CODE_DATA:  // data channel opened

				if(ret == IEC::ATN_CMD_TALK) {
					if(CMD_CHANNEL == chan) {
						// Send status message
						sendStatus();
						// go back to OK state, we have dispatched the error to IEC host now.
						m_queuedError = ErrOK;
					}
					else if(m_openState == O_INFO) {
						// Reset and send SD card info
						reset();
						sendListing(&send_sdinfo);
					}
					else if(m_openState == O_FILE_ERR) {
						sendListing(&send_file_err);
					}
					else if((m_openState == O_NOTHING) or (pff == NULL)) {
						// Say file not found
						m_iec.sendFNF();
					}
					else if(m_openState == O_FILE) {
						// Send program file
						sendFile((PFUNC_CHAR_VOID)(pff->getc),
										 (PFUNC_UCHAR_VOID)(pff->eof));
					}
					else if(m_openState == O_DIR) {
						// Send listing
						sendListing((PFUNC_SEND_LISTING)(pff->send_listing));
					}

				}
				else if(ret == IEC::ATN_CMD_LISTEN) {
					// We are about to save stuff
					if(pff == NULL) {
						// file format functions unavailable, save dummy
						saveFile(/*&dummy_1*/);
						m_queuedError = ErrDriveNotReady;
					}
					else {
						// Check conditions before saving
						ret = true;
						if(m_openState not_eq O_SAVE_REPLACE) {
							// Not a save with replace, if file exists its an error
							if(m_queuedError not_eq ErrFileNotFound) {
								m_queuedError = ErrFileExists;
								ret = false;
							}
						}

						if(ret) {
							// No overwrite problem, try to create a file to save in:
							ret = ((PFUNC_UCHAR_CSTR)(pff->newfile))(oldCmdStr);
							if(!ret)
								// unable to save, just say write protect
								m_queuedError = ErrWriteProtectOn;
							else
								m_queuedError = ErrOK;

						}

						if(ret)
							saveFile(/*(PFUNC_UCHAR_CHAR)(pff->putc)*/);
						else
							saveFile(/*&dummy_1*/);
					}
				}
				break;

			case IEC::ATN_CODE_CLOSE:
				if(pff not_eq NULL) {
					((PFUNC_UCHAR_VOID)(pff->close))();
				}
				break;
		}

		//BUSY_LED_OFF();
	}
} // handler
