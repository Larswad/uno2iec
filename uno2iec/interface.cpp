//
// Title        : RPI2UNO2IEC - interface implementation, arduino side.
// Author       : Lars Wadefalk
// Version      : 0.1
// Target MCU   : Arduino Uno AtMega328(H, 5V) at 16 MHz, 2KB SRAM, 32KB flash, 1KB EEPROM.
//
// CREDITS:
// --------
// The RPI2UNO2IEC application is inspired by Lars Pontoppidan's MMC2IEC project.
// It has been ported to C++.
// The MMC2IEC application is inspired from Jan Derogee's 1541-III project for
// PIC: http://jderogee.tripod.com/
// This code is a complete reimplementation though, which includes some new
// features and excludes others.
//
// DESCRIPTION:
// The interface connects all the loose ends in MMC2IEC.
//
// Commands from the IEC communication are interpreted, and the appropriate data
// from either Native, a D64 or T64 image is sent back.
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

#include "log.h"

using namespace CBM;

namespace {

// atn command buffer struct
IEC::ATNCmd cmd;
char serCmdIOBuf[MAX_BYTES_PER_REQUEST];

#ifdef USE_LED_DISPLAY
byte scrollBuffer[30];
#endif

} // unnamed namespace


Interface::Interface(IEC& iec)
	: m_iec(iec)
#ifdef USE_LED_DISPLAY
	, m_pDisplay(0)
#endif

{
	reset();
} // ctor


void Interface::reset(void)
{
	m_openState = O_NOTHING;
	m_queuedError = ErrIntro;
} // reset


void Interface::sendStatus(void)
{
	byte i, readResult;
	Serial.write('E'); // ask for error string from the last queued error.
	Serial.write(m_queuedError);

	// first sync the response.
	do {
		readResult = Serial.readBytes(serCmdIOBuf, 1);
	} while(readResult not_eq 1 or serCmdIOBuf[0] not_eq ':');
	// get the string.
	readResult = Serial.readBytesUntil('\r', serCmdIOBuf, sizeof(serCmdIOBuf));
	if(!readResult)
		return; // something went wrong with result from host.

	// Length does not include the CR, write all but the last one should be with EOI.
	for(i = 0; i < readResult - 2; ++i)
		m_iec.send(serCmdIOBuf[i]);
	// ...and last byte in string as with EOI marker.
	m_iec.sendEOI(serCmdIOBuf[i]);
} // sendStatus


// send single basic line, including heading basic pointer and terminating zero.
void Interface::sendLine(byte len, char* text)
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
} // sendLine


void Interface::sendListing()
{
	noInterrupts();
	// Reset basic memory pointer:
	m_basicPtr = C64_BASIC_START;

	// Send load address
	m_iec.send(C64_BASIC_START bitand 0xff);
	m_iec.send((C64_BASIC_START >> 8) bitand 0xff);
	interrupts();
	// This will be slightly tricker: Need to specify the line sending protocol between Host and Arduino.
	// Call the listing function
	byte resp;
	do {
		Serial.write('L'); // initiate request.
		Serial.readBytes(serCmdIOBuf, 2);
		resp = serCmdIOBuf[0];
		if('L' == resp) { // PI will give us something else if we're at last line to send.
			// get the length as one byte: This is kind of specific: For listings we allow 256 bytes length. Period.
			byte len = serCmdIOBuf[1];
			// WARNING: Here we might need to read out the data in portions. The serCmdIOBuf might just be too small
			// for very long lines.
			byte actual = Serial.readBytes(serCmdIOBuf, len);
			if(len == actual) {
				// send the bytes directly to CBM!
				noInterrupts();
				sendLine(len, serCmdIOBuf);
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

	// End program with two zeros after last line. Last zero goes out as EOI.
	noInterrupts();
	m_iec.send(0);
	m_iec.sendEOI(0);
	interrupts();
} // sendListing


void Interface::sendFile()
{
	// Send file bytes, such that the last one is sent with EOI.
	byte resp;
	Serial.write('S'); // ask for file size.
	byte len = Serial.readBytes(serCmdIOBuf, 3);
	// it is supposed to answer with S<highByte><LowByte>
	if(3 not_eq len or serCmdIOBuf[0] not_eq 'S')
		return;
	word written = 0;

#ifdef USE_LED_DISPLAY
	if(0 not_eq m_pDisplay)
		m_pDisplay->resetPercentage(((word)(serCmdIOBuf[1]) << 8) bitor serCmdIOBuf[2]);
#endif

	bool success = true;
	do {
		Serial.write('R'); // ask for a byte/bunch of bytes
		len = Serial.readBytes(serCmdIOBuf, 2); // read the ack type ('B' or 'E')
		if(2 not_eq len) {
			Log(Error, FAC_IFACE, "2 Host bytes expected, stopping");
			break;
		}
		resp = serCmdIOBuf[0];
		len = serCmdIOBuf[1];
		if('B' == resp or 'E' == resp) {
			byte actual = Serial.readBytes(serCmdIOBuf, len);
			if(actual not_eq len) {
				Log(Error, FAC_IFACE, "Host bytes expected, stopping");
				break;
			}
			// so we get some bytes, send them to CBM.
			for(byte i = 0; success and i < len; ++i) { // End if sending to CBM fails.
				noInterrupts();
				if(resp == 'E' and i == len - 1)
					success = m_iec.sendEOI(serCmdIOBuf[i]); // indicate end of file.
				else
					success = m_iec.send(serCmdIOBuf[i]);
				interrupts();
				++written;

#ifdef USE_LED_DISPLAY
				if(!(written % 32) and 0 not_eq m_pDisplay)
					m_pDisplay->showPercentage(written);
#endif
			}
		}
		else if('E' not_eq resp)
			Log(Error, FAC_IFACE, "Got unexp. cmd resp.char.");
	} while(resp == 'B' and success); // keep asking for more as long as we don't get the 'E' or something else (indicating out of sync).

#ifdef USE_LED_DISPLAY
	if(0 not_eq m_pDisplay)
		m_pDisplay->showPercentage(written);
#endif
} // sendFile


void Interface::saveFile()
{
	boolean done = false;
	// Recieve bytes until a EOI is detected
	serCmdIOBuf[0] = 'W';
	do {
		byte bytesInBuffer = 2;
		do {
			noInterrupts();
			serCmdIOBuf[bytesInBuffer++] = m_iec.receive();
			interrupts();
			done = (m_iec.state() bitand IEC::eoiFlag) or (m_iec.state() bitand IEC::errorFlag);
		} while(bytesInBuffer < sizeof(serCmdIOBuf) and !done);
		// indicate to media host that we want to write a buffer. Give the total length including the heading 'W'+length bytes.
		serCmdIOBuf[1] = bytesInBuffer;
		Serial.write((const uint8_t*)serCmdIOBuf, bytesInBuffer);
		Serial.flush();
	} while(!done);
} // saveFile


byte Interface::handler(void)
{
	if(m_iec.checkRESET()) {
		// IEC reset line is in reset state, so we should set all states in reset.
		reset();
		return IEC::ATN_RESET;
	}
	noInterrupts();
	IEC::ATNCheck retATN = m_iec.checkATN(cmd);
	interrupts();

	if(retATN == IEC::ATN_ERROR) {
		Log(Error, FAC_IFACE, "ATNCMD: IEC_ERROR!");
		reset();
	}
	// Did anything happen from the host side?
	else if(retATN not_eq IEC::ATN_IDLE) {
		// A command is recieved, make cmd string null terminated
		cmd.str[cmd.strlen] = '\0';
#ifdef CONSOLE_DEBUG
		{
			sprintf(serCmdIOBuf, "ATN code:%d cmd: %s (len: %d) retATN: %d", cmd.code, cmd.str, cmd.strlen, retATN);
			Log(Information, FAC_IFACE, serCmdIOBuf);
		}
#endif

		// lower nibble is the channel.
		byte chan = cmd.code bitand 0x0F;

		// check upper nibble, the command itself.
		switch(cmd.code bitand 0xF0) {
			case IEC::ATN_CODE_OPEN:
				// Open either file or prg for reading, writing or single line command on the command channel.
				// In any case we just issue an 'OPEN' to the host and let it process.
				// Note: Some of the host response handling is done LATER, since we will get a TALK or LISTEN after this.
				// Also, simply issuing the request to the host and not waiting for any response here makes us more
				// responsive to the CBM here, when the DATA with TALK or LISTEN comes in the next sequence.
				handleATNCmdCodeOpen(cmd);
			break;

			case IEC::ATN_CODE_DATA:  // data channel opened
				if(retATN == IEC::ATN_CMD_TALK) {
					 // when the CMD channel is read (status), we first need to issue the host request. The data channel is opened directly.
					if(CMD_CHANNEL == chan)
						handleATNCmdCodeOpen(cmd);
					handleATNCmdCodeDataTalk(chan);
				}
				else if(retATN == IEC::ATN_CMD_LISTEN)
					handleATNCmdCodeDataListen();
				else if(retATN == IEC::ATN_CMD)
					handleATNCmdCodeOpen(cmd);
				break;

			case IEC::ATN_CODE_CLOSE:
				// handle close with host.
				handleATNCmdClose();
				break;

			case IEC::ATN_CODE_LISTEN:
				Log(Information, FAC_IFACE, "LISTEN");
				break;
			case IEC::ATN_CODE_TALK:
				Log(Information, FAC_IFACE, "TALK");
				break;
			case IEC::ATN_CODE_UNLISTEN:
				//Log(Information, FAC_IFACE, "UNLISTEN");
				break;
			case IEC::ATN_CODE_UNTALK:
				//Log(Information, FAC_IFACE, "UNTALK");
				break;
		} // switch
	} // IEC not idle

	return retATN;
} // handler


#ifdef USE_LED_DISPLAY
void Interface::setMaxDisplay(Max7219 *pDisplay)
{
	m_pDisplay = pDisplay;
} // setMaxDisplay
#endif


void Interface::handleATNCmdCodeOpen(IEC::ATNCmd& cmd)
{
	sprintf(serCmdIOBuf, "O%u|%s\r", cmd.code bitand 0xF, cmd.str);
	// NOTE: Host side handles BOTH file open command AND the command channel command (from the cmd.code).
	Serial.print(serCmdIOBuf);
} // handleATNCmdCodeOpen


void Interface::handleATNCmdCodeDataTalk(byte chan)
{
	byte lengthOrResult;
	boolean wasSuccess = false;

	// process response into m_queuedError.
	// Response: ><code in binary><CR>

	serCmdIOBuf[0] = 0;
	do {
		lengthOrResult = Serial.readBytes(serCmdIOBuf, 1);
	} while(lengthOrResult not_eq 1 or serCmdIOBuf[0] not_eq '>');

	if(!lengthOrResult or '>' not_eq serCmdIOBuf[0]) {
		m_iec.sendFNF();
		Log(Error, FAC_IFACE, "response not sync.");
	}
	else {
		if(lengthOrResult = Serial.readBytes(serCmdIOBuf, 2)) {
			if(2 == lengthOrResult) {
				lengthOrResult = serCmdIOBuf[0];
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
				sendListing();
				break;

			case O_FILE_ERR:
				// FIXME: interface with pi for error info.
				//sendListing(/*&send_file_err*/);
				m_iec.sendFNF();
				break;

			case O_NOTHING:
				// Say file not found
				m_iec.sendFNF();
				break;

			case O_FILE:
				// Send program file
				sendFile();
				break;

			case O_DIR:
				// Send listing
				sendListing();
				break;
			}
		}
	}
//	Log(Information, FAC_IFACE, serCmdIOBuf);
} // handleATNCmdCodeDataTalk


void Interface::handleATNCmdCodeDataListen()
{
	byte lengthOrResult;
	boolean wasSuccess = false;

	// process response into m_queuedError.
	// Response: ><code in binary><CR>

	serCmdIOBuf[0] = 0;
	do {
		lengthOrResult = Serial.readBytes(serCmdIOBuf, 1);
	} while(lengthOrResult not_eq 1 or serCmdIOBuf[0] not_eq '>');

	if(!lengthOrResult or '>' not_eq serCmdIOBuf[0]) {
		// FIXME: Check what the drive does here when things go wrong. FNF is probably not right.
		m_iec.sendFNF();
		Log(Error, FAC_IFACE, "response not sync.");
	}
	else {
		if(lengthOrResult = Serial.readBytes(serCmdIOBuf, 2)) {
			if(2 == lengthOrResult) {
				lengthOrResult = serCmdIOBuf[0];
				wasSuccess = true;
			}
			else
				Log(Error, FAC_IFACE, serCmdIOBuf);
		}
		m_queuedError = wasSuccess ? lengthOrResult : ErrSerialComm;

		if(ErrOK == m_queuedError)
			saveFile();
		else // FIXME: Check what the drive does here when saving goes wrong. FNF is probably not right.
			m_iec.sendFNF();
	}
} // handleATNCmdCodeDataListen


void Interface::handleATNCmdClose()
{
	// handle close of file. PI will return the name of the last loaded file to us.
	Serial.print("C");
	Serial.readBytes(serCmdIOBuf, 2);
	byte resp = serCmdIOBuf[0];
	if('N' == resp or 'n' == resp) { // N indicates we have a name.
		// get the length of the name as one byte.
		byte len = serCmdIOBuf[1];
		byte actual = Serial.readBytes(serCmdIOBuf, len);
		if(len == actual) {
#ifdef USE_LED_DISPLAY
			serCmdIOBuf[len] = '\0';
			if('n' == resp)
				strcpy((char*)scrollBuffer, " SAVED: ");
			else
				strcpy((char*)scrollBuffer, " LOADED: ");
			strcat((char*)scrollBuffer, serCmdIOBuf);

			if(0 not_eq m_pDisplay)
				m_pDisplay->resetScrollText(scrollBuffer);
#endif

		}
		else {
			sprintf(serCmdIOBuf, "Exp: %d chars, got %d.", len, actual);
			Log(Error, FAC_IFACE, serCmdIOBuf);
		}
	}
} // handleATNCmdClose
