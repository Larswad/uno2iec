#ifndef T64DRIVER_H
#define T64DRIVER_H

#include "filedriverbase.hpp"

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


class T64 : public FileDriverBase
{
public:
	// Host file system D64-image can be opened from constructor, if specified.
	T64(const QString& fileName = QString());
	virtual ~T64();

	const QString& extension() const
	{
		static const QString ext("T64");
		return ext;
	} // extension

	// The host file system T64 image is opened or re-opened with the method below.
	// If cannot be opened or not a T64 image, it returns false, true otherwise.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	// t64 driver api
	//
	// It can be checked if the disk image is considered OK:
	FSStatus status(void) const;

	bool sendListing(ISendLine& cb);

	bool supportsListing() const
	{
		return true;
	}

	// Open a file by filename: Returns true if successfull
	bool fopen(char* filename);
	//
	// Get character from open file:
	char getc(void);

	//
	// Returns true if last character was retrieved:
	bool isEOF(void) const;

	//
	// Close current file
	bool close(void);

private:

	typedef struct {
		uchar c64sFileType;
		uchar d64FileType;
		uchar startAddressLo;
		uchar startAddressHi;
		uchar endAddressLo;
		uchar endAddressHi;
		uchar unused[2];
		ulong fileOffset;
		uchar unused2[4];
		uchar fileName[16];
	} DirEntry; // 32 bytes

	// The real host file system D64 image file:
	QFile m_hostFile;

	// Status of the t64 driver:
	uchar m_status;

	// T64 driver state variables:

	ushort m_dirEntries;
	ushort m_dirEntry;

	// file status
	uchar  m_fileStartAddress[2]; // first two basic bytes
	ushort m_fileOffset;           // progress in file
	ushort m_fileLength;

	qint32 hostSize() const
	{
		return static_cast<qint32>(m_hostFile.size());
	}
	uchar hostReadByte(uint length = 1);
	bool hostSeek(qint32 pos, bool relative = false);

	bool getDirEntry(DirEntry& dir);
	bool seekFirstDir(void);
	void seekToTapeName(void);
};

#endif
