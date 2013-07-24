#include "interface.hpp"
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
#include <QStringList>
#include <logger.hpp>

using namespace Logging;

#define CBM_BACK_ARROW 95
#define CBM_EXCLAMATION '!'
namespace {
const QString FAC_IFACE("IFACE");

// The previous cmd is copied to this string:
//char oldCmdStr[IEC::ATN_CMD_MAX_LENGTH];

const QStringList s_IOErrorMessages = QStringList()
		<< "00,OK"
		<< "21,READ ERROR"
		<< "26,WRITE PROTECT ON"
		<< "33,SYNTAX ERROR"
		<< "62,FILE NOT FOUND"
		<< "63,FILE EXISTS"
		<< "73,UNO2IEC DOS V0.1"
		<< "74,DRIVE NOT READY"
		<< "75,RPI SERIAL ERR.";
const QString s_unknownMessage = "99, UNKNOWN ERROR";
const QString s_errorEnding = ",00,00";

}


Interface::Interface(QextSerialPort& port)
	: m_currFileDriver(0), m_port(port), m_queuedError(ErrOK), m_openState(O_NOTHING), m_pListener(0)
{
	m_fsList.append(&m_native);
	m_fsList.append(&m_d64);
	m_fsList.append(&m_t64);
//	m_fsList.append(&m_m2i);
	m_currFileDriver = &m_native;
} // ctor


void Interface::reset()
{
	m_currFileDriver = &m_native;
	m_queuedError	= ErrOK;
	m_openState = O_NOTHING;
	m_dirListing.empty();
	m_lastCmdString.clear();
	foreach(FileDriverBase* fs, m_fsList)
		fs->closeHostFile(); // TODO: Better with a reset or init method on all file systems.
} // reset


void Interface::setMountNotifyListener(IMountNotify* pListener)
{
	m_pListener = pListener;
} // setMountNotifyListener


// This function crops cmd string of initial @ or until :
// Returns true if @ was detected
bool Interface::removeFilePrefix(QString& cmd)
{
	bool ret = false;

	// crop all from ':' and afterwards.
	int cropPos = cmd.indexOf(QChar(':'));
	if(cmd.at(0) == QChar('@')) {
		ret = true;
		if(-1 == cropPos)
			cropPos = 0;
	}

	if(-1 not_eq cropPos)
		cmd = cmd.mid(cropPos + 1);

	return ret;
} // removeFilePrefix


// Parse LOAD command, open either file/directory/d64/t64
//
void Interface::openFile(const QString& cmdString, bool localImageSelection)
{
	QString cmd(cmdString);

	if(localImageSelection) {// for this we have to fall back to nativeFS driver first.
		m_currFileDriver->closeHostFile();
		m_currFileDriver = &m_native;
	}
	// fall back result
	m_openState = O_NOTHING;

	// Check double back arrow first
	if((CBM_BACK_ARROW == cmd.at(0).toLatin1()) and (CBM_BACK_ARROW == cmd.at(1))) {
		// reload sdcard and send info
		m_currFileDriver = &m_native;
		m_openState = m_currFileDriver->supportsMediaInfo() ? O_INFO : O_NOTHING;
		Log(FAC_IFACE, "Going back to NativeFS and sending media info.", info);
	}
	else if((CBM_EXCLAMATION == cmd.at(0).toLatin1()) and (CBM_EXCLAMATION == cmd.at(1))) {
		// to get the media info for any OTHER media, the '!!' should be used on the CBM side.
		// whatever file system we have active, check if it supports media info.
		m_openState = m_currFileDriver->supportsMediaInfo() ? O_INFO : O_NOTHING;
	}
// Commented out here: Not applicable on pi really. However, some kind of sane check of fs might be done anyway...hmmm.
//	else if(!sdCardOK or !(fatGetStatus() & FAT_STATUS_OK)) {
//		// User tries to open stuff and there is a problem. Status is fail
//		m_queuedError = ErrDriveNotReady;
//		m_currFileDriver = 0;
//	}
	else if(cmd.at(0) == QChar('$')) {
		// Send directory listing of current dir
		m_openState = O_DIR;
	}
	else if(CBM_BACK_ARROW == cmd.at(0)) {
		// Exit current file format or cd..
		if(m_currFileDriver == &m_native) {
			m_currFileDriver->setCurrentDirectory("..");
			m_openState = O_DIR;
		}
		else if(0 not_eq m_currFileDriver) {
			// We are in some other state, exit to FAT and show current dir
			m_currFileDriver = &m_native;
			m_openState = O_DIR;
		}
	}
	else {
		// It was not any special command, remove eventual CBM dos prefix
		if(removeFilePrefix(cmd)) {
			// @ detected, this means save with replace:
			m_openState = O_SAVE_REPLACE;
		}
		else {
			// open file depending on interface state
			if(m_currFileDriver == &m_native) {
				// Exchange 0xFF with tilde to allow shortened long filenames
				// HMM: This is DOS mumble jumble, we're on a linux FS...will not work.
				cmd.replace(QChar(0xFF), "~");

				// Try if cd works, then try open as file and if none of these ok...then give up
				if(m_native.setCurrentDirectory(cmd)) {
					Log(FAC_IFACE, QString("Changed to native FS directory: %1").arg(cmd), success);
					m_openState = O_DIR;
				}
				else if(m_native.openHostFile(cmd)) {
					// File opened, investigate filetype
					QList<FileDriverBase*>::iterator i;
					for(i = m_fsList.begin(); i < m_fsList.end(); ++i) {
						// if extension matches last three characters in any file system, then we set that filesystem into use.
						if(!(*i)->extension().isEmpty() and cmd.endsWith((*i)->extension(), Qt::CaseInsensitive)) {
							m_currFileDriver = *i;
							break;
						}
					}

					if(i not_eq m_fsList.end()) {
						m_native.closeHostFile();
						Log(FAC_IFACE, QString("Trying image mount using driver: %1").arg(m_currFileDriver->extension()), info);
						// file extension matches, change interface state
						// call new format's reset
						if(m_currFileDriver->openHostFile(cmd)) {
							// see if this format supports listing, if not we're just opening as a file.
							if(!m_currFileDriver->supportsListing())
								m_openState = O_FILE;
							else // otherwise we're in directory mode now on this selected file system image.
								m_openState = O_DIR;
							Log(FAC_IFACE, QString("Mount OK of image: %1").arg(m_currFileDriver->openedFileName()), success);
						}
						else {
							// Error initializing driver, back to native file system.
							Log(FAC_IFACE, QString("Error initializing driver for FS.ext: %1. Going back to native.").arg(m_currFileDriver->extension()), error);
							m_currFileDriver = &m_native;
							m_openState = O_FILE_ERR;
						}
					}
					else { // No specific file format for this file, stay in FAT and send as straight .PRG
						Log(FAC_IFACE, QString("No specific file format for the names extension: %1, assuming .PRG in native file mode.").arg(cmd), info);
						m_openState = O_FILE;
					}
				}
				else { // File not found, giveup.
					Log(FAC_IFACE, QString("File %1 not found. Returning FNF to CBM.").arg(cmd), warning);
					m_openState = O_NOTHING;
				}
			}
			else if(0 not_eq m_currFileDriver) {
				// Call file format's own open command
				Log(FAC_IFACE, QString("Trying open FS file: %1 on FS: %2").arg(cmd).arg(m_currFileDriver->extension()), info);
				if(m_currFileDriver->fopen(cmd))
					m_openState = O_FILE;
				else // File not found
					m_openState = O_NOTHING;
			}
		}
	}

	if(!localImageSelection) {
		// Remember last cmd string.
		m_lastCmdString = cmd;
		if(O_INFO == m_openState or O_DIR == m_openState)
			buildDirectoryOrMediaList();
	}
} // openFile


void Interface::processOpenCommand(const QString& cmd, bool localImageSelectionMode)
{
	// Request: <channel>|<command string>
	Log(FAC_IFACE, cmd, info);
	QStringList params(cmd.split('|'));
	if(params.count() < 2) // enough params?
		m_queuedError = ErrSerialComm;
	else {
		uchar chan = params.at(0).toInt();
		// Are we addressing the command channel?
		if(CMD_CHANNEL == chan) {
			// command channel command
			// (note: if any previous openfile command has given us an error, the 'current' file system to use is not defined and
			// therefore the command will fail, we don't even have the native fs to use for the open command).
			if(0 not_eq m_currFileDriver) {
				m_queuedError = m_currFileDriver->cmdChannel(params.at(1));
			}
			else
				m_queuedError = ErrDriveNotReady;
			if(!localImageSelectionMode) {
				// Response: ><code><CR>
				// The code return is according to the values of the IOErrorMessage enum.
				// send back m_queuedError to uno.
				QByteArray data;
				data.append('>');
				data.append((char)m_queuedError);
				data.append('\r');
				m_port.write(data);
				m_port.flush();
				Log(FAC_IFACE, QString("CmdChannel_Response code: %1").arg(QString::number(m_queuedError)), m_queuedError == ErrOK ? success : error);
			}
		}
		else {
			// ...otherwise it was a open file command, be optimistic
			m_openState = O_NOTHING;
			openFile(params.at(1), localImageSelectionMode);
			if(!localImageSelectionMode) {
				// Response: ><code><CR>
				// The code return is according to the values of the IOErrorMessage enum.
				QByteArray data;
				data.append('>');
				data.append((char)m_openState);
				data.append('\r');
				m_port.write(data);
				m_port.flush();
				bool fail = m_openState == O_NOTHING or m_openState == O_FILE_ERR;
				Log(FAC_IFACE, QString("Open_Response code: %1").arg(QString::number(m_openState)), fail ? error : success);
			}
		}
	}
} // processOpenCommand


void Interface::processCloseCommand()
{
	QString name = m_currFileDriver->openedFileName();
	QByteArray data;
	data.append('N').append((char)name.length()).append(name);
	m_port.write(data);
	m_port.flush();
	Log(FAC_IFACE, QString("Returning last opened file name: %1").arg(name), info);
} // processCloseCommand


void Interface::processGetOpenFileSize()
{
	ushort size = m_currFileDriver->openedFileSize();

	QByteArray data;
	data.append('S').append(size >> 8).append(size bitand 0xff);
	m_port.write(data.data(), data.size());
	m_port.flush();
	Log(FAC_IFACE, QString("Returning file size: %1").arg(QString::number(size)), info);
} // processGetOpenedFileSize


void Interface::processLineRequest()
{
	if(O_INFO == m_openState or O_DIR == m_openState) {
		if(m_dirListing.isEmpty()) {
			// last line was produced. Send back the ending char.
			m_port.write("l");
			m_port.flush();
			Log(FAC_IFACE, "Last directory line written to arduino.", success);
		}
		else {
			m_port.write(m_dirListing.first().data(), m_dirListing.first().size());
			m_port.flush();
			m_dirListing.removeFirst();
		}
	}
	else {
		// TODO: This is a strange error state. Maybe we should return something to CBM here.
		Log(FAC_IFACE, "Strange state.", error);
	}
} // processOpenCommand


void Interface::processReadFileRequest(void)
{
	QByteArray data;
	char theByte = m_currFileDriver->getc();
	data.append(m_currFileDriver->isEOF() ? 'E' : 'B');
	data.append(1);
	data.append(theByte);

	m_port.write(data.data(), data.size());
} // processReadFileRequest


void Interface::processWriteFileRequest(uchar theByte)
{
	m_currFileDriver->putc(theByte);
} // processReadFileRequest


void Interface::processErrorStringRequest(IOErrorMessage code)
{
	// the return message begins with ':' for sync.
	QByteArray retStr(1, ':');
	retStr.append(code >= s_IOErrorMessages.count() ? s_unknownMessage : s_IOErrorMessages.at(code));
	// append the common ending.
	retStr.append(s_errorEnding);
	// terminate with CR.
	retStr.append('\r');
	m_port.write(retStr);
	m_port.flush();
} // processErrorStringRequest


void Interface::send(short lineNo, const QString& text)
{
	QByteArray line(text.toLocal8Bit());
	// the line number is included with the line itself. It goes in with lobyte,hibyte.
	line.prepend(uchar((lineNo bitand 0xFF00) >> 8));
	line.prepend(uchar(lineNo bitand 0xFF));
	// length of it all is the first byte.
	line.prepend((uchar)text.size() + 2);
	// add the response byte.
	line.prepend('L');
	// add it to the total dirlisting array.
	m_dirListing.append(line);
} // send


void Interface::buildDirectoryOrMediaList()
{
	m_dirListing.clear();
	if(O_DIR == m_openState) {
	Log(FAC_IFACE, QString("Producing directory listing for FS: \"%1\"...").arg(m_currFileDriver->extension()), warning);
	if(!m_currFileDriver->sendListing(*this))
		Log(FAC_IFACE, QString("Directory listing indicated error. Still sending: %1 chars").arg(QString::number(m_dirListing.length())), warning);
	else
		Log(FAC_IFACE, QString("Directory listing ok (%1 lines). Ready waiting for line requests from arduino.").arg(m_dirListing.count()), success);
	}
	else if(O_INFO == m_openState) {
		Log(FAC_IFACE, QString("Producing media info for FS: \"%1\"...").arg(m_currFileDriver->extension()), warning);
		if(!m_currFileDriver->sendMediaInfo(*this))
			Log(FAC_IFACE, QString("Media info listing indicated error. Still sending: %1 chars").arg(QString::number(m_dirListing.length())), warning);
		else
			Log(FAC_IFACE, QString("Media info listing ok (%1 lines). Ready waiting for line requests from arduino.").arg(m_dirListing.count()), success);
	}
} // buildDirectoryOrMediaList


bool Interface::changeNativeFSDirectory(const QString& newDir)
{
	return m_native.setCurrentDirectory(newDir);
} // changeNativeFSDirectory

////////////////////////////////////////////////////////////////////////////////
//
// File formats definition
//
/*
typedef  void(*PFUNC_SEND_LINE)(short line_no, unsigned char len, char *text);
typedef          void(*PFUNC_SEND_LISTING)(PFUNC_SEND_LINE send_line);
typedef          void(*PFUNC_VOID_VOID)(void);
typedef unsigned char(*PFUNC_UCHAR_VOID)(void);
typedef          char(*PFUNC_CHAR_VOID)(void);
typedef unsigned char(*PFUNC_UCHAR_CHAR)(char);
typedef          void(*PFUNC_VOID_CSTR)(char *s);
typedef unsigned char(*PFUNC_UCHAR_CSTR)(char *s);

#define FILE_FORMATS 5

struct file_format_struct {
	char extension[3];         // DOS extension of file format
	PFUNC_UCHAR_CSTR   init;   // intitialize driver, cmd_str is parameter,
	// must return true if OK
	PFUNC_SEND_LISTING send_listing; // Send listing function.
	PFUNC_UCHAR_CSTR   cmd;    // command channel command, must return ERR NUMBER!
	PFUNC_UCHAR_CSTR   open;   // open file
	PFUNC_UCHAR_CSTR   newfile;// create new empty file
	PFUNC_CHAR_VOID    getc;   // get char from open file
	PFUNC_UCHAR_VOID   eof;    // returns true if EOF
	PFUNC_UCHAR_CHAR   putc;   // write char to open file, return false if failure
	PFUNC_UCHAR_VOID   close;  // close file
};
*/

/*
const struct file_format_struct file_format[FILE_FORMATS] = {
	{{0,0,0}, &dummy_2, &fat_send_listing, &fat_cmd,
	 &fatFopen, &fat_newfile, &fatFgetc, &fatFeof, &fatFputc, &fatFclose},

	{{'D','6','4'}, &D64_reset, &D64_send_listing, &dummy_cmd,
	 &D64_fopen, &dummy_no_newfile, &D64_fgetc, &D64_feof, &dummy_1, &D64_fclose},

	{{'M','2','I'}, &M2I_init, &M2I_send_listing, &M2I_cmd,
	 &M2I_open, &M2I_newfile, &M2I_getc, &M2I_eof, &M2I_putc, &M2I_close},

	{{'T','6','4'}, &T64_reset, &T64_send_listing, &dummy_cmd,
	 &T64_fopen, &dummy_no_newfile, &T64_fgetc, &T64_feof, &dummy_1, &T64_fclose},

	{{'P','0','0'}, &P00_reset, 0, &dummy_cmd,
	 &dummy_2, &dummy_no_newfile, &fatFgetc, &fatFeof, &dummy_1, &close_to_fat}
};
*/

/*
const char pstr_file_err[] = "ERROR: FILE FORMAT";

void send_file_err(void (*send_line)(word lineNo, byte len, char* text))
{
	char buffer[19];

	memcpy(buffer, pstr_file_err, 18);
	(send_line)(0, 18, buffer);
} // send_file_err

*/

////////////////////////////////////////////////////////////////////////////////
//
// P00 file format implementation
//
/* TODO: Move as P00 format to PI, recode.
bool P00_reset(char *s)
{
	byte i;

	// Check filetype, and skip past 26 bytes
	if((fatFgetc() == 'C') and (fatFgetc() == '6') and (fatFgetc() == '4')) {

		for(i = 0; i < 23; i++)
			fatFgetc();

		if(!fatFeof())
			// All is ok
			return true;
	}

	return false;
} // P00_reset
*/

/* TODO: Move as P00 format to PI, recode.
bool close_to_fat(void)
{
	// Close and back to fat
	m_interfaceState = IS_NATIVE;
	fatFclose();
	return true;
} // close_to_fat
*/

////////////////////////////////////////////////////////////////////////////////
//
// Dummy functions for unused function pointers
//

/*
byte dummy_1(char c)
{
	return true;
}

byte dummy_2(char *s)
{
	return true;
}

byte dummy_cmd(char *s)
{
	return ErrWriteProtectOn;
}

byte dummy_no_newfile(char *s)
{
	return false;
}
*/

