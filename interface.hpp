#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "filedriverbase.hpp"
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
#include <qextserialport.h>

#define CMD_CHANNEL 15

enum  {
	IS_FAIL = 0xFF, // IFail: SD card or fat not ok
	IS_NATIVE = 0,			// Regular file system file state
	// states 1 -> NumInterfaceStates are also valid, representing what's open
	IS_D64 = 1,
	IS_T64 = 2,
	IS_M2I = 3,
	IS_PRG = 4,
	NumInterfaceStates
};

enum OpenState {
	O_NOTHING,			// Nothing to send / File not found error
	O_INFO,					// User issued a reload sd card
	O_FILE,					// A program file is opened
	O_DIR,					// A listing is requested
	O_FILE_ERR,			// Incorrect file format opened
	O_SAVE_REPLACE	// Save-with-replace is requested
};

class Interface
{
public:
	Interface(QextSerialPort& port);
	void processOpenCommand(const QString &cmd);

private:
	void openFile(const QString &cmdString);
	bool removeFilePrefix(QString &cmd);

	D64 m_d64;
	T64 m_t64;
//	M2I m_m2i;
	NativeFS m_native; // In fact, this is .PRG

	QList<FileDriverBase*> m_fsList;
	FileDriverBase* m_currFileDriver;
	QextSerialPort& m_port;
	IOErrorMessage m_queuedError;
	OpenState m_openState;
	QString m_lastCmdString;
};

#endif // INTERFACE_HPP
