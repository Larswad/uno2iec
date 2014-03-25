#include "d64driver.hpp"
#include "logger.hpp"

#ifdef CONSOLE_DEBUG
#include <QDebug>
#endif
#include <math.h>

using namespace Logging;

namespace {

#define D64_BLOCK_SIZE 256  // Actual block size
#define D64_BLOCK_DATA 254  // Data capacity of block

#define D64_FIRSTDIR_TRACK  18
#define D64_FIRSTDIR_SECTOR 1

#define D64_BAM_TRACK  18
#define D64_BAM_SECTOR 0

#define D64_BAM_DISKNAME_OFFSET 0x90

#define D64_IMAGE_SIZE 174848

typedef struct {
		uchar disk_name[16]; // disk name padded with A0
		uchar disk_id[5];    // disk id and dos type
} D64DiskInfo;

// Sectors p. track table
// sectors_track[3] is the number of sectors in track 4 (!)
//
const uchar sectorsPerTrack[40] = {
		21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
		19,19,19,19,19,19,19,
		18,18,18,18,18,18,
		17,17,17,17,17,17,17,17,17,17
};

const QString strFileTypes[] = { "DEL", "SEQ", "PRG", "USR", "REL", "???" };
const QString strBlocksFree("BLOCKS FREE.");
const QString strD64Error("ERROR: D64");

} // anonymous


D64::D64(const QString& fileName)
		: FileDriverBase(), m_hostFile(fileName), m_currentTrack(0), m_currentSector(0), m_currentOffset(0),
				m_currentLinkTrack(0), m_currentLinkSector(0)
{
		if(not fileName.isEmpty())
				mountHostImage(fileName);
} // dtor


D64::~D64()
{
		unmountHostImage();
} // dtor


bool D64::mountHostImage(const QString& fileName)
{
		unmountHostImage();
		m_hostFile.setFileName(fileName);
		if(m_hostFile.open(QIODevice::ReadOnly)) {
				// Check if file is a valid disk image by the simple criteria that
				// file size is at least 174.848
				if(hostSize() >= D64_IMAGE_SIZE) {
						m_status = IMAGE_OK;
						m_lastName = QString("Image: ") + fileName;
						return true;
				}
		}
		m_lastName.clear();

		// yikes.
		return false;
} // mountHostImage


void D64::unmountHostImage()
{
		if(not m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
				m_hostFile.close();
		m_status = NOT_READY;
} // unmountHostImage


// This function sets the filepointer to third byte in a block.
//
// It also reads in link to next block, which is what the two first bytes
// contains
//
void D64::seekBlock(uchar track, uchar sector)
{
		ushort absSector;
		ulong absOffset;
		uchar i;

#ifdef CONSOLE_DEBUG
		//qDebug() << "seekblock: " << track << " " << sector << " ";
#endif

		// Change 1 based track notion to 0 based
		track--;

		//   // Sanity check track value
		//   if (track > 39) {
		//     m_status = 0;
		//     return;
		//   }
		//
		//   // Sanity check sector value
		//   if (sector > pgm_read_byte_near(&(sectors_track[track]))) {
		//     m_status = 0;
		//     return;
		//   }

		// Calculate absolute sector number
		absSector = sector;
		for(i = 0; i < track; i++)
				absSector += sectorsPerTrack[i];

		// Calculate absolute file offset
		absOffset = absSector * 256;

#ifdef CONSOLE_DEBUG
		//qDebug() << absOffset << endl;
#endif

		// Seek to that position if possible
		if(absOffset < static_cast<ulong>(hostSize())) {
				hostSeek(absOffset);

				// Read in link to next block
				m_currentLinkTrack = hostReadByte();
				m_currentLinkSector = hostReadByte();

				// We are done, update vars
				m_currentTrack = track;
				m_currentSector = sector;
				m_currentOffset = 2;
		}
		else // Track or sector value must have been invalid! Bad image
				m_status = 0;
} // seekBlock


bool D64::isEOF(void) const
{
		return not(m_status bitand IMAGE_OK) or not(m_status bitand FILE_OPEN)
						or (m_status bitand FILE_EOF);
} // isEOF


// This function reads a character and updates file position to next
//
char D64::getc(void)
{
		uchar ret = 0;

		// Check status
		if(not isEOF()) {
				ret = hostReadByte();

				if(255 == m_currentOffset) {
						// We need to go to a new block, end of file?
						if(m_currentLinkTrack not_eq 0) {
								// Seek the next block:
								seekBlock(m_currentLinkTrack, m_currentLinkSector);
						}
						else
								m_status or_eq FILE_EOF;

				}
				else
						m_currentOffset++;
		}

		return ret;
} // getc



bool D64::close(void)
{
		m_status and_eq IMAGE_OK;  // Clear all flags except disk ok

		return true;
} // fclose


bool D64::seekFirstDir(void)
{
		if(m_status bitand IMAGE_OK) {
				// Seek to first dir entry
				seekBlock(D64_FIRSTDIR_TRACK, D64_FIRSTDIR_SECTOR);

				// Set correct status
				m_status = IMAGE_OK bitor DIR_OPEN;

				return true;
		}
		return false;
} // seekFirstDir


bool D64::getDirEntry(DirEntry& dir)
{
		uchar i, j;
		uchar* pEntry = reinterpret_cast<uchar*>(&dir);

		// Check if correct status
		if(not ((m_status bitand IMAGE_OK) and (m_status bitand DIR_OPEN)
										and not(m_status bitand DIR_EOF)))
				return false;

		// OK, copy current dir to target
		for(i = 0; i < sizeof(DirEntry); i++) {
				pEntry[i] = hostReadByte();
				m_currentOffset++;
		}

		// Have we crossed a block boundry?
		if(0 == m_currentOffset) {
				// We need to go to a new block, end of directory chain?
				if(m_currentLinkTrack not_eq 0) {
						// Seek the next block:
						seekBlock(m_currentLinkTrack, m_currentLinkSector);
				}
				else
						m_status or_eq DIR_EOF;

		}
		else {
				// No boundry crossing, skip past two initial bytes of next dir
				i = hostReadByte();
				j = hostReadByte();
				m_currentOffset += 2;

				if(0 == i and 0xFF == j) {
						// No more dirs!
						m_status or_eq DIR_EOF;
				}
		}

		return true;
} // getDirEntry


bool D64::getDirEntryByName(D64::DirEntry &dir, const QString &name)
{
		// Now for the list entries
		seekFirstDir();
		while(getDirEntry(dir)) {
				if(name == dir.name()) // found it?
						return true;
		}
		// no match.
		return false;
} // getDirEntry


uchar D64::hostReadByte(uint length)
{
		char theByte;
		qint64 numRead(m_hostFile.read(&theByte, length));
		if(numRead < length) // shouldn't happen?
				m_status = FILE_EOF;

		return theByte;
} // hostReadByte


bool D64::hostSeek(qint32 pos, bool relative)
{
		if(relative)
				pos += m_hostFile.pos();

		return m_hostFile.seek(pos);
} // hostSeek


// At the position seeked comes:
//   16 chars of disk name (padded with A0)
//   2 chars of A0
//   5 chars of disk type
//
// Get these bytes directly from FAT by readHostByte();
void D64::seekToDiskName(void)
{
		if(m_status bitand IMAGE_OK) {
				// Seek BAM block
				seekBlock(D64_BAM_TRACK, D64_BAM_SECTOR);

				// Seek to disk name (-2 because seek_block already skips two bytes)
				hostSeek(D64_BAM_DISKNAME_OFFSET - 2, true);

				// Status now is no file open as such
				m_status = IMAGE_OK;
		}
} // seekToDiskName


ushort D64::blocksFree(void)
{
		// Not implemented yet
		return 0;
}

// Opens a file. Filename * will open first file with PRG status
//
bool D64::fopen(const QString& fileName)
{
		bool found = false;
		uchar len;
		uchar i;

		len = fileName.length();
		if(len > sizeof(m_currDirEntry.m_name))
				len = sizeof(m_currDirEntry.m_name);

		seekFirstDir();

		while(not found and getDirEntry(m_currDirEntry)) {

				// Acceptable filetype?
				i = m_currDirEntry.m_type bitand FILE_TYPE_MASK;
				if(SEQ == i or PRG == i) {

						// Compare filename respecting * and ? wildcards
						found = true;
						for(i = 0; i < len and found; i++) {
								if('?' == fileName[i]) {
										// This character is ignored
								}
								else if('*' == fileName[i]) {
										// No need to check more chars
										break;
								}
								else
										found = fileName[i] == m_currDirEntry.m_name[i];
						}

						// If searched to end of filename, dir.file_name must end here also
						if(found and (i == len))
								if(len < 16)
										found = m_currDirEntry.m_name[i] == 0xA0;

				}
		}

		if(found) {
				// File found. Jump to block and set correct state
				seekBlock(m_currDirEntry.track(), m_currDirEntry.sector());
				m_status = (FSStatus)(IMAGE_OK bitor FILE_OPEN);
				m_lastName = fileName;
		}
		else
				m_lastName.clear();

		return found;
} // fopen


const QString D64::openedFileName() const
{
		return m_lastName;
} // openedFileName


ushort D64::openedFileSize() const
{
		return m_currDirEntry.sizeBytes();
} // // openedFileSize


bool D64::sendListing(ISendLine& cb)
{
		if(not (m_status bitand IMAGE_OK)) {
				// We are not happy with the d64 file
				cb.send(0, strD64Error);
				return true;
		}

		// Send line with disc name and stuff, 25 chars
		seekToDiskName();

		QString line("\x12\x22"); // Invert face, "

		for(uchar i = 2; i < 25; i++) {
				uchar c = hostReadByte();

				if(0xA0 == c) // Convert padding A0 to spaces
						c = ' ';

				if(18 == i)   // Ending "
						c = '"';

				line += c;
		}
		cb.send(0, line);

		// Now for the list entries
		seekFirstDir();

		DirEntry dir;
		while(getDirEntry(dir)) {
				// Determine if dir entry is valid:
				if(dir.m_track not_eq 0) {
						// A direntry always takes 32 bytes total = 27 chars
						// Send filename until A0 or 16 chars
						QString name(17, QChar(' '));
						uchar i;
						for(i = 0; i < sizeof(dir.m_name); ++i) {
								uchar c = dir.m_name[i];
								if(0xA0 == c)
										break;  // Filename is no longer
								name[i] = c;
						}
						// Ending name with dbl quotes
						name[i] = QChar('"');

						// Write filetype
						uchar fileType = dir.m_type bitand FILE_TYPE_MASK;
						if(fileType > NumD64FileTypes)
								fileType = NumD64FileTypes; // Limit to Unknown type (???) when out of range.

						// Prepare buffer
						line = QString("   \"%1 %2%3%4").arg(name) // %s  %s%c%c
										.arg(strFileTypes[fileType])
										.arg((dir.m_type bitand FILE_LOCKED) ? '<' : ' ') // Perhaps write locked symbol
										.arg(not (dir.m_type bitand FILE_CLOSED) ? '*' : ' ');	// Perhaps write splat symbol

						// Line number is filesize in blocks:
						ushort fileSize = dir.m_blocksLo + (dir.m_blocksHi << 8);

						// Send initial spaces (offset) according to file size
						cb.send(fileSize, line.mid((int)log10((double)fileSize)));
				}
		}

		// Send line with 0 blocks free
		QString blkFree(QString(strBlocksFree) + QString(13, ' '));
		cb.send(0, blkFree);

		return true;
} // sendListing


bool D64::sendMediaInfo(ISendLine &cb)
{
		// TODO: Improve this with information about the file system type AND, usage and free data.
		Log("D64", info, "sendMediaInfo.");
		cb.send(0, QString("D64 FS -> %1").arg(m_hostFile.fileName().toUpper()));
		cb.send(1, QString("FILE SIZE: %1").arg(QString::number(m_hostFile.size())));
		seekFirstDir();
		ushort entryCnt = 0;
		DirEntry dir;
		while(getDirEntry(dir))
				entryCnt++;
		cb.send(2, QString("%1 ENTRIES IN IMAGE.").arg(QString::number(entryCnt)));

		return true;
} // sendMediaInfo


D64::DirEntry::DirEntry()
{
} // ctor


D64::DirEntry::~DirEntry()
{
} // dtor


CBM::IOErrorMessage D64::newDisk(const QString& name, const QString& id)
{
	return FileDriverBase::newDisk(name, id);
} // newDisk


QString D64::DirEntry::name() const
{
		return QString::fromLocal8Bit((const char*)(m_name));
} // getName


D64::FileType D64::DirEntry::type() const
{
		return static_cast<FileType>(m_type);
} // type


ushort D64::DirEntry::numBlocks() const
{
		return (m_blocksHi << 8) bitor m_blocksLo;
} // getNumBlocks


ushort D64::DirEntry::sizeBytes() const
{
		return numBlocks() * D64_BLOCK_DATA;
} // getSizeBytes


uchar D64::DirEntry::track() const
{
		return m_track;
} // getTrack


uchar D64::DirEntry::sector() const
{
		return m_sector;
} // getSector

/**
 * sectorsPerTrack - number of sectors on given track
 * @track: Track number
 *
 * Returns the number of sectors on the given track
 * of a 1541/71/81 disk. Invalid track numbers will return invalid results.
 */
ushort D64::xxxsectorsPerTrack(uchar track)
{
		Q_UNUSED(track);
		//	switch(m_header.imageType bitand IMAGE_TYPE_MASK)
		//	{
		//		case D81:
		//			return 40;

		//		case DNP:
		//			return 256;

		//		case D41:
		//		case D71:
		//		default:
		//			if(track > 35)
		//				track -= 35;
		//			if(track < 18)
		//				return 21;
		//			if(track < 25)
		//				return 19;
		//			if(track < 31)
		//				return 18;
		//			return 17;
		//	}
		return 0;
} // sectorsPerTrack
