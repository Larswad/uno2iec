#include <QStringList>
#include <QDir>
#include <QDebug>

#include "interface.hpp"
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
#include "logger.hpp"
#include "doscommands.hpp"

using namespace Logging;

namespace {
const QString FAC_IFACE("IFACE");

// The previous cmd is copied to this string:
//char oldCmdStr[IEC::ATN_CMD_MAX_LENGTH];

// See error list in cbmdefines.h, these error codes must be mapped to the errors there.
// If an error code is not found it will return the "99, UNKNOWN ERROR" string.
const QStringList s_IOErrorMessages = QStringList()
		<< "00,OK"
		<< "01,FILES SCRATCHED"
		<< "20,READ ERROR"		// Block header not found
		<< "21,READ ERROR"		// no sync character
		<< "22,READ ERROR"		// data block not present
		<< "23,READ ERROR"		// checksum error in data block
		<< "24,READ ERROR"		// byte decoding error
		<< "25,WRITE ERROR"		// write-verify error
		<< "26,WRITE PROTECT ON"
		<< "27,READ ERROR"		// checksum error in header
		<< "28,WRITE ERROR"		// long data block
		<< "29,DISK ID MISMATCH"
		<< "30,SYNTAX ERROR"	// general syntax Forexample, two file names may appear on the left side of the COPY command.
		<< "31,SYNTAX ERROR"	// The DOS does not recognize the command. The command must start in the first position.
		<< "32,SYNTAX ERROR"	// The command sent is longer than 58 characters.
		<< "33,SYNTAX ERROR"	// Pattern matching is invalidly used in the OPEN or SAVE command.
		<< "34,SYNTAX ERROR"	// The file name was left out of a command or the DOS does not recognize it as such.
		<< "39,SYNTAX ERROR"	// This error may result if the command sent to command channel (secondary address 15) is unrecognized by the DOS.
		<< "50,RECORD NOT PRESENT"
		<< "51,OVERFLOW IN RECORD"
		<< "52,FILE TOO LARGE"
		<< "60,WRITE FILE OPEN"
		<< "61,FILE NOT OPEN"
		<< "62,FILE NOT FOUND"
		<< "63,FILE EXISTS"
		<< "64,FILE TYPE MISMATCH"
		<< "65,NO BLOCK"
		<< "66,ILLEGAL TRACK AND SECTOR"
		<< "67,ILLEGAL SYSTEM T OR S"
		<< "70,NO CHANNEL"
		<< "71,DIRECTORY ERROR"
		<< "72,DISK FULL"
		<< "73,UNO2IEC DOS V0.2"
		<< "74,DRIVE NOT READY"
		<< "97,RPI SERIAL ERR."	// Specific error to this emulated device, serial communication has gone out of sync.
		<< "98,NOT IMPLEMENTED";

const QString s_unknownMessage = "99,UNKNOWN ERROR";
const QString s_errorEnding = ",00,00";

} // anonymous


Interface::Interface(QextSerialPort& port)
	: m_currFileDriver(0), m_port(port)
	, m_queuedError(CBM::ErrOK)
	,	m_openState(O_NOTHING)
	, m_currReadLength(MAX_BYTES_PER_REQUEST)
	, m_pListener(0)
{
	// Build the list of implemented / supported file systems.
	m_fsList.append(&m_native);
	m_fsList.append(&m_d64);
	m_fsList.append(&m_t64);
	m_fsList.append(&m_m2i);
	m_fsList.append(&m_x00fs);

	// We have included the rom in our Qt resources.
	QFile romFile(":/roms/rom_1541");
	bool success = romFile.open(QIODevice::ReadOnly);
	if(not success)
		qDebug() << "couldn't open romfile: " << romFile.fileName();
	else {
		m_driveROM = romFile.readAll();
		romFile.close();
	}
	reset();
} // ctor


Interface::~Interface()
{} // dtor


void Interface::setImageFilters(const QString& filters, bool showDirs)
{
	m_native.setListingFilters(filters, showDirs);
} // setImageFilters


CBM::IOErrorMessage Interface::reset(bool informUnmount)
{
	// restore RAM and via areas.
	m_driveRAM.fill(0, CBM1541_RAM_SIZE);
	m_via1MEM.fill(0, CBM1541_VIA1_SIZE);
	m_via2MEM.fill(0, CBM1541_VIA2_SIZE);
	if(informUnmount and 0 not_eq m_pListener)
		m_pListener->imageUnmounted();
	m_currFileDriver = &m_native;
	m_openState = m_currFileDriver->supportsMediaInfo() ? O_INFO : O_NOTHING;
	m_dirListing.empty();
	m_lastCmdString.clear();
	foreach(FileDriverBase* fs, m_fsList)
		fs->closeHostFile(); // TODO: Better with a reset or init method on all file systems.
	if(0 not_eq m_pListener)
		m_pListener->deviceReset();
	m_queuedError = CBM::ErrIntro;

	return m_queuedError;
} // reset


void Interface::setMountNotifyListener(IFileOpsNotify* pListener)
{
	m_pListener = pListener;
} // setMountNotifyListener


// This function crops cmd string of initial @ or until :
// Returns true if @ was detected
bool Interface::removeFilePrefix(QString& cmd) const
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


void Interface::moveToParentOrNativeFS()
{
	// Exit current file format or cd..
	if(m_currFileDriver == &m_native) {
		m_currFileDriver->setCurrentDirectory("..");
		if(0 not_eq m_pListener) // notify UI listener of change.
			m_pListener->directoryChanged(QDir::currentPath());
		m_openState = O_DIR;
	}
	else if(0 not_eq m_currFileDriver) {
		// We are in some other state, exit to FAT and show current dir
		m_currFileDriver = &m_native;
		m_openState = O_DIR;
		if(0 not_eq m_pListener)
			m_pListener->imageUnmounted();
	}
} // moveToParentOrNativeFS


void Interface::readDriveMemory(ushort address, ushort length, QByteArray& bytes) const
{
	bytes.clear();
	if(/*address >= CBM1541_RAM_OFFSET and */address < CBM1541_RAM_OFFSET + m_driveRAM.size())
		bytes = m_driveRAM.mid(address, length);
	else if(address >= CBM1541_VIA1_OFFSET and address < CBM1541_VIA1_OFFSET + m_via1MEM.size())
		bytes = m_via1MEM.mid(address - CBM1541_VIA1_OFFSET, length);
	else if(address >= CBM1541_VIA2_OFFSET and address < CBM1541_VIA2_OFFSET + m_via2MEM.size())
		bytes = m_via2MEM.mid(address - CBM1541_VIA2_OFFSET, length);
	else if(address >= CBM1541_ROM_OFFSET and address < CBM1541_ROM_OFFSET + m_driveROM.size())
		bytes = m_driveROM.mid(address - CBM1541_ROM_OFFSET, length);

	// resize to be the expected length in case the read was done at memory borders.
	// It will be garbage data out of course, but that's to expect.
	if(bytes.size() < length)
		bytes.resize(length);
} // readDriveMemory


void Interface::writeDriveMemory(ushort address, const QByteArray& bytes)
{
	// When doing resize (chop) after replace its because if address + array goes outside memory.
	if(/*address >= CBM1541_RAM_OFFSET and */address < CBM1541_RAM_OFFSET + m_driveRAM.size()) {
		m_driveRAM.replace(address, bytes.size(), bytes);
		m_driveRAM.resize(CBM1541_RAM_SIZE);
	}
	else if(address >= CBM1541_VIA1_OFFSET and address <= CBM1541_VIA1_OFFSET + m_via1MEM.size()) {
		m_via1MEM.replace(address - CBM1541_VIA1_OFFSET, bytes.size(), bytes);
		qDebug() << "size before: " << m_via1MEM.size();
		m_via1MEM.resize(CBM1541_VIA1_SIZE);
		qDebug() << "size after: " << m_via1MEM.size();
	}
	else if(address >= CBM1541_VIA2_OFFSET and address <= CBM1541_VIA2_OFFSET + m_via2MEM.size()) {
		m_via2MEM.replace(address - CBM1541_VIA2_OFFSET, bytes.size(), bytes);
		m_via2MEM.resize(CBM1541_VIA2_SIZE);
	}
	// any other memory range is a no-op. Can't write to ROM, can't write to non-existent memory.
} // writeDriveMemory


// Parse LOAD command, open either special/file/directory/d64/t64/...
// The specials are:
// single arrow / double slash: up one folder/image, rest of string may reference file or folder relative that.
// double arrow: Reset interface
// double exclamation: Get media info
// dollar: Load current directory listing
CBM::IOErrorMessage Interface::openFile(const QString& cmdString)
{
	QString cmd(cmdString); // need to take copy since we're continously modifying string while processing.
	CBM::IOErrorMessage retCode = CBM::ErrOK;

	// assume fall back result
	m_openState = O_NOTHING;

	// Either a single back arrow or double slash takes the command relative parent folder or 'up' from the current image.
	if(not cmd.isEmpty() and CBM_BACK_ARROW == cmd.at(0).toLatin1() and cmd not_eq QString("%1%1").arg(CBM_BACK_ARROW)) {
		moveToParentOrNativeFS();
		cmd.remove(0, 1);
	}
	else while((cmd.size() >= 2 and "//" == cmd.left(2))) {
		moveToParentOrNativeFS();
		cmd.remove(0, 2);
	}

	// Check double back arrow first, it is a reset state.
	if(cmd == QString("%1%1").arg(CBM_BACK_ARROW)) {
		// move to reset state of interface and make sure UI does not assume any mounted media as well.
		reset(true);
		Log(FAC_IFACE, success, "Resetting, going back to NativeFS and sending media info.");
	}
	else if(CBM_EXCLAMATION_MARKS == cmd) {
		// to get the media info for any OTHER media, the '!!' should be used on the CBM side.
		// whatever file system we have active, check if it supports media info.
		m_openState = m_currFileDriver->supportsMediaInfo() ? O_INFO : O_NOTHING;
	}
	else if(not cmd.isEmpty() and cmd.at(0) == QChar(CBM_DOLLAR_SIGN)) // Send directory listing of the current directory, of whatever file system is the actual one.
		m_openState = O_DIR;
	else {
		// open file depending on interface state
		if(m_currFileDriver == &m_native) {
			// Try if cd works, then try open as file and if none of these ok...then give up
			if(QDir(cmd).exists() and m_native.setCurrentDirectory(cmd)) {
				Log(FAC_IFACE, success, QString("Changed to native FS directory: %1").arg(cmd));
				if(0 not_eq m_pListener) // notify UI listener of change.
					m_pListener->directoryChanged(QDir::currentPath());
				m_openState = O_DIR;
				if(0 not_eq m_pListener)
					m_pListener->imageMounted(cmd, m_currFileDriver);
			}
			else if(m_native.openHostFile(cmd)) {
				// File opened, investigate filetype
				bool fsFound = false;
				foreach(FileDriverBase* fs, m_fsList) {
					// if extension matches last three characters in any file system, then we set that filesystem into use.
					foreach(const QString& ext, fs->extension()) {
						if(not ext.isEmpty() and cmd.endsWith(ext, Qt::CaseInsensitive)) {
							m_currFileDriver = fs;
							fsFound = true;
							break;
						}
					}
					if(fsFound)
						break;
				}
				// did we have a match?
				if(fsFound) {
					m_native.closeHostFile();
					Log(FAC_IFACE, info, QString("Trying image mount using driver: %1").arg(m_currFileDriver->extFriendly()));
					// file extension matches, change interface state
					// call new format's reset
					if(m_currFileDriver->openHostFile(cmd)) {
						// see if this format supports listing, if not we're just opening as a file.
						if(not m_currFileDriver->supportsListing())
							m_openState = O_FILE;
						else {
							// otherwise we're in directory mode now on this selected file system image.
							m_openState = O_DIR;
							// Notify UI listener if attached.
						}
						if(0 not_eq m_pListener)
							m_pListener->imageMounted(cmd, m_currFileDriver);
						Log(FAC_IFACE, success, QString("Mount OK of image: %1").arg(m_currFileDriver->openedFileName()));
					}
					else {
						// Error initializing driver, back to native file system.
						Log(FAC_IFACE, error, QString("Error initializing driver for FS.ext: %1. Going back to Native FS.").arg(m_currFileDriver->extFriendly()));
						m_currFileDriver = &m_native;
						m_openState = O_FILE_ERR;
						retCode = CBM::ErrDriveNotReady;
					}
				}
				else { // No specific file format for this file, stay in FAT and send as straight .PRG
					Log(FAC_IFACE, info, QString("No specific file format for the names extension: %1, assuming .PRG in native file mode.").arg(cmd));
					m_openState = O_FILE;
				}
			}
			else { // File not found, giveup.
				Log(FAC_IFACE, warning, QString("File %1 not found. Returning FNF to CBM.").arg(cmd));
				m_openState = O_NOTHING;
				retCode = CBM::ErrFileNotFound;
			}
		}
		else if(0 not_eq m_currFileDriver) {
			// Call file format's own open command
			Log(FAC_IFACE, info, QString("Trying open FS file: %1 on FS: %2").arg(cmd).arg(m_currFileDriver->extFriendly()));
			if(m_currFileDriver->fopen(cmd))
				m_openState = O_FILE;
			else {// File not found
				m_openState = O_NOTHING;
				retCode = CBM::ErrFileNotFound;
			}
		}
	}

	return retCode;
} // openFile


void Interface::sendOpenResponse(char code) const
{
	// Response: ><code><CR>
	// send back response / result code to uno.
	QByteArray data;
	data.append('>');
	data.append(code);
	data.append('\r');
	m_port.write(data);
	m_port.flush();
} // sendOpenResponse


void Interface::processOpenCommand(const QString& cmd, bool localImageSelectionMode)
{
	// Request: <channel>|<command string>
	Log(FAC_IFACE, info, QString("processOpenCommand, cmd: %1").arg(cmd));
	QStringList params(cmd.split('|'));
	if(params.count() < 2) // enough params?
		m_queuedError = CBM::ErrSerialComm;
	else {
		uchar chan = params.at(0).toInt();
		const QString cmd = params.at(1);
		// Are we addressing the command channel?
		if(CBM::CMD_CHANNEL == chan) {
			// command channel command
			// (note: if any previous openfile command has given us an error, the 'current' file system to use is not defined and
			// therefore the command will fail, we don't even have the native fs to use for the open command).
			//			if(0 == m_currFileDriver)
			//				m_queuedError = ErrDriveNotReady;
			//			else
			//				m_queuedError = m_currFileDriver->cmdChannel(cmd);
			if(cmd.isEmpty()) {
				// Response: ><code><CR>
				// The code return is according to the values of the IOErrorMessage enum.
				// send back m_queuedError to uno.
				sendOpenResponse((char)m_queuedError);
			}
			else {
				m_queuedError = CBMDos::Command::execute(cmd, *this);
			}
			Log(FAC_IFACE, m_queuedError == CBM::ErrOK ? success : error, QString("CmdChannel_Response code: %1").arg(QString::number(m_queuedError)));
		}
		else if(CBM::READPRG_CHANNEL == chan) {
			// ...it was a open file for reading (load) command.
			m_openState = O_NOTHING;
			if(localImageSelectionMode) {// for this we have to fall back to nativeFS driver first.
				m_currFileDriver->closeHostFile();
				m_currFileDriver = &m_native;
			}
			m_queuedError = openFile(cmd);
			// if it was not only a local "UI" operation, we need to return some response to client.
			if(not localImageSelectionMode) {
				// Remember last cmd string.
				m_lastCmdString = cmd;
				if(m_queuedError == CBM::ErrOK and (O_INFO == m_openState or O_DIR == m_openState))
					buildDirectoryOrMediaList();

				// Response: ><code><CR>
				// The code return is according to the values of the IOErrorMessage enum.
				sendOpenResponse((char)m_openState);
				bool fail = m_openState == O_NOTHING or m_openState == O_FILE_ERR;
				Log(FAC_IFACE, fail ? error : success, QString("Open ReadPRG Response code: %1").arg(QString::number(m_openState)));

				// notify UI of opened file name and size.
				if(O_FILE == m_openState and 0 not_eq m_pListener)
					m_pListener->fileLoading(m_currFileDriver->openedFileName(), m_currFileDriver->openedFileSize());
			}

		}
		else if(CBM::WRITEPRG_CHANNEL == chan) {
			// it was an open file for writing (save) command.
			m_openState = O_NOTHING;
			if(0 not_eq m_currFileDriver) {
				bool overWrite = cmd.startsWith('@');
				const QString fileName(overWrite ? cmd.mid(1) : cmd);
				if(fileName.isEmpty())
					m_queuedError = CBM::ErrNoFileGiven;
				else if(0 not_eq m_pListener and m_pListener->isWriteProtected())
					m_queuedError = CBM::ErrWriteProtectOn;
				else {
					m_queuedError = m_currFileDriver->fopenWrite(fileName, overWrite);
					if(CBM::ErrOK not_eq m_queuedError)
						m_openState = O_FILE_ERR;
					else if(0 not_eq m_pListener) {
						m_pListener->fileSaving(fileName);
						m_openState = overWrite ? O_SAVE_REPLACE : O_SAVE;
					}
				}
			}
			else
				m_queuedError = CBM::ErrDriveNotReady;

			// The code return is according to the values of the IOErrorMessage enum.
			sendOpenResponse((char)m_queuedError);
			Log(FAC_IFACE, m_queuedError == CBM::ErrOK ? success : error, QString("Open WritePRG Response code: %1").arg(QString::number(m_queuedError)));
		}
		else { // some other channel.
			Log(FAC_IFACE, warning, QString("processOpenCommand: got open for channel: %1, not implemented.").arg(chan));
		}
	}
} // processOpenCommand


void Interface::processCloseCommand()
{
	QString name = m_currFileDriver->openedFileName();
	QByteArray data;
	// TODO: return proper response here: small 'n' means last operation was a save operation.
	data.append(m_openState == O_SAVE or m_openState == O_SAVE_REPLACE ? 'n' : 'N').append((char)name.length()).append(name);
	m_port.write(data);
	m_port.flush();
	if(0 not_eq m_pListener) // notify UI listener of change.
		m_pListener->fileClosed(name);
	Log(FAC_IFACE, info, QString("Close: Returning last opened file name: %1").arg(name));
	if(not m_currFileDriver->close()) {
		m_currFileDriver = &m_native;
		if(0 not_eq m_pListener)
			m_pListener->imageUnmounted();
	}
	// FIXME: Maybe this should not be set to ok here, it is up to the last processRead/Write operation to set that?
	// That is depending on whether reading failed or writing failed (e.g. full disk).
	m_queuedError = CBM::ErrOK;
} // processCloseCommand


void Interface::processGetOpenFileSize()
{
	ushort size = m_currFileDriver->openedFileSize();

	QByteArray data;
	uchar high = size >> 8, low = size bitand 0xff;
	data.append('S').append((char)high).append((char)low);
	m_port.write(data.data(), data.size());
	m_port.flush();
	Log(FAC_IFACE, info, QString("GetOpenFileSize: Returning file size: %1").arg(QString::number(size)));
} // processGetOpenedFileSize


void Interface::processLineRequest()
{
	if(O_INFO == m_openState or O_DIR == m_openState) {
		if(m_dirListing.isEmpty()) {
			// last line was produced. Send back the ending char.
			m_port.write("l");
			m_port.flush();
			Log(FAC_IFACE, success, "Last directory line written to arduino.");
		}
		else {
			m_port.write(m_dirListing.first().data(), m_dirListing.first().size());
			m_port.flush();
			m_dirListing.removeFirst();
		}
	}
	else {
		// TODO: This is a strange error state. Maybe we should return something to CBM here.
		Log(FAC_IFACE, error, "Strange state.");
	}
} // processOpenCommand


void Interface::processReadFileRequest(ushort length)
{
	QByteArray data;
	uchar count;
	bool atEOF = false;

	if(length)
		m_currReadLength = length;
	// NOTE: -2 here because we need two bytes for the protocol.
	for(count = 0; count < m_currReadLength - 2 and not atEOF; ++count) {
		data.append(m_currFileDriver->getc());
		atEOF = m_currFileDriver->isEOF();
	}
	if(0 not_eq m_pListener)
		m_pListener->bytesRead(data.size());
	// prepend whatever count we got.
	data.prepend(count);
	// If we reached end of file, head byte in answer indicates with 'E' instead of 'B'.
	data.prepend(atEOF ? 'E' : 'B');

	m_port.write(data.data(), data.size());
	m_port.flush();
} // processReadFileRequest


void Interface::processWriteFileRequest(const QByteArray& theBytes)
{
	foreach(uchar theByte, theBytes)
		m_currFileDriver->putc(theByte);
	if(0 not_eq m_pListener)
		m_pListener->bytesWritten(theBytes.length());
} // processWriteFileRequest


// For a specific error code, we are supposed to return the corresponding error string.
void Interface::processErrorStringRequest(CBM::IOErrorMessage code)
{
	// the return message begins with ':' for sync.
	QByteArray retStr(1, ':');
	// Assume not found by pre-assigning the unknown message.
	QString messageString(s_unknownMessage);
	foreach(const QString& msg, s_IOErrorMessages) {
		if(msg.split(',', QString::KeepEmptyParts).first().toInt() == code) {
			// found it!
			messageString = msg;
			break;
		}
	}

	// append message and the common ending and terminate with CR.
	retStr.append(messageString + s_errorEnding + '\r');
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
		Log(FAC_IFACE, info, QString("Producing directory listing for FS: \"%1\"...").arg(m_currFileDriver->extFriendly()));
		if(not m_currFileDriver->sendListing(*this)) {
			m_queuedError = CBM::ErrDirectoryError;
			Log(FAC_IFACE, warning, QString("Directory listing indicated error. Still sending: %1 chars").arg(QString::number(m_dirListing.length())));
		}
		else {
			Log(FAC_IFACE, success, QString("Directory listing ok (%1 lines). Ready waiting for line requests from arduino.").arg(m_dirListing.count()));
			m_queuedError = CBM::ErrOK;
		}
	}
	else if(O_INFO == m_openState) {
		Log(FAC_IFACE, info, QString("Producing media info for FS: \"%1\"...").arg(m_currFileDriver->extFriendly()));
		if(not m_currFileDriver->sendMediaInfo(*this)) {
			Log(FAC_IFACE, warning, QString("Media info listing indicated error. Still sending: %1 chars").arg(QString::number(m_dirListing.length())));
			m_queuedError = CBM::ErrDirectoryError;
		}
		else {
			Log(FAC_IFACE, success, QString("Media info listing ok (%1 lines). Ready waiting for line requests from arduino.").arg(m_dirListing.count()));
			m_queuedError = CBM::ErrOK;
		}
	}
} // buildDirectoryOrMediaList


bool Interface::changeNativeFSDirectory(const QString& newDir)
{
	return m_native.setCurrentDirectory(newDir);
} // changeNativeFSDirectory
