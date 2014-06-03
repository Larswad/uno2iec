#ifndef D64DRIVER_H
#define D64DRIVER_H

#include "filedriverbase.hpp"


class D64 : public FileDriverBase
{
public:
	enum FileType
	{
		DEL         = 0,
		SEQ         = 1,
		PRG         = 2,
		USR         = 3,
		REL         = 4,
		NumD64FileTypes,
		FILE_TYPE_MASK   = 0x07,
		FILE_LOCKED = 0x40,
		FILE_CLOSED = 0x80
	};

	// Disk image types - values must match G-P partition type byte
	enum ImageType
	{
		NONE = 0,
		DNP = 1,
		D41 = 2,
		D71 = 3,
		D81 = 4,
		NumD64ImageTypes,
		HAS_ERRORINFO = (1 << 7),
		IMAGE_TYPE_MASK = 0x7f
	};

	// Host file system D64-image can be opened from constructor, if specified.
	D64(const QString& fileName = QString());
	virtual ~D64();

	const QStringList& extension() const
	{
#if !(defined(__APPLE__) || defined(_MSC_VER))
		static const QStringList ext({ "D64" });
#else
		static QStringList ext;
		ext << "D64";
#endif
		return ext;
	} // extension

	// The host file system D64 image is opened or re-opened with the method below.
	// If cannot be opened or not a D64 image, it returns false, true otherwise.
	bool mountHostImage(const QString& fileName);
	void unmountHostImage();

	// Open a file in the image by filename: Returns true if successful
	bool fopen(const QString& fileName);
	// return the name of the last opened file (may not be same as fopen in case it resulted in something else, like when using wildcards).
	const QString openedFileName() const;
	// return the file size of the last opened file.
	ushort openedFileSize() const;
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
	// Whether this file system supports media info or not (true == supports it).
	bool supportsMediaInfo() const
	{
		return true;
	}
	// Send information about file system (whether it is OK, sizes etc.).
	bool sendMediaInfo(ISendLine &cb);

#ifdef _MSC_VER
#pragma pack(push, before_1, 1)
	class ImageHeader
		#else
	class __attribute__((packed)) ImageHeader
#endif
	{
		public:
		// Note: This is a POD class, may NOT contain any virtual methods or additional data than the DirType struct.

		ImageHeader();
		~ImageHeader();

		// Offsets in a D64 directory entry, also needed for raw dirs
#define DIR_OFS_FILE_TYPE       2
#define DIR_OFS_TRACK           3
#define DIR_OFS_SECTOR          4
#define DIR_OFS_FILE_NAME       5
#define DIR_OFS_SIZE_LOW        0x1e
#define DIR_OFS_SIZE_HI         0x1f
	};

#ifdef _MSC_VER
	class DirEntry
		#else
	class __attribute__((packed)) DirEntry
#endif
	{
		public:
		// Note: This is a POD class, may NOT contain any virtual methods or additional data than the DirType struct.

		DirEntry();
		~DirEntry();
		QString name() const;
		FileType type() const;
		ushort sizeBytes() const;
		ushort numBlocks() const;
		uchar track() const;
		uchar sector() const;

		// uchar reserved1[2];  // track/sector of next direntry.
		// Note: only the very first direntry of a sector has this 'reserved' field.
		uchar m_type; // D64FileType
		uchar m_track;
		uchar m_sector;
		uchar m_name[16];
		uchar m_sideTrack;    // For REL files
		uchar m_sideSector;   // For REL files
		uchar m_recordLength; // For REL files
		uchar m_unused[6];    // Except for GEOS disks
		uchar m_blocksLo;     // 16-bit file size in blocks, multiply by
		uchar m_blocksHi;     // D64_BLOCK_DATA to get bytes
	}; // total of 30 bytes
#ifdef _MSC_VER
#pragma pack(pop, before_1)
#endif
	// special commands.
	CBM::IOErrorMessage newDisk(const QString& name, const QString& id);

private:

	uchar hostReadByte(uint length = 1);
	bool hostSeek(qint32 pos, bool relative = false);
	qint32 hostSize() const
	{
		return static_cast<qint32>(m_hostFile.size());
	}

	ushort xxxsectorsPerTrack(uchar track);
	void seekBlock(uchar track, uchar sector);
	bool seekFirstDir(void);
	bool getDirEntry(DirEntry& dir);
	bool getDirEntryByName(DirEntry& dir, const QString& name);
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
	DirEntry m_currDirEntry;
	QString m_lastName;
};

#endif
