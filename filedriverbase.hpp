#ifndef FILEDRIVERBASE_HPP
#define FILEDRIVERBASE_HPP

#include <QString>
#include <QFile>

class ISendLine
{
public:
	virtual void send(short lineNo, const QString& text) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// Error messages
//
typedef enum {
	ErrOK,
	ErrReadError,
	ErrWriteProtectOn,
	ErrSyntaxError,
	ErrFileNotFound,
	ErrFileExists,
	ErrIntro,
	ErrDriveNotReady,	// typically in this emulation could also mean: not supported on this file system.
	ErrSerialComm,		// something went sideways with serial communication to the file server.
	ErrCount
} IOErrorMessage;


class FileDriverBase
{
public:
	enum FSStatus
	{
		NOT_READY = 0,  // driver not ready (host file not read or accepted).
		IMAGE_OK   = (1 << 0),  // The open file in fat driver is accepted as a valid image (of the specific file system type).
		FILE_OPEN = (1 << 1),  // A file is open right now
		FILE_EOF  = (1 << 2),  // The open file is ended
		DIR_OPEN  = (1 << 3),  // A directory entry is active
		DIR_EOF   = (1 << 4)  // Last directory entry has been retrieved
	};

	FileDriverBase();
	virtual ~FileDriverBase();

	// The three letter extension this file format represents (DOS style) Empty string returned means 'any' and is in default fs mode.
	virtual const QString& extension() const = 0;
	// method below performs init of the driver with the given ATN command string.
	virtual bool openHostFile(const QString& fileName) = 0;
	virtual void closeHostFile() = 0;

	// Send realistic $ file basic listing, line by line (returning false means not supported).
	virtual bool sendListing(ISendLine& cb);
	virtual bool supportsListing() const;
	// Command to the command channel. When not supported (overridden we just say write protect error).
	virtual IOErrorMessage cmdChannel(const QString& cmd);
	// Open a file in the image/file system by filename: Returns true if successful (false if not supported or error).
	virtual bool fopen(const QString& fileName);
	// Create a file in the image/file system by filename: Returns true if successful (false if not supported or error).
	virtual bool newFile(const QString& fileName);
	// returns a character from the open file. Should always be supported in order to make implementation make any sense.
	virtual char getc() = 0;
	// returns true if end of file reached. Should always be supported in order to make implementation make any sense.
	virtual bool isEOF() const = 0;
	// returns a character to the open file. If not overridden, returns always true. If implemented returns false on failure.
	virtual bool putc();
	// closes the open file. Should always be supported in order to make implementation make any sense.
	virtual bool close() = 0;

	// Current status of operation.
	virtual FSStatus status() const = 0;

	// This method is not relevant for any c64 side file systems (unless future one support such).
	// It sets the current directory on the native fs. Optionally implemented, base returns false.
	virtual bool setCurrentDirectory(const QString& dir);

protected:
	// Status of the driver:
	uchar m_status;

};

#endif // FILEDRIVERBASE_HPP
