#include "nativefs.hpp"
#include "logger.hpp"
#include <QDir>
#include <math.h>

using namespace Logging;

namespace {
const QString strDir(" <DIR>");
}

NativeFS::NativeFS()
{
} // ctor

bool NativeFS::openHostFile(const QString& fileName)
{
	return fopen(fileName);
} // openHostFile


void NativeFS::closeHostFile()
{
	if(!m_hostFile.fileName().isEmpty() and m_hostFile.isOpen())
		m_hostFile.close();
	m_status = NOT_READY;
} // closeHostFile


bool NativeFS::fopen(const QString& fileName)
{
	closeHostFile();
	m_hostFile.setFileName(fileName);
	bool isOpen = m_hostFile.open(QIODevice::ReadOnly);
	m_status = isOpen ? FILE_OPEN : NOT_READY;

	return isOpen;
} // fopen


bool NativeFS::newFile(const QString& fileName)
{
	Q_UNUSED(fileName);
	// TODO: Implement.
	return false;
} // newFile
/* TODO: Move this to PI NativeFS and recode, create new file.
byte fat_newfile(char *filename)
{
	// Remove and create file
	fatRemove(filename);
	return fatFcreate(filename);
} // fat_newfile
*/


char NativeFS::getc()
{
	char theByte;
	qint64 numRead(m_hostFile.read(&theByte, 1));
	if(numRead < 1) // shouldn't happen?
		m_status = FILE_EOF;

	return theByte;
} // getc


QString NativeFS::openedFileName() const
{
	return m_hostFile.fileName();
} // openedFileName


ushort NativeFS::openedFileSize() const
{
	return m_hostFile.size();
} // openedFileSize


bool NativeFS::isEOF() const
{
	return m_hostFile.atEnd();
} // isEOF


bool NativeFS::putc(char c)
{
	qint64 written = m_hostFile.write(&c, 1);
	// TODO: Implement.
	return 1 == written;
} // putc


bool NativeFS::close()
{
	closeHostFile();
	return true;
} // close


bool NativeFS::sendListing(ISendLine& cb)
{
	QDir dir(QDir::current());
	QString dirName(dir.dirName().toUpper());
	dirName.truncate(23);
	dirName = dirName.leftJustified(23);

	QStringList filters;
	filters.append("*.D64");
	filters.append("*.T64");
	filters.append("*.M2I");
	filters.append("*.PRG");
	filters.append("*.P00");

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

	QFileInfoList list(dir.entryInfoList(filters, QDir::NoDot bitor QDir::AllEntries, QDir::Name));
	Log("NATIVEFS", QString("Listing %1 entrie(s) to CBM.").arg(QString::number(list.count())), info);
	while(!list.isEmpty()) {
		QFileInfo entry = list.first();
		list.removeFirst();
		line = "   \"";
		line.append(entry.fileName().toUpper());
		if(entry.isDir())
			line.append(strDir + '"');
		else
			line.append('"');

		ushort fileSize = entry.size() / 1024;
		// Send initial spaces (offset) according to file size
		cb.send(fileSize, line.mid((int)log10(fileSize)));
	}

	return true;
} // sendListing


////////////////////////////////////////////////////////////////////////////////
//
// Send SD info function

bool NativeFS::sendMediaInfo(ISendLine &cb)
{
	// TODO: Improve this with information about the file system type AND, usage and free data.
	Log("NATIVEFS", "sendMediaInfo.", info);
	cb.send(0, QString("NATIVE FS ACTIVE -> XXX.")); // TODO: File system type instead of xxx.
	cb.send(1, QString("CURRENT DIR: %1").arg(QDir::currentPath().toUpper()));
	cb.send(2, "HELLO FROM ARDUINO UNO!");

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
		Log("NATIVEFS", QString("Changing current directory to: %1").arg(QDir::currentPath()), success);
	else
		Log("NATIVEFS", QString("Failed changing current directory to: %1 (this may be just OK)").arg(dir), warning);
	return wasSuccess;
} // setCurrentDirectory


// Command to the command channel.
IOErrorMessage NativeFS::cmdChannel(const QString& cmd)
{
	Q_UNUSED(cmd);
	return ErrDriveNotReady;
} // cmdChannel
// Fat parsing command channel
//
/* TODO: Move this to PI, Native file system command.
unsigned char fat_cmd(char *c)
{

	// Get command letter and argument

	char cmd_letter;
	char *arg, *p;
	char ret = ERR_OK;

	cmd_letter = cmd.str[0];

	arg = strchr(cmd.str,':');

	if(arg not_eq NULL) {
		arg++;

		if(cmd_letter == 'S') {
			// Scratch a file
			if(!fatRemove(arg))
				ret = ERR_FILE_NOT_FOUND;
		}
		else if(cmd_letter == 'R') {
			// Rename, find =  and replace with 0 to get cstr

			p = strchr(arg, '=');

			if(p not_eq NULL) {
				*p = 0;
				p++;
				if(!fatRename(p, arg)) {
					ret = ERR_FILE_NOT_FOUND;
				}
			}
		}
		else if(cmd_letter == 'N') {
			// Format new disk creates an M2I file
			ret = M2I_newdisk(arg);

			if(ret == ERR_OK) {
				// It worked, go to M2I state
				interface_state = I_M2I;
			}

		}
		else {
			ret = ErrSyntaxError;
		}
	}

	return ret;
}
*/
