#ifndef NATIVEFS_HPP
#define NATIVEFS_HPP

#include "filedriverbase.hpp"

class NativeFS : public FileDriverBase
{
public:
	NativeFS();
	virtual ~NativeFS()
	{}

	const QString& extension() const
	{
		static const QString ext;
		return ext;
	} // extension


	// method below performs init of the driver with the given ATN command string.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	// Send realistic $ file basic listing, line by line (returning false means not supported).
	bool sendListing(ISendLine& cb);
	bool supportsListing() const
	{
		return true;
	}

	bool supportsMediaInfo() const
	{
		return true;
	}

	bool sendMediaInfo(ISendLine& cb);

	bool newFile(const QString& fileName);
	bool fopen(const QString& fileName);
	QString openedFileName() const;
	ushort openedFileSize() const;
	char getc();
	bool isEOF() const;
	bool putc(char c);
	bool close();
	// Command to the command channel.
	IOErrorMessage cmdChannel(const QString& cmd);

	FSStatus status() const;
	bool setCurrentDirectory(const QString& dir);
private:
	// File to open, either as for checking its existance before trying another FS, or for reading .PRG native files.
	QFile m_hostFile;

};

#endif // NATIVEFS_HPP
