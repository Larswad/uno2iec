// DESCRIPTION:
// This module implements support for the file format with extension .m2i
// With an .m2i file, it is possible to mimic an unlimited size diskette with
// read/write support and realistic filenames, while actually using nativs file system ("FAT") files.
//
// .m2i format description:
//
// <DISK TITLE (16 chars)><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
//
// file types:	P means prg file
//							D means del file
//							- means deleted file

#include <string.h>
#include <math.h>
#include <QTextStream>
#include <QFileInfo>
#include <QRegExp>
#include <QDir>

#include "m2idriver.hpp"
#include "logger.hpp"
#include "utils.hpp"

using namespace Logging;

// Local vars, driver state:

namespace {

const QString strM2IError("M2I FILE ERROR");
const QString strDEL("DEL");
const QString strDotPRG("PRG");
const QString strDotM2I("M2I");
const QString strM2IEnd("M2I END.");

const int TITLE_SIZE = 16;
const int NATIVENAME_SIZE = 12;
const int CBMNAME_SIZE = 16;

}


M2I::M2I()
{}

bool M2I::mountHostImage(const QString& fileName)
{
	unmountHostImage();

	// Interface has just opened the m2i file, save filename
	m_hostFile.setFileName(fileName);

	if(not m_hostFile.open(QIODevice::ReadOnly))
		return false;

	QTextStream in(&m_hostFile);
	bool isFirst = true;
	bool success = true;
	int lineNbr = 1;
	// Parse file.
	while(not in.atEnd() and success) {
		QString line(in.readLine());
		//		if(not line.endsWith("\r\n")) {
		//			Log("M2I", error, QString("Parsing file %1 at line %2 failed, disk title too long.").arg(fileName, QString::number(lineNbr)));
		//			success = false;
		//			continue;
		//		}
		//		// TODO: Not trim ALL whites here, only the ending \r\n
		line = line.trimmed();
		// first line is disk title, process separately.
		if(isFirst) {
			isFirst = false;
			if(line.length() > TITLE_SIZE) {
				Log("M2I", error, QString("Parsing file %1 at line %2 failed, disk title too long.").arg(fileName, QString::number(lineNbr)));
				success = false;
			}
			else
				m_diskTitle = line;
			++lineNbr;
			continue;
		}
		QStringList params(line.split(QChar(':')));
		if(3 not_eq params.size()) {
			Log("M2I", error, QString("Parsing file %1 at line %2 failed, more than 3 columns.").arg(fileName, QString::number(lineNbr)));
			success = false;
			continue;
		}
		FileEntry fe;
		QString strColumn(params.takeFirst());
		if(1 not_eq strColumn.length()) {
			Log("M2I", error, QString("Parsing file %1 at line %2 failed, file type not of single character.").arg(fileName, QString::number(lineNbr)));
			success = false;
			continue;
		}
		switch(strColumn[0].toUpper().toLatin1()) {
		case 'P':
			fe.fileType = FileEntry::TypePrg;
			break;
		case 'D':
			fe.fileType = FileEntry::TypeDel;
			break;
		case '-':
			fe.fileType = FileEntry::TypeErased;
			break;
		default:
			success = false;
			Log("M2I", error, QString("Parsing file %1 at line %2 failed, illegal file type: '%3'").arg(fileName
				, QString::number(lineNbr), strColumn));
			break;
		}
		if(not success)
			continue;
		fe.nativeName = params.takeFirst();
		// Being strict here: we stick to DOS 8.3 length, no more than that.
		if(fe.nativeName.length() > 12) {
			Log("M2I", error, QString("Parsing file %1 at line %2 failed, '%3' not DOS 8.3 length (max 12 chars)").arg(fileName
				, QString::number(lineNbr), fe.nativeName));
			success = false;
			continue;
		}
		fe.cbmName = params.takeFirst();
		// Being strict: CBM name not longer than 16 chars.
		if(fe.cbmName.length() <= 16)
			m_entries.append(fe);
		else {
			success = false;
			Log("M2I", error, QString("Parsing file %1 at line %2 failed, '%3' not CBM length (max 16 chars)").arg(fileName
				, QString::number(lineNbr), fe.cbmName));
		}
		++lineNbr;
	} // while

	// We close immediately as we're done, host file (.M2I) is only kept open during parsing (or writing).
	m_hostFile.close();
	m_status = success ? IMAGE_OK : NOT_READY;

	return success;
} // mountHostImage


void M2I::unmountHostImage()
{
	m_entries.clear();
	if(not m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
		m_hostFile.close();
	m_status = NOT_READY;
} // unmountHostImage


bool M2I::sendListing(ISendLine &cb)
{
	if(not (m_status bitand IMAGE_OK)) {
		// We are not happy with the m2i file
		cb.send(0, strM2IError);
		return false;
	}

	// First line is disk title
	cb.send(0, QString("\x12\x22%1\x22 %2").arg(m_diskTitle, strDotM2I));

	// Write lines
	foreach(const FileEntry& e, m_entries) {
		if(FileEntry::TypeDel == e.fileType or FileEntry::TypePrg == e.fileType) {
			QString name = '"' + e.cbmName + '"';
			QFile f(e.nativeName.trimmed());
			ushort fileSize = (ushort)(f.exists() ? f.size() : 0) / 256;
			QString line(QString("   %1%2").arg(name, -19, ' ').arg(FileEntry::TypePrg == e.fileType ? strDotPRG : strDEL));
			cb.send(fileSize, line.mid((int)log10((double)fileSize)));
		}
	}

	// Write last line with M2I_END
	cb.send(0, strM2IEnd);
	return true;
} // sendListing


bool M2I::deleteFile(const QString& fileName)
{
	FileEntry e;
	bool result = findEntry(fileName, e);
	if(result) {
		// only try removing native fs file if it is a prg.
		if(FileEntry::TypePrg == e.fileType) {
			QFile	f(QDir(QFileInfo(m_hostFile).absolutePath()).filePath(e.nativeName.trimmed()));
			result = f.remove() or !f.exists();
			if(result) {
				// remove from entry list as well.
				// NOTE: Here we can possible mark the entry as 'deleted' instead with FileEntry::Erased, but...why?
				result = m_entries.removeOne(e);
				// succeeded, so rewrite the updated M2I index file.
				if(result) {
					result = m_hostFile.open(QFile::WriteOnly);
					if(result) {
						m_hostFile.write(QByteArray().append(generateFile()));
						m_hostFile.close();
					}
					else
						Log("M2I", error, "Failed opening m2i container for writing.");
				}
				else
					Log("M2I", error, "Failed removing entry");
			}
		}
		else
			result = false;
	}

	return result;
} // deleteFile


CBM::IOErrorMessage M2I::renameFile(const QString& oldName, const QString& newName)
{
	FileEntry e;
	CBM::IOErrorMessage ret = CBM::ErrFileNotFound;
	bool result = findEntry(oldName, e, false);
	if(result and FileEntry::TypePrg == e.fileType) {
		// modify in-place instead of deleting and creating new entry.
		FileEntry& modEntry(m_entries[m_entries.indexOf(e)]);
		QFile	f(QDir(QFileInfo(m_hostFile).absolutePath()).filePath(modEntry.nativeName.trimmed()));

		modEntry.nativeName = newName.trimmed().left(NATIVENAME_SIZE);
		modEntry.cbmName = withoutExtension(newName.trimmed().left(CBMNAME_SIZE));
		// Do the physical renaming of the native file system file.
		if(f.rename(modEntry.nativeName)) {
			// operation succeeded, so rewrite the updated M2I index file.
			if(m_hostFile.open(QFile::WriteOnly)) {
				m_hostFile.write(QByteArray().append(generateFile()));
				m_hostFile.close();
				ret = CBM::ErrOK;
			}
			else
				ret = CBM::ErrFileNotOpen;
		}
	}
	return ret;
} // rename


bool M2I::fileExists(const QString& filePath)
{
	FileEntry e;
	return findEntry(filePath, e, false) and QFile(e.nativeName.trimmed().left(NATIVENAME_SIZE)).exists();
} // fileExists


CBM::IOErrorMessage M2I::newDisk(const QString& name, const QString& id)
{
	QFile file;
	// disk id not supported.
	Q_UNUSED(id);
	unmountHostImage();
	file.setFileName(name + ".M2I");
	// comment out to prevent overwrite existing m2i files.
//	if(file.exists())
//		return CBM::ErrFileExists;

	m_diskTitle = name.toUpper(); // TODO: Strip to disk name length
	bool success = file.open(QFile::WriteOnly);
	if(not success)
		return CBM::ErrWriteProtectOn;

	file.write(QByteArray().append(generateFile()));
	file.close();
	// remount if the file we have mounted is this one!
	if(not file.fileName().compare(m_hostFile.fileName(), Qt::CaseInsensitive))
		return mountHostImage(file.fileName()) ? CBM::ErrOK : CBM::ErrDriveNotReady;
	return CBM::ErrOK;
} // newDisk


bool M2I::fopen(const QString& fileName)
{
	m_status and_eq compl FILE_OPEN;
	FileEntry e;
	if(findEntry(fileName, e) and FileEntry::TypePrg == e.fileType) {
		QFileInfo f(m_hostFile);
		m_nativeFile.setFileName(QDir(f.absolutePath()).filePath(e.nativeName.trimmed()));
		// open the corresponding native name (dos 8.3 name).
		if(m_nativeFile.open(QFile::ReadOnly)) {
			m_status or_eq FILE_OPEN;
			m_openedEntry = e;
		}
	}

	return 0 not_eq (m_status bitand FILE_OPEN);
} // fopen


// The new function ensures filename exists and is empty.
//
CBM::IOErrorMessage M2I::fopenWrite(const QString& fileName, bool replaceMode)
{
	if(m_status bitand FILE_OPEN)
		close();
	QFileInfo f(m_hostFile);
	m_nativeFile.setFileName(QDir(f.absolutePath()).filePath(fileName.trimmed()));
	FileEntry e;
	// if file exists already, only accept if we're in replace mode.
	if((m_nativeFile.exists() or findEntry(fileName, e, false)) and not replaceMode)
		return CBM::ErrFileExists;

	bool success = m_nativeFile.open(QIODevice::WriteOnly bitor QIODevice::Truncate);
	if(success)
		m_status or_eq FILE_OPEN;
	CBM::IOErrorMessage ret;
	if(success) {
		success = m_hostFile.open(QFile::WriteOnly);
		if(success) {
			e.cbmName = withoutExtension(fileName.toUpper());
			e.fileType = FileEntry::TypePrg;
			e.nativeName = fileName;
			m_entries.append(e);
			m_hostFile.write(QByteArray().append(generateFile()));
			m_hostFile.close();
		}
		else
			close();
		ret = success ? CBM::ErrOK : CBM::ErrFileNotOpen;
	}
	else {
		if(m_hostFile.error() == QFile::PermissionsError or m_hostFile.error() == QFile::OpenError)
			ret = CBM::ErrWriteProtectOn;
		else
			ret = CBM::ErrFileNotOpen;
	}

	return ret;
} // fopenWrite


const QString M2I::openedFileName() const
{
	return m_openedEntry.cbmName.trimmed();
} // openedFileName


ushort M2I::openedFileSize() const
{
	return m_nativeFile.isOpen() ? m_nativeFile.size() : 0;
} // openedFileSize


char M2I::getc(void)
{
	char ret = 0;
	if(m_status bitand FILE_OPEN and not isEOF()) {
		m_nativeFile.read(&ret, 1);
		return ret;
	}
	return 0;
} // getc


// write char to open file, returns false if failure
bool M2I::putc(char c)
{
	return 1 == (m_status bitand FILE_OPEN) ? m_nativeFile.write(&c, 1) : 0;
} // putc


bool M2I::isEOF(void) const
{
	if(m_status bitand FILE_OPEN)
		return m_nativeFile.atEnd();
	return true;
} // isEOF


// close file
bool M2I::close(void)
{
	m_status and_eq compl FILE_OPEN;
	m_nativeFile.close();

	return true;
} // close


///////////////////////////////////////////////////////////////////////////////////////////////////
/// Private helpers
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Seek through M2I index.
/// findName: Name of the entry to search for (may contain ? or * wildcard character(s)).
/// entry: If found the entry will be placed in this reference.
/// return bool: true if entry was found.
bool M2I::findEntry(const QString& findName, FileEntry& entry, bool allowWildcards) const
{
	const QString trimmedFind(findName.trimmed());
	// trimming here is mostly for disregarding any ending blanks.
	QRegExp matcher(trimmedFind, Qt::CaseInsensitive, QRegExp::Wildcard);
	bool found = false;
	foreach(const FileEntry& e, m_entries) {
		if(/*FileEntry::TypePrg == e.fileType and */allowWildcards ? matcher.exactMatch(e.cbmName.trimmed())
			 : 0 == trimmedFind.compare(e.cbmName, Qt::CaseInsensitive)) {
			entry = e;
			found = true;
			break;
		}
	}
	return found;
} // findEntry


/// Generate the host index file from the current entry list.
/// Returns the file as a single QString, can be converted to QByteArray for writing to file.
const QString M2I::generateFile()
{
	// generate disktitle on first line.
	QString result(m_diskTitle + "\r\n");

	// generate file entries.
	foreach(const FileEntry& e, m_entries) {
		QChar typeChar;
		switch(e.fileType) {
		case FileEntry::TypePrg:
			typeChar = QChar('P');
			break;
		case FileEntry::TypeDel:
			typeChar = QChar('D');
			break;
		default:
			typeChar = QChar('-');
			break;
		}
		// pad both native file name and cbm dos name with spaces.
		result += QString("%1:%2:%3\r\n").arg(typeChar)
									.arg(e.nativeName, -NATIVENAME_SIZE, QChar(' '))
									.arg(e.cbmName, -CBMNAME_SIZE, QChar(' '));
	} // foreach

	return result;
} // generateFile
