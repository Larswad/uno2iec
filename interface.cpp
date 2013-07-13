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
}

Interface::Interface(QextSerialPort& port)
	: m_currFileDriver(0), m_port(port), m_queuedError(ErrOK), m_openState(O_NOTHING)
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
void Interface::openFile(const QString& cmdString)
{
	QString cmd(cmdString);

	// fall back result
	m_openState = O_NOTHING;

	// Check double back arrow first
	if((CBM_BACK_ARROW == cmd.at(0).toLatin1()) and (CBM_BACK_ARROW == cmd.at(1))) {
		// reload sdcard and send info
		m_currFileDriver = &m_native;
		m_openState = m_currFileDriver->supportsMediaInfo() ? O_INFO : O_NOTHING;
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
					m_openState = O_DIR;
				}
				else if(m_native.openHostFile(cmd)) {
					// File opened, investigate filetype
					QList<FileDriverBase*>::iterator i;
					for(i = m_fsList.begin(); i < m_fsList.end(); ++i) {
						// if extension matches last three characters in any file system, then we set that filesystem into use.
						if(!(*i)->extension().isEmpty() and cmd.endsWith((*i)->extension())) {
							m_currFileDriver = *i;
							break;
						}
					}

					if(i not_eq m_fsList.end()) {
						m_native.closeHostFile();
						Log("IFACE", QString("Trying driver: %1").arg(m_currFileDriver->extension()), info);
						// file extension matches, change interface state
						// call new format's reset
						if(m_currFileDriver->openHostFile(cmd)) {
							// see if this format supports listing, if not we're just opening as a file.
							if(!m_currFileDriver->supportsListing())
								m_openState = O_FILE;
							else // otherwise we're in directory mode now on this selected file system image.
								m_openState = O_DIR;
						}
						else {
							// Error initializing driver, back to native file system.
							Log("IFACE", QString("Error initializing driver for FS.ext: %1. Going back to native.").arg(m_currFileDriver->extension()), error);
							m_currFileDriver = &m_native;
							m_openState = O_FILE_ERR;
						}
					}
					else { // No specific file format for this file, stay in FAT and send as straight .PRG
						Log("IFACE", QString("No specific file format for the names extension: %1, assuming .PRG in native file mode.").arg(cmd), info);
						m_openState = O_FILE;
					}
				}
				else { // File not found, giveup.
					Log("IFACE", QString("File %1 not found. Returning FNF to CBM.").arg(cmd), warning);
					m_openState = O_NOTHING;
				}
			}
			else if(0 not_eq m_currFileDriver) {
				// Call file format's own open command
				Log("IFACE", QString("Trying open FS file: %1 on FS: %2").arg(cmd).arg(m_currFileDriver->extension()), info);
				if(m_currFileDriver->fopen(cmd))
					m_openState = O_FILE;
				else // File not found
					m_openState = O_NOTHING;
			}
		}
	}

	// Remember last cmd string.
	m_lastCmdString = cmd;
	if(O_INFO == m_openState or O_DIR == m_openState)
		buildDirectoryOrMediaList();
} // openFile


void Interface::processOpenCommand(const QString& cmd)
{
	// Request: <channel>|<command string>
	Log("IFACE", cmd, info);
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
			// Response: ><code><CR>
			// The code return is according to the values of the IOErrorMessage enum.
			// send back m_queuedError to uno.
			QByteArray data;
			data.append('>');
			data.append((char)m_queuedError);
			data.append('\r');
			m_port.write(data);
			m_port.flush();
			Log("IFACE", QString("CmdChannel_Response code: %1").arg(QString::number(m_queuedError)), m_queuedError == ErrOK ? success : error);
		}
		else {
			// ...otherwise it was a open file command, be optimistic
			m_openState = O_NOTHING;
			openFile(params.at(1));
			// Response: ><code><CR>
			// The code return is according to the values of the IOErrorMessage enum.
			QByteArray data;
			data.append('>');
			data.append((char)m_openState);
			data.append('\r');
			m_port.write(data);
			m_port.flush();
			bool fail = m_openState == O_NOTHING or m_openState == O_FILE_ERR;
			Log("IFACE", QString("Open_Response code: %1").arg(QString::number(m_openState)), fail ? error : success);
		}
	}
} // processOpenCommand


void Interface::processLineRequest()
{
	if(O_INFO == m_openState or O_DIR == m_openState) {
		if(m_dirListing.isEmpty()) {
			// last line was produced. Send back the ending char.
			m_port.write("l");
			Log(FAC_IFACE, "Last line written to arduino.", success);
		}
		else {
			m_port.write(reinterpret_cast<char*>(m_dirListing.first().data()));
			m_dirListing.removeFirst();
			Log(FAC_IFACE, "Writing line to arduino", info);
		}
	}
	else {
		// TODO: This is a strange error state. return something to CBM.
		Log(FAC_IFACE, "Strange state.", error);
	}

} // processOpenCommand


void Interface::send(short lineNo, const QString& text)
{
	QString line(text);
	// the line number is included with the line itself. It goes in with lobyte,hibyte.
	line.prepend(uchar((lineNo bitand 0xFF00) >> 8));
	line.prepend(uchar(lineNo bitand 0xFF));
	// length of it all is the first byte.
	line.prepend((uchar)text.length());
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
		// TODO: implement.
	}

} // buildDirectoryOrMediaList
