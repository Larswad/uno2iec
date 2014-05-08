#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <QStringList>

#include "filedriverbase.hpp"
#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"
#include "x00fs.hpp"
#include "nativefs.hpp"

typedef QList<FileDriverBase*> FileDriverList;

enum OpenState {
	O_NOTHING,			// Nothing to send / File not found error
	O_INFO,					// User issued a reload sd card
	O_FILE,					// A program file is opened for reading
	O_DIR,					// A listing is requested
	O_FILE_ERR,			// Incorrect file format opened
	O_SAVE,					// A program file is opened for writing
	O_SAVE_REPLACE,	// "---", but Save-with-replace is requested
	O_CMD						//  Command channel was opened
};


class Interface : public ISendLine
{
public:
	// Callback Interface for notification when Arduino triggers various operations (upon CBM requests).
	// This is typically just for UI reflection, updating progress bars, mounted file names, directory lists etc.
	struct IFileOpsNotify
	{
		virtual void directoryChanged(const QString& newPath) = 0;
		virtual void imageMounted(const QString& imagePath, FileDriverBase* pFileSystem) = 0;
		virtual void imageUnmounted() = 0;
		virtual void fileLoading(const QString& fileName, ushort fileSize) = 0;
		virtual void fileSaving(const QString& fileName) = 0;
		virtual void bytesRead(uint numBytes) = 0;
		virtual void bytesWritten(uint numBytes) = 0;
		virtual void fileClosed(const QString& lastFileName) = 0;
		virtual bool isWriteProtected() const = 0;
		virtual ushort deviceNumber() const = 0;
		virtual void setDeviceNumber(ushort deviceNumber) = 0;
		virtual void deviceReset() = 0;
		/// Write AND flush the data to the serial port, IF open.
		virtual void writePort(const QByteArray& data, bool flush = true) = 0;
	};

	Interface();
	virtual ~Interface();

	CBM::IOErrorMessage openFile(const QString &cmdString);
	void processOpenCommand(uchar channel, const QByteArray &cmd, bool localImageSelectionMode = false);
	void processReadFileRequest(ushort length = 0);
	void priocessWriteFileRequest(const QByteArray &theBytes);
	CBM::IOErrorMessage reset(bool informUnmount = false);

	bool isDiskWriteProtected() const
	{
		return NULL == m_pListener or m_pListener->isWriteProtected();
	}

	ushort deviceNumber() const
	{
		return NULL == m_pListener ? 8 : m_pListener->deviceNumber();
	}


	void setDeviceNumber(ushort deviceNumber)
	{
		if(NULL not_eq m_pListener)
			m_pListener->setDeviceNumber(deviceNumber);
	}

	// State specific: CBM requests a single directory line from us.
	void processLineRequest();
	void buildDirectoryOrMediaList();

	// ISendLine implementation.
	void send(short lineNo, const QString& text);

	void processGetOpenFileSize();
	void processCloseCommand();
	void processErrorStringRequest(CBM::IOErrorMessage code);
	bool changeNativeFSDirectory(const QString &newDir);
	void setMountNotifyListener(IFileOpsNotify *pListener);
	void setImageFilters(const QString &filters, bool showDirs);
	void processWriteFileRequest(const QByteArray &theBytes);
	void writePort(const QByteArray& data, bool flush = true);
	FileDriverBase* driverForFile(const QString& name) const;
	FileDriverBase* currentFileDriver()
	{
		return m_currFileDriver;
	}

	void readDriveMemory(ushort address, ushort length, QByteArray &bytes) const;
	void writeDriveMemory(ushort address, const QByteArray &bytes);

private:
	void moveToParentOrNativeFS(bool toRoot);
	bool removeFilePrefix(QString &cmd) const;
	void sendOpenResponse(char code) const;
	void write(const QByteArray &data, bool flush = true) const;
	QString errorStringFromCode(CBM::IOErrorMessage code) const;

	// Instantiation of implemented file system handlers. They will be added to the FileDriverList.
	D64 m_d64;
	T64 m_t64;
	M2I m_m2i;
	x00FS m_x00fs;

	NativeFS m_native; // In fact, this is .PRG

	FileDriverList m_fsList;
	FileDriverBase* m_currFileDriver;
	CBM::IOErrorMessage m_queuedError;
	OpenState m_openState;
	ushort m_currReadLength;
	QByteArray m_lastCmdString;
	QList<QByteArray> m_dirListing;
	IFileOpsNotify* m_pListener;

	// The ROM file for the 1541 drive (16 KB).
	QByteArray m_driveROM;
	// The RAM memory for the 1541 drive.
	QByteArray m_driveRAM;
	// The VIA1 control area for the 1541 drive.
	QByteArray m_via1MEM;
	// The VIA2 control area for the 1541 drive.
	QByteArray m_via2MEM;
};

#endif // INTERFACE_HPP
