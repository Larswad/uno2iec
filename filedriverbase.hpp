#ifndef FILEDRIVERBASE_HPP
#define FILEDRIVERBASE_HPP

#include <QString>
#include <QStringList>
#include <QFile>

#include "uno2iec/cbmdefines.h"

class ISendLine
{
public:
	virtual void send(short lineNo, const QString& text) = 0;
};

/// Base class for all virtual file systems supported.
class FileDriverBase
{
public:
	enum FSStatus
	{
		NOT_READY = 0,					// driver not ready (host file not read or accepted).
		IMAGE_OK  = (1 << 0),		// The open file in fat driver is accepted as a valid image (of the specific file system type).
		FILE_OPEN = (1 << 1),		// A file is open right now
		FILE_EOF  = (1 << 2),		// The open file is ended
		DIR_OPEN  = (1 << 3),		// A directory entry is active
		DIR_EOF   = (1 << 4)		// Last directory entry has been retrieved
	};

	FileDriverBase();
	virtual ~FileDriverBase();

	// The three letter extension this file format represents (DOS style) Empty string returned means 'any' and is in default fs mode.
	virtual const QStringList& extension() const = 0;
	// Returns a print-friendly version of the supported extensions as a pipe separated (single) string.
	virtual const QString extFriendly() const
	{
		return extension().join(QChar('|'));
	}
	// method below performs init of the driver with the given ATN command string.
	virtual bool openHostFile(const QString& fileName) = 0;
	virtual void closeHostFile() = 0;
	// returns true if the file system supports directory listing (t64 for instance doesn't).
	virtual bool supportsListing() const;
	// Send realistic $ file basic listing, line by line (returning false means there was some error, but that there is a listing anyway).
	virtual bool sendListing(ISendLine& cb);
	// Whether this file system supports media info or not (true == supports it).
	virtual bool supportsMediaInfo() const;
	// Send information about file system (whether it is OK, sizes etc.).
	virtual bool sendMediaInfo(ISendLine &);
	// Command to the command channel. When not supported (overridden we just say write protect error).
	virtual CBM::IOErrorMessage cmdChannel(const QString& cmd);

	// Open a file in the image/file system by filename: Returns true if successful (false if not supported or error).
	virtual bool fopen(const QString& fileName);
	// Open a file in the image/file system by filename for writing: Returns true if successful (false if not supported, error or file already exists).
	virtual CBM::IOErrorMessage fopenWrite(const QString& fileName, bool replaceMode = false);
	// return the name of the last opened file (may not be same as fopen in case it resulted in something else, like when using wildcards).
	virtual const QString openedFileName() const = 0;
	// return the file size of the last opened file.
	virtual ushort openedFileSize() const = 0;
	// check if a file exists with the given path/name on the file system.
	virtual bool fileExists(const QString& filePath);
	// Rename a file in the image/file system: Returns CBM status of success or failure.
	virtual CBM::IOErrorMessage renameFile(const QString& oldName, const QString& newName);
	// Copy one or more file(s) to one destination file. If several source files they are concatenated in specified order.
	virtual CBM::IOErrorMessage copyFiles(const QStringList& sourceNames, const QString& destName);
	// Delete a file in the image/file system: True if successful, false if not supported or error.
	virtual bool deleteFile(const QString& fileName);
	// returns a character from the open file. Should always be supported in order to make implementation make any sense.
	virtual char getc() = 0;
	// returns true if end of file reached. Should always be supported in order to make implementation make any sense.
	virtual bool isEOF() const = 0;
	// returns a character to the open file. If not overridden, returns always true. If implemented returns false on failure.
	// write char to open file, returns false if failure
	virtual bool putc(char c);
	// closes the open file. Should always be supported in order to make implementation make any sense.
	// If returning false here it indicates the filesystem is ready and should move back to native file system.
	virtual bool close() = 0;

	// Current status of operation.
	virtual FSStatus status() const;

	// This method is not relevant for any CBM side file systems (unless future one support such).
	// It sets the current directory on the native fs. Optionally implemented for a specific file system, base returns false.
	virtual bool setCurrentDirectory(const QString& dir);

protected:
	// Status of the driver:
	uchar m_status;

};

#endif // FILEDRIVERBASE_HPP
