#include "interface.hpp"
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
#include <QStringList>

Interface::Interface(QextSerialPort& port)
	: m_currFileDriver(0), m_port(port), m_queuedError(ErrOK), m_openState(O_NOTHING)
{
	m_fsList.append(&m_native);
	m_fsList.append(&m_d64);
	m_fsList.append(&m_t64);
//	m_fsList.append(&m_m2i);

	m_currFileDriver = &m_native;
}

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
	if((95 == cmd.at(0).toLatin1()) and (95 == cmd.at(1))) {
		// reload sdcard and send info
		m_currFileDriver = &m_native;
		m_openState = O_INFO;
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
	else if(95 == cmd.at(0)) {    // back arrow sign on C64
		// One back arrow, exit current file format or cd..
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
		// It was not a special command, remove eventual CBM dos prefix
		if(removeFilePrefix(cmd)) {
			// @ detected, save with replace:
			m_openState = O_SAVE_REPLACE;
		}
		else {
			// open file depending on interface state
			if(m_currFileDriver == &m_native) {
				// Exchange 0xFF with tilde to allow shortened long filenames
				// HMM: This is DOS mumble jumble, we're on a linux FS...will not work.
				cmd.replace(QChar(0xFF), "~");

				// Try if cd works, then open file, then give up
				if(m_native.setCurrentDirectory(cmd))
					m_openState = O_DIR;
				else if(m_native.openHostFile(cmd)) {
					// File opened, investigate filetype
					QList<FileDriverBase*>::iterator i;
					for(i = m_fsList.begin(); i < m_fsList.end(); ++i) {
						if((*i)->extension() == cmd.right(3))
							m_currFileDriver = *i;
							break;
					}

					if(i not_eq m_fsList.end()) {
						m_native.closeHostFile();
						// file extension matches, change interface state
						// call new format's reset
						if(m_currFileDriver->openHostFile(cmd)) {
							// see if this format supports listing
							if(!m_currFileDriver->supportsListing())
								m_openState = O_FILE;
							else
								m_openState = O_DIR;
						}
						else {
							// Error initializing driver, back to fat
							m_currFileDriver = m_native;
							m_openState = O_FILE_ERR;
						}
					}
					else // No specific file format for this file, stay in FAT and send as .PRG
						m_openState = O_FILE;
				}
				else // File not found
					m_queuedError = ErrFileNotFound;
			}
			else if(0 not_eq m_currFileDriver) {
				// Call file format's open command
				if(m_currFileDriver->fopen(cmd))
					m_openState = O_FILE;
				else // File not found
					m_queuedError = ErrFileNotFound;
			}
		}
	}

	// Remember last cmd string.
	m_lastCmdString = cmd;
} // openFile


void Interface::processOpenCommand(const QString& cmd)
{
	// Request: <channel>|<command string>
	QStringList params(cmd.split('|'));
	if(params.count() < 2) // enough params?
		m_queuedError = ErrSerialComm;
	else {
		uchar chan = params.at(0).toInt();

		if(CMD_CHANNEL == chan) {
			// command channel command
			// (note: if any previous openfile command has given us an error, the 'current' file system to use is not defined and
			// therefore the command will fail, we don't even have the native fs to use for the open command).
			if(0 not_eq m_currFileDriver) {
				// TODO: here send command, with string sequence. get return code from pi (m_queuedError).
				m_queuedError = m_currFileDriver->cmdChannel(params.at(1));
			}
			else
				m_queuedError = ErrDriveNotReady;
		}
		else {
			// load command, be optimistic
			m_queuedError = ErrOK;
			openFile(params.at(1));
		}
	}
	// send back m_queuedError to uno.
	// Response: ><code><CR>
	// The code return is according to the values of the IOErrorMessage enum.
	m_port.write(QString(">%1\r").arg(m_queuedError).toLatin1());
} // processOpenCommand
