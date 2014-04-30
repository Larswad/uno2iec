#include "nativefs.hpp"
#include "logger.hpp"
#include <QDir>
#include <math.h>

using namespace Logging;

namespace {
const QString strDir("DIR");
const QString strPrg("PRG");
}

NativeFS::NativeFS()
	: m_listDirectories(false)
{
} // ctor


void NativeFS::setListingFilters(const QString &filters, bool listDirectories)
{
	m_filters = filters;
	m_listDirectories = listDirectories;
} // setListingFilters


bool NativeFS::mountHostImage(const QString& fileName)
{
	return fopen(fileName);
} // mountHostImage


void NativeFS::unmountHostImage()
{
	if(not m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
		m_hostFile.close();
	m_status = NOT_READY;
} // unmountHostImage


bool NativeFS::fopen(const QString& fileName)
{
	unmountHostImage();
	m_hostFile.setFileName(fileName);
	bool success = m_hostFile.open(QIODevice::ReadOnly);
	m_status = success ? FILE_OPEN : NOT_READY;

	return success;
} // fopen


CBM::IOErrorMessage NativeFS::fopenWrite(const QString &fileName, bool replaceMode)
{
	unmountHostImage();
	m_hostFile.setFileName(fileName);
	if(m_hostFile.exists() and not replaceMode)
		return CBM::ErrFileExists;
	bool success = m_hostFile.open(QIODevice::WriteOnly);
	m_status = success ? FILE_OPEN : NOT_READY;

	// We don't need to write a header or anything special here since the .PRG
	// format is exactly that what the CBM writes.
	CBM::IOErrorMessage retCode;
	if(success)
		retCode = CBM::ErrOK;
	else {
		if(m_hostFile.error() == QFile::PermissionsError or m_hostFile.error() == QFile::OpenError)
			retCode = CBM::ErrWriteProtectOn;
		else
			retCode = CBM::ErrFileNotOpen;
	}
	return retCode;
} // fopenWrite


char NativeFS::getc()
{
	char theByte;
	qint64 numRead(m_hostFile.read(&theByte, 1));
	if(numRead < 1) // shouldn't happen?
		m_status = FILE_EOF;

	return theByte;
} // getc


const QString NativeFS::openedFileName() const
{
	return m_hostFile.fileName();
} // openedFileName


ushort NativeFS::openedFileSize() const
{
	return m_hostFile.size();
} // openedFileSize


bool NativeFS::fileExists(const QString &filePath)
{
	return QFile::exists(filePath);
} // fileExists


CBM::IOErrorMessage NativeFS::renameFile(const QString &oldName, const QString &newName)
{
	return QFile::rename(oldName, newName) ? CBM::ErrOK : CBM::ErrFileNotFound;
} // renameFile


bool NativeFS::deleteFile(const QString &fileName)
{
	return QFile::remove(fileName);
} // deleteFile


bool NativeFS::isEOF() const
{
	return m_hostFile.atEnd();
} // isEOF


bool NativeFS::putc(char c)
{
	qint64 written = m_hostFile.write(&c, 1);
	return 1 == written;
} // putc


bool NativeFS::close()
{
	unmountHostImage();
	return true;
} // close


CBM::IOErrorMessage NativeFS::copyFiles(const QStringList &sourceNames, const QString &destName)
{
	QFile destFile(destName);
	if(not destFile.open(QFile::WriteOnly))
		return CBM::ErrWriteProtectOn; // TODO: Maybe find out better reason for error.

	// copy (append) each file from the list.
	foreach(const QString& source, sourceNames) {
		QFile sourceFile(source);
		if(not sourceFile.open(QFile::ReadOnly)) {
			destFile.close();
			destFile.remove();
			return CBM::ErrFileNotFound;
		}
		destFile.write(sourceFile.readAll());
		sourceFile.close();
	}
	destFile.close();
	return CBM::ErrOK;
} // copyFiles


bool NativeFS::sendListing(ISendLine& cb)
{
	QDir dir(QDir::current());
	QString dirName(dir.dirName().toUpper());
	dirName.truncate(23);
	dirName = dirName.leftJustified(23);

	QStringList filters = m_filters.split(',', QString::SkipEmptyParts);

	QString line("\x12\""); // Invert face, "
	QString diskLabel("NATIVE FS");
	uchar i;
	for(i = 2; i < 25; i++) {
		uchar c = dirName.at(0).toLatin1();
		dirName.remove(0, 1);

		if(0xA0 == c) // Convert padding A0 to spaces
			c = ' ';

		if(18 == i)   // Ending "
			c = '"';

		line += c;
	}
	// Ending name with dbl quotes
	line[i] = QChar('"').toLatin1();

	cb.send(0, line);

	QFileInfoList list(dir.entryInfoList(filters, QDir::NoDot bitor QDir::Files
																			 bitor (m_listDirectories ? QDir::AllDirs : QDir::Files), QDir::Name bitor QDir::DirsFirst));

	Log("NATIVEFS", info, QString("Listing %1 entrie(s) to CBM.").arg(QString::number(list.count())));
	while(not list.isEmpty()) {
		QFileInfo entry = list.first();
		list.removeFirst();
		line = "   \"";
		line.append(entry.fileName().toUpper());
		line.append("\" ");
		int spaceFill = 16 - entry.fileName().length();
		if(spaceFill > 0) {
			QString spaceAdd(spaceFill, ' ');
			line += spaceAdd;
		}
		if(entry.isDir())
			line.append(strDir);
		else
			line.append(strPrg);

		ushort fileSize = entry.size() / 1024;
		// Send initial spaces (offset) according to file size
		cb.send(fileSize, line.mid((int)log10((double)fileSize)));
	}

	return true;
} // sendListing


////////////////////////////////////////////////////////////////////////////////
//
// Send SD info function

bool NativeFS::sendMediaInfo(ISendLine &cb)
{
	// TODO: Improve this with information about the file system type AND, usage and free data.
	Log("NATIVEFS", info, "sendMediaInfo.");
	cb.send(0, QString("NATIVE FS ACTIVE."));
	cb.send(1, QString("CURRENT DIR: %1").arg(QDir::currentPath().toUpper()));
	cb.send(2, "HELLO FROM ARDUINO!");

	return true;
} // sendMediaInfo


FileDriverBase::FSStatus NativeFS::status() const
{
	return FileDriverBase::status();
} // status


bool NativeFS::setCurrentDirectory(const QString& dir)
{
	bool wasSuccess = QDir::setCurrent(dir);
	if(wasSuccess)
		Log("NATIVEFS", success, QString("Changing current directory to: %1").arg(QDir::currentPath()));
	else
		Log("NATIVEFS", warning, QString("Failed changing current directory to: %1 (this may be just OK)").arg(dir));

	return wasSuccess;
} // setCurrentDirectory


// Command to the command channel.
CBM::IOErrorMessage NativeFS::cmdChannel(const QString& cmd)
{
	Q_UNUSED(cmd);
	return CBM::ErrDriveNotReady;
} // cmdChannel
