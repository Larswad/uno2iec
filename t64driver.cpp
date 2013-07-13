//
// Title        : MMC2IEC - T64DRIVER
// Author       : Lars Pontoppidan
// Version      : 0.6
// Target MCU   : AtMega32(L) at 8 MHz
//
// DESCRIPTION:
// This module works on top of the FAT driver, providing access to files in an
// T64 tape image.
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
#include "t64driver.hpp"

//
// struct t64_header {
//
//   // uchar signature[32];
//
//   ushort versionNo;
//
//   uchar maxEntriesLo;
//   uchar maxEntriesHi;
//
//   uchar usedEntriesLo;
//   uchar usedEntriesHi;
//
//   uchar reserved[2];
//
//   uchar tapeName[24];
// }; // 32 bytes

// Signature section
#define T64_SIGNATURE_OFFSET 0

// Header section
#define T64_ENTRIES_LO_OFFSET 0x22
#define T64_ENTRIES_HI_OFFSET 0x23
#define T64_TAPE_NAME_OFFSET 0x28

// Dir section
#define T64_FIRST_DIR_OFFSET 0x40

#define OFFSET_PRE1 0xFFFE            //
#define OFFSET_PRE2 0xFFFF            //

namespace {
const QString strTapeEnd("TAPE END.");
}


T64::T64(const QString& fileName)
	:  FileDriverBase(), m_hostFile(fileName), m_dirEntries(0), m_dirEntry(0),
		m_fileOffset(0), m_fileLength(0)
{
	if(!fileName.isEmpty())
		openHostFile(fileName);
} // dtor


T64::~T64()
{
	closeHostFile();
} // dtor


bool T64::openHostFile(const QString& fileName)
{
	closeHostFile();
	m_hostFile.setFileName(fileName);
	// Analyse the file open in host file system and if it is a valid t64, set up
	// variables
	if(m_hostFile.open(QIODevice::ReadOnly)) {
		// Before going on, check filesize. This also checks if a file IS open
		if(hostSize() >= 64) {
			// Verify first three bytes of file signature:
			hostSeek(T64_SIGNATURE_OFFSET);

			if((hostReadByte() == 'C') and (hostReadByte() == '6') and (hostReadByte() == '4')) {
				// Read header, get dir information
				hostSeek(T64_ENTRIES_LO_OFFSET);
				m_dirEntries = (unsigned short)hostReadByte();
				hostSeek(T64_ENTRIES_HI_OFFSET);
				m_dirEntries or_eq (unsigned short)hostReadByte() << 8;

				// We are happy
				m_status = IMAGE_OK;
				return true;
			}
		}
	}
	// yikes.
	return false;
}

void T64::closeHostFile()
{
	if(!m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
		m_hostFile.close();
	// Reset status
	m_status = NOT_READY;
} // closeHostFile


uchar T64::hostReadByte(uint length)
{
	char theByte;
	qint64 numRead(m_hostFile.read(&theByte, length));
	if(numRead < length) // shouldn't happen?
		m_status = FILE_EOF;

	return theByte;
} // hostReadByte


bool T64::hostSeek(qint32 pos, bool relative)
{
	if(relative)
		pos += m_hostFile.pos();

	return m_hostFile.seek(pos);
} // hostSeek


void T64::seekToTapeName(void)
{
	if(m_status bitand IMAGE_OK) {
		hostSeek(T64_TAPE_NAME_OFFSET);

		// Now we have moved the file pointer, we are only tape_ok:
		m_status = IMAGE_OK;
	}
} // seekToTapeName


bool T64::isEOF(void) const
{
	return !(m_status bitand IMAGE_OK) or !(m_status bitand FILE_OPEN)
			or (m_status bitand FILE_EOF);
} // isEOF


// This function reads a character and updates file position to next
//
char T64::getc(void)
{
	uchar ret = 0;

	// Check status
	if(!isEOF()) {

		// Either give the first two address bytes, or load from file
		if(m_fileOffset == OFFSET_PRE1) {
			ret = m_fileStartAddress[0];
			m_fileOffset = OFFSET_PRE2;
		}
		else if(m_fileOffset == OFFSET_PRE2) {
			ret = m_fileStartAddress[1];
			m_fileOffset = 0;
		}
		else {
			ret = hostReadByte();
			m_fileOffset++;

			if(m_fileOffset == m_fileLength)
				m_status or_eq FILE_EOF;
		}
	}

	return ret;
} // fgetc


bool T64::seekFirstDir(void)
{
	if(m_status bitand IMAGE_OK) {
		// Seek first dir:
		hostSeek(T64_FIRST_DIR_OFFSET);

		// Set correct status
		m_status = IMAGE_OK bitor DIR_OPEN;
		m_dirEntry = 0;

		if(m_dirEntry == m_dirEntries)
			m_status or_eq DIR_EOF;

		return true;
	}
	return false;
} // seekFirstDir


bool T64::getDirEntry(DirEntry& dir)
{
	// Check if correct status
	if((m_status bitand IMAGE_OK) and (m_status bitand DIR_OPEN)
		 and !(m_status bitand DIR_EOF)) {

		uchar* pEntry = reinterpret_cast<uchar*>(&dir);
		// OK, copy current dir to target
		for(uchar i = 0; i < sizeof(DirEntry); i++)
			pEntry[i] = hostReadByte();

		m_dirEntry++;

		if(m_dirEntry == m_dirEntries)
			m_status or_eq DIR_EOF;

		return true;
	}
	return false;
} // getDirEntry


FileDriverBase::FSStatus T64::status(void) const
{
	return static_cast<FSStatus>(m_status);
} // status


// Opens a file. Filename * will open first file with PRG status
//
bool T64::fopen(char *filename)
{
	DirEntry dir;
	bool found = false;
	uchar len;

	len = strlen(filename);

	if(len > 16)
		len = 16;

	seekFirstDir();

	while(!found and getDirEntry(dir)) {
		// Acceptable filetype?
		if(0 not_eq dir.c64sFileType and 0 not_eq dir.d64FileType) {
			// Compare filename respecting * and ? wildcards
			found = true;
			uchar i;
			for(i = 0; i < len and found; i++) {
				if('?' == filename[i])
					; // This character is ignored
				else if('*' == filename[i]) // No need to check more chars
					break;
				else
					found = filename[i] == dir.fileName[i];
			}

			// If searched to end of filename, dir.file_name must end here also
			if(found and i == len)
				if(len < 16)
					found = ' ' == dir.fileName[i];
		}
	}

	if(found) {
		// File found. Set state vars and jump to file

		m_fileStartAddress[0] = dir.startAddressLo;
		m_fileStartAddress[1] = dir.startAddressHi;

		m_fileOffset = OFFSET_PRE1;

		m_fileLength = ((ushort)dir.endAddressLo
										bitor ((ushort)dir.endAddressHi << 8))
				- ((ushort)dir.startAddressLo
					 bitor ((ushort)dir.startAddressHi << 8));

		hostSeek(dir.fileOffset);

		m_lastOpenedFileName = filename;
		m_status = IMAGE_OK bitor FILE_OPEN;
	}

	return found;
} // fopen


QString T64::openedFileName() const
{
	return m_hostFile.fileName();
} // openedFileName


ushort T64::openedFileSize() const
{
	return m_fileLength;
} // openedFileSize


bool T64::close(void)
{
	m_status and_eq IMAGE_OK;  // Clear all flags except tape ok
	return true;
} // close


bool T64::sendListing(ISendLine& cb)
{
	seekToTapeName();

	QString name;
	for(uchar i = 0; i < 19; ++i) {
		uchar c = hostReadByte();
		name += 0xA0 == c ? ' ' : c; // Convert padding A0 to spaces
	}

	name[16] = '"'; // Ending quote
	cb.send(0, QString("\x12\"%1").arg(name));

	// Now for the list entries
	seekFirstDir();

	DirEntry dir;
	while(getDirEntry(dir)) {
		// Determine if dir entry is valid:
		if(0 not_eq dir.c64sFileType and 0 not_eq dir.d64FileType) {
			// Send filename, which is padded with spaces, line number is just zero.
			cb.send(0, QString("  \x22%1\x22").arg(reinterpret_cast<char*>(dir.fileName)));
		}
	}
	// Write line with TAPE_END
	cb.send(0, strTapeEnd);

	return true;
} // sendListing
