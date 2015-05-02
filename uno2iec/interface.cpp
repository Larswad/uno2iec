//
// Title        : UNO2IEC - interface implementation, arduino side.
// Author       : Lars Wadefalk
// Version      : 0.1
// Target MCU   : Arduino Uno AtMega328(H, 5V) at 16 MHz, 2KB SRAM, 32KB flash, 1KB EEPROM.
//
// CREDITS:
// --------
// The UNO2IEC application is inspired by Lars Pontoppidan's MMC2IEC project.
// It has been ported to C++.
// The MMC2IEC application is inspired from Jan Derogee's 1541-III project for
// PIC: http://jderogee.tripod.com/
// This code is a complete reimplementation though, which includes some new
// features and excludes others.
//
// DESCRIPTION:
// This "interface" class is the main driving logic for the IEC command handling.
//
// Commands from the IEC communication are interpreted, and the appropriate data
// from either Native, D64, T64, M2I, x00 image formats is sent back.
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

// Buffer for incoming and outgoing serial bytes and other stuff.
char serCmdIOBuf[MAX_BYTES_PER_REQUEST];

#ifdef USE_LED_DISPLAY
byte scrollBuffer[50];
#endif

} // unnamed namespace


Interface::Interface(IEC& iec)
	: m_iec(iec)
#ifdef USE_LED_DISPLAY
	, m_pDisplay(0)
#endif
	// NOTE: Householding with RAM bytes: We use the middle of serial buffer for the ATNCmd buffer info.
	// This is ok and won't be overwritten by actual serial data from the host, this is because when this ATNCmd data is in use
	// only a few bytes of the actual serial data will be used in the buffer.
	, m_cmd(*reinterpret_cast<IEC::ATNCmd*>(&serCmdIOBuf[sizeof(serCmdIOBuf) / 2]))
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
	COMPORT.write('E'); // ask for error string from the last queued error.
	COMPORT.write(m_queuedError);

	// first sync the response.
	do {
		readResult = COMPORT.readBytes(serCmdIOBuf, 1);
	} while(readResult not_eq 1 or serCmdIOBuf[0] not_eq ':');
	// get the string.
	readResult = COMPORT.readBytesUntil('\r', serCmdIOBuf, sizeof(serCmdIOBuf));
	if(not readResult)
		return; // something went wrong with result from host.

	// Length does not include the CR, write all but the last one should be with EOI.
	for(i = 0; i < readResult - 2; ++i)
		m_iec.send(serCmdIOBuf[i]);
	// ...and last byte in string as with EOI marker.
	m_iec.sendEOI(serCmdIOBuf[i]);
} // sendStatus


// send single basic line, including heading basic pointer and terminating zero.
void Interface::sendLine(byte len, char* text, word& basicPtr)
{
	byte i;

	// Increment next line pointer
	// note: minus two here because the line number is included in the array already.
	basicPtr += len + 5 - 2;

	// Send that pointer
	m_iec.send(basicPtr bitand 0xFF);
	m_iec.send(basicPtr >> 8);

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
	// Reset basic memory pointer:
	word basicPtr = C64_BASIC_START;
	noInterrupts();
	// Send load address
	m_iec.send(C64_BASIC_START bitand 0xff);
	m_iec.send((C64_BASIC_START >> 8) bitand 0xff);
	interrupts();
	// This will be slightly tricker: Need to specify the line sending protocol between Host and Arduino.
	// Call the listing function
	byte resp;
	do {
		COMPORT.write('L'); // initiate request.
		COMPORT.readBytes(serCmdIOBuf, 2);
		resp = serCmdIOBuf[0];
		if('L' == resp) { // Host system will give us something else if we're at last line to send.
			// get the length as one byte: This is kind of specific: For listings we allow 256 bytes length. Period.
			byte len = serCmdIOBuf[1];
			// WARNING: Here we might need to read out the data in portions. The serCmdIOBuf might just be too small
			// for very long lines.
			byte actual = COMPORT.readBytes(serCmdIOBuf, len);
			if(len == actual) {
				// send the bytes directly to CBM!
				noInterrupts();
				sendLine(len, serCmdIOBuf, basicPtr);
				interrupts();
			}
			else {
				resp = 'E'; // just to end the pain. We're out of sync or somthin'
				sprintf_P(serCmdIOBuf, (PGM_P)F("Expected: %d chars, got %d."), len, actual);
				Log(Error, FAC_IFACE, serCmdIOBuf);
			}
		}
		else {
			if('l' not_eq resp) {
				sprintf_P(serCmdIOBuf, (PGM_P)F("Ending at char: %d."), resp);
				Log(Error, FAC_IFACE, serCmdIOBuf);
				COMPORT.readBytes(serCmdIOBuf, sizeof(serCmdIOBuf));
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
	COMPORT.write('S'); // ask for file size.
	byte len = COMPORT.readBytes(serCmdIOBuf, 3);
	// it is supposed to answer with S<highByte><LowByte>
	if(3 not_eq len or serCmdIOBuf[0] not_eq 'S')
		return; // got some garbage response.
	word bytesDone = 0, totalSize = (((word)((byte)serCmdIOBuf[1])) << 8) bitor (byte)(serCmdIOBuf[2]);
#ifdef USE_LED_DISPLAY
	if(0 not_eq m_pDisplay)
		m_pDisplay->resetPercentage(totalSize);
#endif

	bool success = true;
	// Initial request for a bunch of bytes, here we specify the read size for every subsequent 'R' command.
	// This begins the transfer "game".
	COMPORT.write('N');											// ask for a byte/bunch of bytes
	COMPORT.write(MAX_BYTES_PER_REQUEST);		// specify the arduino serial library buffer limit for best performance / throughput.
	do {
		len = COMPORT.readBytes(serCmdIOBuf, 2); // read the ack type ('B' or 'E')
		if(2 not_eq len) {
			strcpy_P(serCmdIOBuf, (PGM_P)F("2 Host bytes expected, stopping"));
			Log(Error, FAC_IFACE, serCmdIOBuf);
			success = false;
			break;
		}
		resp = serCmdIOBuf[0];
		len = serCmdIOBuf[1];
		if('B' == resp or 'E' == resp) {
			byte actual = COMPORT.readBytes(serCmdIOBuf, len);
			if(actual not_eq len) {
				strcpy_P(serCmdIOBuf, (PGM_P)F("Host bytes expected, stopping"));
				success = false;
				Log(Error, FAC_IFACE, serCmdIOBuf);
				break;
			}
#ifdef EXPERIMENTAL_SPEED_FIX
			if('E' not_eq resp) // if not received the final buffer, initiate a new buffer request while we're feeding the CBM.
				COMPORT.write('R'); // ask for a byte/bunch of bytes
#endif
			// so we get some bytes, send them to CBM.
			for(byte i = 0; success and i < len; ++i) { // End if sending to CBM fails.
#ifndef EXPERIMENTAL_SPEED_FIX
				noInterrupts();
#endif
				if(resp == 'E' and i == len - 1)
					success = m_iec.sendEOI(serCmdIOBuf[i]); // indicate end of file.
				else
					success = m_iec.send(serCmdIOBuf[i]);
#ifndef EXPERIMENTAL_SPEED_FIX
				interrupts();
#endif
				++bytesDone;

#ifdef USE_LED_DISPLAY
				// Every xx bytes received, update the percentage.
				if(not (bytesDone % 32) and 0 not_eq m_pDisplay)
					m_pDisplay->showPercentage(bytesDone);
#endif
			}
#ifndef EXPERIMENTAL_SPEED_FIX
			if('E' not_eq resp) // if not received the final buffer, initiate a new buffer request while we're feeding the CBM.
				COMPORT.write('R'); // ask for a byte/bunch of bytes
#endif
		}
		else {
			strcpy_P(serCmdIOBuf, (PGM_P)F("Got unexp. cmd resp.char."));
			Log(Error, FAC_IFACE, serCmdIOBuf);
			success = false;
		}
	} while(resp == 'B' and success); // keep asking for more as long as we don't get the 'E' or something else (indicating out of sync).
	// If something failed and we have serial bytes in recieve queue we need to flush it out.
	if(not success and COMPORT.available()) {
		while(COMPORT.available())
			COMPORT.read();
	}
#ifdef USE_LED_DISPLAY
	if(0 not_eq m_pDisplay)
		m_pDisplay->showPercentage(bytesDone);
#endif
	if(success) {
		sprintf_P(serCmdIOBuf, (PGM_P)F("Transferred %u of %u bytes."), bytesDone, totalSize);
		Log(Success, FAC_IFACE, serCmdIOBuf);
	}
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
		} while(bytesInBuffer < sizeof(serCmdIOBuf) and not done);
		// indicate to media host that we want to write a buffer. Give the total length including the heading 'W'+length bytes.
		serCmdIOBuf[1] = bytesInBuffer;
		COMPORT.write((const byte*)serCmdIOBuf, bytesInBuffer);
		COMPORT.flush();
	} while(not done);

    Log(Information, 'M', "END");
} // saveFile


byte Interface::handler(void)
{
#ifdef HAS_RESET_LINE
	if(m_iec.checkRESET()) {
		// IEC reset line is in reset state, so we should set all states in reset.
		reset();
		return IEC::ATN_RESET;
	}
#endif
	noInterrupts();
	IEC::ATNCheck retATN = m_iec.checkATN(m_cmd);
	interrupts();

	if(retATN == IEC::ATN_ERROR) {
		strcpy_P(serCmdIOBuf, (PGM_P)F("ATNCMD: IEC_ERROR!"));
		Log(Error, FAC_IFACE, serCmdIOBuf);
		reset();
	}
	// Did anything happen from the host side?
	else if(retATN not_eq IEC::ATN_IDLE) {
		// A command is recieved, make cmd string null terminated
		m_cmd.str[m_cmd.strLen] = '\0';
#ifdef CONSOLE_DEBUG
		{
			sprintf_P(serCmdIOBuf, (PGM_P)F("ATN code:%d cmd: %s (len: %d) retATN: %d"), m_cmd.code, m_cmd.str, m_cmd.strLen, retATN);
			Log(Information, FAC_IFACE, serCmdIOBuf);
		}
#endif

		// lower nibble is the channel.
		byte chan = m_cmd.code bitand 0x0F;

		// check upper nibble, the command itself.
		switch(m_cmd.code bitand 0xF0) {
			case IEC::ATN_CODE_OPEN:
				// Open either file or prg for reading, writing or single line command on the command channel.
				// In any case we just issue an 'OPEN' to the host and let it process.
				// Note: Some of the host response handling is done LATER, since we will get a TALK or LISTEN after this.
				// Also, simply issuing the request to the host and not waiting for any response here makes us more
				// responsive to the CBM here, when the DATA with TALK or LISTEN comes in the next sequence.
				handleATNCmdCodeOpen(m_cmd);
			break;

			case IEC::ATN_CODE_DATA:  // data channel opened
				if(retATN == IEC::ATN_CMD_TALK) {
					 // when the CMD channel is read (status), we first need to issue the host request. The data channel is opened directly.
					if(CMD_CHANNEL == chan)
						handleATNCmdCodeOpen(m_cmd); // This is typically an empty command,
					handleATNCmdCodeDataTalk(chan); // ...but we do expect a response from PC that we can send back to CBM.
				}
				else if(retATN == IEC::ATN_CMD_LISTEN)
					handleATNCmdCodeDataListen();
				else if(retATN == IEC::ATN_CMD) // Here we are sending a command to PC and executing it, but not sending response
					handleATNCmdCodeOpen(m_cmd);	// back to CBM, the result code of the command is however buffered on the PC side.
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


void Interface::setDateTime(word year, byte month, byte day, byte hour, byte minute, byte second)
{
	// Querying the time after this moment will be limited to almost 50-day runtime (after that it will wrap around),
	// thats a limitation that we accept. If querying once between this day interval another 50 day limit is given
	// since time will be recalculated between each moment of query.
	m_timeOfSet = millis();
	m_year = year;
	m_month = month;
	m_day = day;
	m_hour = hour;
	m_minute = minute;
	m_second = second;
} // setDateTime


void Interface::updateDateTime()
{
	static const byte daysMonths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	ulong now = millis();
	ulong diff =  now - m_timeOfSet;
	// reset time stamp from this moment.
	m_timeOfSet = now;

	// now we have to count up (offset) the time with the elapsed millis since it was set last time.
	diff /= 1000; // get rid of millis, making it seconds
	m_second += diff % 60;
	if(m_second >= 60) {
		++m_minute;
		m_second -= 60;
	}
	diff /= 60; // get rid of seconds, making it minutes
	m_minute += diff % 60;
	if(m_minute >= 60) {
		++m_hour;
		m_minute -= 60;
	}
	diff /= 60; // get rid of minutes, making it hours
	m_hour += diff % 60;
	if(m_hour >= 24) {
		++m_day;
		m_hour -= 60;
	}
	diff /= 24; // get rid of hours, making it days
	m_day += diff;
	byte daysInMonth = daysMonths[m_month];
	if(0 == (m_year % 4))
		++daysInMonth;	 // it's a leap year
	if(m_day > daysInMonth) {
		++m_month;
		m_day -= daysInMonth;
	}
	if(m_month > 12) {
		++m_year;
		m_month -= 12;
	}
} // updateDateTime


char* Interface::dateTimeString(char* dest, bool timeOnly)
{
	updateDateTime();

	// format the string(s).
	if(not timeOnly)
		sprintf_P(dest, (PGM_P)F("%04u02u02u "), m_year, (word)m_month, (word)m_day);
	else
		dest[0] = '\0';
	byte timePos = strlen(dest);
	sprintf_P(&dest[timePos], (PGM_P)F("%02u02u02u"), (word)m_hour, (word)m_minute, (word)m_second);

	return dest;
} // dateTimeString


#ifdef USE_LED_DISPLAY
void Interface::setMaxDisplay(Max7219 *pDisplay)
{
	m_pDisplay = pDisplay;
} // setMaxDisplay
#endif


void Interface::handleATNCmdCodeOpen(IEC::ATNCmd& cmd)
{
	serCmdIOBuf[0] = 'O';
	serCmdIOBuf[2] = cmd.code bitand 0xF;
	byte length = 3;
	memcpy(&serCmdIOBuf[length], cmd.str, cmd.strLen);
	length += cmd.strLen;
	// Set the length so that receiving side know how much to read out.
	serCmdIOBuf[1] = length;
	// NOTE: Host side handles BOTH file open command AND the command channel command (from the cmd.code).
	COMPORT.write((const byte*)serCmdIOBuf, length);
} // handleATNCmdCodeOpen


void Interface::handleATNCmdCodeDataTalk(byte chan)
{
	byte lengthOrResult;
	boolean wasSuccess = false;

	// process response into m_queuedError.
	// Response: ><code in binary><CR>

	serCmdIOBuf[0] = 0;
	do {
		lengthOrResult = COMPORT.readBytes(serCmdIOBuf, 1);
	} while(lengthOrResult not_eq 1 or serCmdIOBuf[0] not_eq '>');

	if(not lengthOrResult or '>' not_eq serCmdIOBuf[0]) {
		m_iec.sendFNF();
		strcpy_P(serCmdIOBuf, (PGM_P)F("response not sync."));
		Log(Error, FAC_IFACE, serCmdIOBuf);
	}
	else {
		if(lengthOrResult = COMPORT.readBytes(serCmdIOBuf, 2)) {
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
				// FIXME: interface with Host for error info.
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
		lengthOrResult = COMPORT.readBytes(serCmdIOBuf, 1);
	} while(lengthOrResult not_eq 1 or serCmdIOBuf[0] not_eq '>');

	if(not lengthOrResult or '>' not_eq serCmdIOBuf[0]) {
		// FIXME: Check what the drive does here when things go wrong. FNF is probably not right.
		m_iec.sendFNF();
		strcpy_P(serCmdIOBuf, (PGM_P)F("response not sync."));
		Log(Error, FAC_IFACE, serCmdIOBuf);
	}
	else {
		if(lengthOrResult = COMPORT.readBytes(serCmdIOBuf, 2)) {
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
//		else // FIXME: Check what the drive does here when saving goes wrong. FNF is probably not right. Dummyread entire buffer from CBM?
//			m_iec.sendFNF();
	}
} // handleATNCmdCodeDataListen


void Interface::handleATNCmdClose()
{
	// handle close of file. Host system will return the name of the last loaded file to us.
	COMPORT.print("C");
	COMPORT.readBytes(serCmdIOBuf, 2);
	byte resp = serCmdIOBuf[0];
	if('N' == resp or 'n' == resp) { // N indicates we have a name. Case determines whether we loaded or saved data.
		// get the length of the name as one byte.
		byte len = serCmdIOBuf[1];
		byte actual = COMPORT.readBytes(serCmdIOBuf, len);
		if(len == actual) {
#ifdef USE_LED_DISPLAY
			serCmdIOBuf[len] = '\0';
			if('n' == resp)
				strcpy_P((char*)scrollBuffer, (PGM_P)F(" SAVED: "));
			else
				strcpy_P((char*)scrollBuffer, (PGM_P)F(" LOADED: "));
			strncat((char*)scrollBuffer, (char*)serCmdIOBuf, sizeof(scrollBuffer) - strlen((char*)scrollBuffer));

			if(0 not_eq m_pDisplay)
				m_pDisplay->resetScrollText(scrollBuffer);
#endif
		}
		else {
			sprintf_P(serCmdIOBuf, (PGM_P)F("Exp: %d chars, got %d."), len, actual);
			Log(Error, FAC_IFACE, serCmdIOBuf);
		}
	}
	else if('C' == resp) {
		if(m_iec.deviceNumber() not_eq serCmdIOBuf[1])
			m_iec.setDeviceNumber(serCmdIOBuf[1]);
	}
} // handleATNCmdClose
