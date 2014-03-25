#include <string.h>
#include <math.h>
#include "t64driver.hpp"
#include "logger.hpp"

using namespace Logging;

// Data capacity of block. This is not really true, there are no "Blocks" in T64, it is only for presenting directory like a floppy.
#define T64_BLOCK_DATA 256

// Signature section
#define T64_SIGNATURE_OFFSET 0

// Header section
#define T64_ENTRIES_LO_OFFSET 0x22
#define T64_ENTRIES_HI_OFFSET 0x23
#define T64_TAPE_NAME_OFFSET 0x28

// Dir section
#define T64_FIRST_DIR_OFFSET 0x40

#define OFFSET_PRE1 0xFFFE            // "Magic" offsets used for keeping track of that the first bytes that should be returned
#define OFFSET_PRE2 0xFFFF            // when reading from the file is the basic start address (which is actually stored in the header and not as the first bytes of the file).

namespace {
const QString strTapeEnd("TAPE END.");
const QString strPrg("PRG");
}


T64::T64(const QString& fileName)
	:  FileDriverBase(), m_hostFile(fileName), m_dirEntries(0), m_dirEntry(0),
		m_fileOffset(0), m_fileLength(0)
{
	if(not fileName.isEmpty())
		mountHostImage(fileName);
} // dtor


T64::~T64()
{
	unmountHostImage();
} // dtor


bool T64::mountHostImage(const QString& fileName)
{
	unmountHostImage();
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
				m_lastOpenedFileName = QString("Image: ") + fileName;
				return true;
			}
		}
	}
	m_lastOpenedFileName.clear();

	// yikes.
	return false;
} // mountHostImage


void T64::unmountHostImage()
{
	if(not m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
		m_hostFile.close();
	// Reset status
	m_status = NOT_READY;
} // unmountHostImage


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
	return not (m_status bitand IMAGE_OK) or not (m_status bitand FILE_OPEN)
			or (m_status bitand FILE_EOF);
} // isEOF


// This function reads a character and updates file position to next
//
char T64::getc(void)
{
	uchar ret = 0;

	// Check status
	if(not isEOF()) {

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
		 and not(m_status bitand DIR_EOF)) {

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
bool T64::fopen(const QString& fileName)
{
	DirEntry dir;
	bool found = false; // be pessimistic
	uchar len;

	len = fileName.length();

	if(fileName.length() > (int)sizeof(dir.fileName))
		len = sizeof(dir.fileName);

	seekFirstDir();

	while(not found and getDirEntry(dir)) {
		// Acceptable filetype?
		if(0 not_eq dir.c64sFileType and 0 not_eq dir.d64FileType) {
			// Compare filename respecting * and ? wildcards
			found = true;
			uchar i;
			for(i = 0; i < len and found; i++) {
				if('?' == fileName.at(i))
					; // This character is ignored
				else if('*' == fileName.at(i)) // No need to check more chars
					break;
				else
					found = fileName.at(i) == dir.fileName[i];
			}

			// If searched to end of filename, dir.file_name must end here also
			if(found and i == len)
				if(len < sizeof(dir.fileName))
					found = ' ' == dir.fileName[i];
		}
	}

	if(found) {
		// File found. Set state vars and jump to file
		m_fileStartAddress[0] = dir.startAddressLo;
		m_fileStartAddress[1] = dir.startAddressHi;

		m_fileOffset = OFFSET_PRE1;
		m_fileLength = calcFileLength(dir);

		hostSeek(dir.fileOffset);

		m_lastOpenedFileName = fileName;
		m_status = IMAGE_OK bitor FILE_OPEN;
	}
	else
		m_lastOpenedFileName.clear();

	return found;
} // fopen


const QString T64::openedFileName() const
{
	return m_lastOpenedFileName;
} // openedFileName


ushort T64::calcFileLength(DirEntry dir)
{
	return ((ushort)dir.endAddressLo
					bitor ((ushort)dir.endAddressHi << 8))
			- ((ushort)dir.startAddressLo
				 bitor ((ushort)dir.startAddressHi << 8));
} // calcFileLength


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
			ushort fileBlocks = (calcFileLength(dir) + T64_BLOCK_DATA - 1) / T64_BLOCK_DATA;
			// Send filename, which is padded with spaces, line number is just zero.
			QString line = QString("  \"%1\" %2").arg(QString::fromLocal8Bit(reinterpret_cast<char*>(dir.fileName), sizeof(dir.fileName)), strPrg);

			cb.send(fileBlocks, line.mid((int)log10((double)fileBlocks)));
		}
	}
	// Write line with TAPE_END
	cb.send(0, strTapeEnd);

	return true;
} // sendListing


bool T64::sendMediaInfo(ISendLine &cb)
{
	// TODO: Improve this with information about the file system type AND, usage and free data.
	Log("T64", info, "sendMediaInfo.");
	cb.send(0, QString("T64 FS -> %1").arg(m_hostFile.fileName()));
	cb.send(1, QString("FILE SIZE: %1").arg(QString::number(m_hostFile.size())));
	cb.send(2, QString("%1 FILE(S) IN IMAGE.").arg(QString::number(m_dirEntries)));

	return true;
} // sendMediaInfo
