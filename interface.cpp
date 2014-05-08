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
		<< "97,UNO SERIAL ERR."	// Specific error to this emulated device, serial communication has gone out of sync.
		<< "98,NOT IMPLEMENTED";

const QString s_unknownMessage = "99,UNKNOWN ERROR";
const QString s_errorEnding = ",00,00";

} // anonymous


Interface::Interface()
	: m_currFileDriver(0)
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
		fs->unmountHostImage(); // TODO: Better with a reset or init method on all file systems.
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


void Interface::moveToParentOrNativeFS(bool toRoot)
{
	// Exit current file format or cd..
	if(m_currFileDriver == &m_native) {
		m_native.setCurrentDirectory(toRoot ? "/" : "..");
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
		if(toRoot) {
			m_native.setCurrentDirectory("/");
			if(0 not_eq m_pListener) // notify UI listener of change.
				m_pListener->directoryChanged(QDir::currentPath());
		}
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


// TODO: Place this in a utility file instead.
static QByteArray& replaceBytes(QByteArray& lhs, int pos, int len, const QByteArray& rhs)
{
	for(int i = pos, j = 0; i < pos + len and i < lhs.length() and j < rhs.length(); ++i, ++j)
		lhs[i] = rhs[j];
	return lhs;
} // replaceBytes


void Interface::writeDriveMemory(ushort address, const QByteArray& bytes)
{
	QByteArray source(bytes);
	// When doing resize (chop) after replace its because if address + array goes outside memory.
	if(/*address >= CBM1541_RAM_OFFSET and */address < CBM1541_RAM_OFFSET + m_driveRAM.size()) {
		replaceBytes(m_driveRAM, address, bytes.size(), source);
		m_driveRAM.resize(CBM1541_RAM_SIZE);
	}
	else if((address >= CBM1541_VIA1_OFFSET and address <= CBM1541_VIA1_OFFSET + m_via1MEM.size())
					or (address < CBM1541_VIA1_OFFSET and address + bytes.length() > CBM1541_VIA1_OFFSET)) {
		ushort length = bytes.length();
		if(address < CBM1541_VIA1_OFFSET) {
			source.remove(0, CBM1541_VIA1_OFFSET - address);
			length -= CBM1541_VIA1_OFFSET - address;
			address = CBM1541_VIA1_OFFSET;
		}
		replaceBytes(m_via1MEM, address - CBM1541_VIA1_OFFSET, length, source);
		m_via1MEM.resize(CBM1541_VIA1_SIZE);
	}
	else if((address >= CBM1541_VIA2_OFFSET and address <= CBM1541_VIA2_OFFSET + m_via2MEM.size())
					or (address < CBM1541_VIA2_OFFSET and address + bytes.length() > CBM1541_VIA2_OFFSET)) {
		ushort length = bytes.length();
		if(address < CBM1541_VIA2_OFFSET) {
			source.remove(0, CBM1541_VIA2_OFFSET - address);
			length -= CBM1541_VIA2_OFFSET - address;
			address = CBM1541_VIA2_OFFSET;
		}
		replaceBytes(m_via2MEM, address - CBM1541_VIA2_OFFSET, bytes.size(), source);
		m_via2MEM.resize(CBM1541_VIA2_SIZE);
	}
	else
		qDebug() << "trying to write to an invalid address.";
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

	cmd.replace("/:", "/");
	// remove leading ':' as they have no meaning (but do so in sd2iec for separation).
	while(not cmd.isEmpty() and ':' == cmd.at(0))
		cmd.remove(0, 1);
	// remove leading single '/' as they have no meaning (but do so in sd2iec for separation).
	if((cmd.size() >= 2 and '/' == cmd.at(0) and '/' not_eq cmd.at(1)) or (cmd.size() == 1 and '/' == cmd.at(0)))
		cmd.remove(0, 1);
	// handle root
	if(cmd.size() >= 2 and cmd.left(2) == "//") {
		cmd.remove(0, 2);
		moveToParentOrNativeFS(true);
	}

	// A single back arrow takes the command relative parent folder or 'up' from the current image.
	if((not cmd.isEmpty() and CBM_BACK_ARROW == cmd.at(0).toLatin1()) and cmd.toLatin1() not_eq QString() + CBM_BACK_ARROW + CBM_BACK_ARROW) {
		moveToParentOrNativeFS(false);
		cmd.remove(0, 1);
	}
	// Check double back arrow first, it is a reset state.
	if(cmd.toLatin1() == QString() + CBM_BACK_ARROW + CBM_BACK_ARROW) {
		// move to reset state of interface and make sure UI does not assume any mounted media as well.
		reset(true);
		Log(FAC_IFACE, success, "Resetting, going back to NativeFS and sending media info.");
		m_openState = O_DIR;
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
			if(not cmd.isEmpty() and QDir(cmd).exists() and m_native.setCurrentDirectory(cmd)) {
				Log(FAC_IFACE, success, QString("Changed to native FS directory: %1").arg(cmd));
				if(0 not_eq m_pListener) // notify UI listener of change.
					m_pListener->directoryChanged(QDir::currentPath());
				m_openState = O_DIR;
//				if(0 not_eq m_pListener)
//					m_pListener->imageMounted(cmd, m_currFileDriver);
			}
			else if(not cmd.isEmpty() and m_native.mountHostImage(cmd)) {
				// File opened, investigate filetype
				// if extension matches ending characters in any file systems extension list, then we set that filesystem into use.
				m_currFileDriver = driverForFile(cmd);
				// did we have a match?
				if(NULL not_eq m_currFileDriver) {
					m_native.unmountHostImage();
					Log(FAC_IFACE, info, QString("Trying image mount using driver: %1").arg(m_currFileDriver->extFriendly()));
					// file extension matches, change interface state
					// call new format's reset
					if(m_currFileDriver->mountHostImage(cmd)) {
						// see if this format supports listing, if not we're just opening as a file.
						if(not m_currFileDriver->supportsListing())
							m_openState = O_FILE;
						else {
							// otherwise we're in directory mode now on this selected file system image.
							m_openState = O_DIR;
						}
						// Notify UI listener if attached.
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
				else { // No specific file format for this file, stay in NATIVE fs and send as straight .PRG
					m_currFileDriver = &m_native;
					Log(FAC_IFACE, info, QString("No specific file format for the names extension: %1, assuming .PRG in native file mode.").arg(cmd));
					m_openState = O_FILE;
				}
			}
			else { // File not found, giveup.
				if(not cmd.isEmpty() or O_NOTHING == m_openState) {
					Log(FAC_IFACE, warning, QString("File %1 not found. Returning FNF to CBM.").arg(cmd));
					m_openState = O_NOTHING;
					retCode = CBM::ErrFileNotFound;
				}
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
	write(QByteArray().append('>').append(code).append('\r'));
} // sendOpenResponse


void Interface::processOpenCommand(uchar channel, const QByteArray& cmd, bool localImageSelectionMode)
{
	// Request: <channel>|<command string>
	Log(FAC_IFACE, info, QString("processOpenCommand, cmd: %1").arg(QString(cmd)));

	// Are we addressing the command channel?
	switch(channel) {
		case CBM::CMD_CHANNEL:
			// command channel command, or request for status if empty.
			if(cmd.isEmpty() or (cmd.length() == 1 and cmd.at(0) == '\r')) {
				// Response: ><code><CR>
				// The code return is according to the values of the IOErrorMessage enum.
				// send back m_queuedError to uno.
				sendOpenResponse((char)m_queuedError);
				Log(FAC_IFACE, info, QString("CmdChannel Status Response code: %1 = '%2'").arg(QString::number(m_queuedError)).arg(errorStringFromCode(m_queuedError)));
				// go back to OK state, we have dispatched the error to IEC host now. Error will only show once.
				m_queuedError = CBM::ErrOK;
			}
			else {
				// it's a DOS command, so execute it.
				m_queuedError = CBMDos::Command::execute(cmd, *this);
				Log(FAC_IFACE, m_queuedError == CBM::ErrOK ? success : error, QString("CmdChannel_Response code: %1 = '%2'")
						.arg(QString::number(m_queuedError)).arg(errorStringFromCode(m_queuedError)));
			}
			// Note: This MAY be actually an OPEN file instead so that close should return file name in this case too.
			m_openState = O_CMD;
			break;

		case CBM::READPRG_CHANNEL:
			// ...it was a open file for reading (load) command.
			m_openState = O_NOTHING;
			if(localImageSelectionMode) {// for this we have to fall back to nativeFS driver first.
				m_currFileDriver->unmountHostImage();
				m_currFileDriver = &m_native;
			}
			m_queuedError = openFile(QString(cmd));
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
			break;

		case CBM::WRITEPRG_CHANNEL:
			// it was an open file for writing (save) command.
			m_openState = O_NOTHING;
			if(0 not_eq m_currFileDriver) {
				bool overWrite = cmd.startsWith('@');
				const QString fileName(overWrite ? cmd.mid(1) : cmd);
				if(fileName.isEmpty())
					m_queuedError = CBM::ErrNoFileGiven;
				else if(isDiskWriteProtected())
					m_queuedError = CBM::ErrWriteProtectOn;
				else {
					m_queuedError = m_currFileDriver->fopenWrite(fileName, overWrite);
					if(CBM::ErrOK == m_queuedError) {
						if(0 not_eq m_pListener)
							m_pListener->fileSaving(fileName);
						m_openState = overWrite ? O_SAVE_REPLACE : O_SAVE;
					}
				}
			}
			else
				m_queuedError = CBM::ErrDriveNotReady;

			if(CBM::ErrOK not_eq m_queuedError)
				m_openState = O_FILE_ERR;

			// The code return is according to the values of the IOErrorMessage enum.
			sendOpenResponse((char)m_queuedError);
			Log(FAC_IFACE, m_queuedError == CBM::ErrOK ? success : error, QString("Open WritePRG Response code: %1").arg(QString::number(m_queuedError)));
			break;

		default:
			// some other channel.
			Log(FAC_IFACE, warning, QString("processOpenCommand: got open for channel: %1, not yet implemented.").arg(channel));
			break;
	}
} // processOpenCommand


void Interface::processCloseCommand()
{
	QString name = m_currFileDriver->openedFileName();
	QByteArray data;
	if(m_openState == O_SAVE or m_openState == O_SAVE_REPLACE or m_openState == O_FILE) {
		// Small 'n' means last operation was a save operation.
		data.append(m_openState == O_SAVE or m_openState == O_SAVE_REPLACE ? 'n' : 'N').append((char)name.length()).append(name);
		if(0 not_eq m_pListener) // notify UI listener of change.
			m_pListener->fileClosed(name);
		Log(FAC_IFACE, info, QString("Close: Returning last opened file name: %1").arg(name));
		if(not m_currFileDriver->close()) {
			m_currFileDriver = &m_native;
			if(0 not_eq m_pListener)
				m_pListener->imageUnmounted();
		}
	}
	else {
		// Means CLOSED and the drive number (that MAY have changed due to a comamnd).
		data.append('C').append(deviceNumber());
	}
	write(data);
	m_openState = O_NOTHING;
} // processCloseCommand


void Interface::processGetOpenFileSize()
{
	ushort size = m_currFileDriver->openedFileSize();

	QByteArray data;
	uchar high = size >> 8, low = size bitand 0xff;
	write(data.append('S').append((char)high).append((char)low));
	Log(FAC_IFACE, info, QString("GetOpenFileSize: Returning file size: %1").arg(QString::number(size)));
} // processGetOpenedFileSize


void Interface::processLineRequest()
{
	if(O_INFO == m_openState or O_DIR == m_openState) {
		if(m_dirListing.isEmpty()) {
			// last line was produced. Send back the ending char.
			write("l");
			Log(FAC_IFACE, success, "Last directory line written to arduino.");
		}
		else {
			write(m_dirListing.first());
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
	write(data);
} // processReadFileRequest


void Interface::processWriteFileRequest(const QByteArray& theBytes)
{
	foreach(uchar theByte, theBytes)
		m_currFileDriver->putc(theByte);
	if(0 not_eq m_pListener)
		m_pListener->bytesWritten(theBytes.length());
} // processWriteFileRequest


FileDriverBase* Interface::driverForFile(const QString& name) const
{
	foreach(FileDriverBase* driver, m_fsList)
		if(driver->supportsType(name))
			return driver;

	return NULL;
} // driverForFile


QString Interface::errorStringFromCode(CBM::IOErrorMessage code) const
{
	// Assume not found by pre-assigning the unknown message.
	foreach(const QString& msg, s_IOErrorMessages)
		if(code == msg.split(',', QString::KeepEmptyParts).first().toInt())	// found it!
			return msg;

	return s_unknownMessage;
} // errorStringFromCode


// For a specific error code, we are supposed to return the corresponding error string.
void Interface::processErrorStringRequest(CBM::IOErrorMessage code)
{
	// the return message begins with ':' for sync.
	QByteArray retStr(1, ':');

	// append message and the common ending and terminate with CR.
	write(retStr.append(errorStringFromCode(code) + s_errorEnding + '\r'));
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


void Interface::write(const QByteArray& data, bool flush) const
{
	if(0 not_eq m_pListener)
		m_pListener->writePort(data, flush);
} // write
