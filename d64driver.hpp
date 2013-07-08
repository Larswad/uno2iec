#ifndef D64DRIVER_H
#define D64DRIVER_H

#include "filedriverbase.hpp"

//
// Title        : D64DRIVER
// Author       : Lars Pontoppidan
// Date         : Jan. 2007
// Version      : 0.5
// Target MCU   : AtMega32(L) at 8 MHz
//
// CREDITS:
// --------
// This D64DRIVER module is inspired from code in Jan Derogee's 1541-III project
// for PIC: http://jderogee.tripod.com/
// This code is a complete reimplementation though.
//
// DESCRIPTION:
// This module works on top of the FAT driver, providing access to files in an
// D64 disk image.
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//

// d64 driver api
//

class D64 : public FileDriverBase
{
public:
	enum D64FileType
	{
		DEL         = 0,
		SEQ         = 1,
		PRG         = 2,
		USR         = 3,
		REL         = 4,
		TYPE_MASK   = 0x07,
		FILE_LOCKED = 0x40,
		FILE_CLOSED = 0x80
	};

	// Host file system D64-image can be opened from constructor, if specified.
	D64(const QString& fileName = QString());
	virtual ~D64();

	const QString& extension() const
	{
		static const QString ext("D64");
		return ext;
	} // extension

	// The host file system D64 image is opened or re-opened with the method below.
	// If cannot be opened or not a D64 image, it returns false, true otherwise.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	// Open a file in the image by filename: Returns true if successful
	bool fopen(const QString& fileName);
	// Get character from open file:
	char getc(void);
	// Returns true if last character was retrieved:
	bool isEOF(void) const;
	// Close current file
	bool close(void);
	// Blocks free information
	ushort blocksFree(void);

	bool supportsListing() const
	{
		return true;
	}
	// Send realistic $ file basic listing, line by line
	bool sendListing(ISendLine& cb);

private:
	typedef struct {
		// uchar reserved1[2];  // track/sector of next direntry.
		// Note: only the very first direntry of a sector has this 'reserved' field.
		D64FileType type;
		uchar track;
		uchar sector;
		uchar name[16];
		uchar sideTrack;    // For REL files
		uchar sideSector;   // For REL files
		uchar recordLength; // For REL files
		uchar unused[6];    // Except for GEOS disks
		uchar blocksLo;     // 16-bit file size in blocks, multiply by
		uchar blocksHi;     // D64_BLOCK_DATA to get bytes
	} DirEntry;           // total of 30 bytes


	uchar hostReadByte(uint length = 1);
	bool hostSeek(qint32 pos, bool relative = false);
	qint32 hostSize() const
	{
		return static_cast<qint32>(m_hostFile.size());
	}

	void seekBlock(uchar track, uchar sector);
	bool seekFirstDir(void);
	bool getDirEntry(DirEntry& dir);
	void seekToDiskName(void);

	// The real host file system D64 file:
	QFile m_hostFile;

	// D64 driver state variables:
	// The current d64 file position described as track/sector/offset
	uchar m_currentTrack;
	uchar m_currentSector;
	uchar m_currentOffset;

	// The current block's link to next block
	uchar m_currentLinkTrack;
	uchar m_currentLinkSector;

};

#endif
