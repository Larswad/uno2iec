#ifndef FILEDRIVERBASE_HPP
#define FILEDRIVERBASE_HPP

#include <QString>
#include <QFile>

class ISendLine
{
public:
	virtual void send(short lineNo, uchar length, char* text) = 0;
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
	ErrDriveNotReady,
	ErrCount
} ErrorMessage;


class FileDriverBase
{
public:
	FileDriverBase();
	virtual ~FileDriverBase();

	// The three letter extension this file format represents (DOS style) Empty string returned means 'any' and is in default fs mode.
	virtual const QString& extension() = 0;
	// method below performs init of the driver with the given ATN command string.
	virtual bool openHostFile(const QString& fileName) = 0;
	// Send realistic $ file basic listing, line by line (returning false means not supported).
	virtual bool sendListing(ISendLine& cb);
	// Command to the command channel. When not supported (overridden we just say write protect error).
	virtual ErrorMessage cmdChannel(const QString& cmd);
	// Open a file in the image/file system by filename: Returns true if successful (false if not supported or error).
	virtual bool fopen(const QString& fileName);
	// Create a file in the image/file system by filename: Returns true if successful (false if not supported or error).
	virtual bool newFile(const QString& fileName);
	// returns a character from the open file. Should always be supported in order to make implementation make any sense.
	virtual bool getc() = 0;
	// returns true if end of file reached. Should always be supported in order to make implementation make any sense.
	virtual bool isEOF() = 0;
	// returns a character to the open file. If not overridden, returns always true. If implemented returns false on failure.
	virtual bool putc();
	// closes the open file. Should always be supported in order to make implementation make any sense.
	virtual bool close() = 0;
};

#endif // FILEDRIVERBASE_HPP
