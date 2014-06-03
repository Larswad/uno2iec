#ifndef T64DRIVER_H
#define T64DRIVER_H

#include "filedriverbase.hpp"


class T64 : public FileDriverBase
{
public:
	// Host file system D64-image can be opened from constructor, if specified.
	T64(const QString& fileName = QString());
	virtual ~T64();

	const QStringList& extension() const
	{
#if !(defined(__APPLE__) || defined(_MSC_VER))
		static const QStringList ext({ "T64" });
#else
		static QStringList ext;
		ext << "T64";
#endif
		return ext;
	} // extension

	// The host file system T64 image is opened or re-opened with the method below.
	// If cannot be opened or not a T64 image, it returns false, true otherwise.
	bool mountHostImage(const QString& fileName);
	void unmountHostImage();

	// t64 driver api
	//
	// It can be checked if the disk image is considered OK:
	FSStatus status(void) const;

	bool sendListing(ISendLine& cb);

	bool supportsListing() const
	{
		return true;
	}
	// Whether this file system supports media info or not (true == supports it).
	bool supportsMediaInfo() const
	{
		return true;
	}
	bool sendMediaInfo(ISendLine &cb);

	// Open a file by filename: Returns true if successfull
	bool fopen(const QString &fileName);
	const QString openedFileName() const;
	ushort openedFileSize() const;
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
#ifdef _MSC_VER
#pragma pack(push, before_1, 1)
	typedef struct
		#else
	typedef struct __attribute__((packed))
#endif
	{
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


#ifdef _MSC_VER
	typedef struct
		#else
	typedef struct __attribute__((packed))
#endif
	{
		uchar signature[32];
		ushort versionNo;
		uchar maxEntriesLo;
		uchar maxEntriesHi;
		uchar usedEntriesLo;
		uchar usedEntriesHi;
		uchar reserved[2];
		uchar tapeName[24];
	} Header; // 32 bytes

#ifdef _MSC_VER
#pragma pack(pop, before_1)
#endif
	// The real host file system D64 image file:
	QFile m_hostFile;

	// T64 driver state variables:

	ushort m_dirEntries;
	ushort m_dirEntry;

	// file status
	uchar  m_fileStartAddress[2]; // first two basic bytes
	ushort m_fileOffset;           // progress in file
	ushort m_fileLength;
	QString m_lastOpenedFileName;

	qint32 hostSize() const
	{
		return static_cast<qint32>(m_hostFile.size());
	}
	uchar hostReadByte(uint length = 1);
	bool hostSeek(qint32 pos, bool relative = false);

	bool getDirEntry(DirEntry& dir);
	ushort calcFileLength(DirEntry dir);
	bool seekFirstDir(void);
	void seekToTapeName(void);
};

#endif
