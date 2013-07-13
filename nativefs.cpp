#include "nativefs.hpp"
#include "logger.hpp"
#include <QDir>

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
	Log("NATIVEFS", "fopen: NOT YET IMPLEMENTED", warning);
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
	Log("NATIVEFS", "sendListing: NOT YET IMPLEMENTED", warning);
	cb.send(0, "LISTING NOT YET IMPLEMENTED.");
	Q_UNUSED(cb);
	QDir dir(".");

	return true;
} // sendListing
/* TODO: Move this to PI, Native file system listing.
void fat_send_listing(void (*send_line)(short line_no, unsigned char len, char *text))
{
	unsigned short s;
	unsigned char i;
	char buffer[24];

	struct diriterator di;
	struct direntry *de;

	// Prepare buffer
	memset(buffer, ' ', 3);
	buffer[3] = '"';   // quotes

	// Iterate through directory
	de = fatFirstDirEntry(fatGetCurDirCluster(), &di);

	while(de not_eq NULL) {
		if(*de->deName == SLOT_EMPTY)
			break; // there are no more direntries

		if((*de->deName not_eq SLOT_DELETED) and
			 (de->deAttributes not_eq ATTR_LONG_FILENAME)) {
			if(de->deAttributes == ATTR_VOLUME) {
				// Print volume label line. This will be 11 chars
				(send_line)(0, 11, de->deName);
			}
			else {
				// Regular file/directory
				if(de->deAttributes & ATTR_DIRECTORY) {
					// Its a dir
					memcpy(&(buffer[4]), de->deName, 8);    // name
					buffer[12] = ' ';                       // space
					memcpy(&(buffer[13]), de->deName+8, 3); // ext
					buffer[16] = 0x22;                      // quote
					memcpy_P(&(buffer[17]), pstr_dir, 6);   // line end

					(send_line)(0, 23, buffer);
				}
				else {
					// Its a file, calc file size in kB:
					s = (de->deFileSize + (1 << 10) - 1) >> 10;

					// Calc number of spaces required
					if(s > 9999) {
						s = 9999;
						i = 0;
					}
					else if(s >= 1000)
						i = 0;
					else if(s >= 100)
						i = 1;
					else if(s >= 10)
						i = 2;
					else
						i = 3;

					memcpy(&(buffer[4]), de->deName, 8);    // name
					buffer[12] = '.';                       // dot
					memcpy(&(buffer[13]), de->deName + 8, 3); // ext
					buffer[16] = '"';                      // quote

					(send_line)(s, 14 + i, &(buffer[3 - i]));
				}
			}
		}

		de = fatNextDirEntry(&di);
	}

	// Was this a natural ending??
	if(sdCardOK == FALSE) {
		// say  "ERROR: SD/MMC"
		memcpy_P(buffer, strSDState1, 13);
		(send_line)(0, 13, buffer);
	}
}
*/

////////////////////////////////////////////////////////////////////////////////
//
// Send SD info function

bool NativeFS::sendMediaInfo(ISendLine &cb)
{
	// TODO: Improve this with information about the file system type AND, usage and free data.
	Log("NATIVEFS", "sendMediaInfo.", info);
	cb.send(0, QString("SD CARD NATIVE FS -> EXT4."));
	cb.send(1, QString("Current Dir: %1").arg(QDir::currentPath()));
	cb.send(2, "HELLO FROM PI!");
	cb.send(3, "HELLO FROM ARDUINO UNO!");

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
		Log("NATIVEFS", QString("Failed changing current directory to: %1").arg(dir), error);
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
