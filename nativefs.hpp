#ifndef NATIVEFS_HPP
#define NATIVEFS_HPP

#include "filedriverbase.hpp"

class NativeFS : public FileDriverBase
{
public:
	NativeFS();
	virtual ~NativeFS()
	{}

	const QStringList& extension() const
	{
		static const QStringList ext;
		return ext;
	} // extension

	// Special method only for native fs: Filtering is supported here.
	void setListingFilters(const QString& filters, bool listDirectories);

	// method below performs init of the driver with the given ATN command string.
	bool mountHostImage(const QString& fileName);
	void unmountHostImage();

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
	CBM::IOErrorMessage fopenWrite(const QString &fileName, bool replaceMode = false);

	const QString openedFileName() const;
	ushort openedFileSize() const;
	bool fileExists(const QString &filePath);
	CBM::IOErrorMessage renameFile(const QString& oldName, const QString& newName);
	bool deleteFile(const QString& fileName);
	char getc();
	bool isEOF() const;
	bool putc(char c);
	bool close();
	CBM::IOErrorMessage copyFiles(const QStringList& sourceNames, const QString &destName);

	// Command to the command channel.
	CBM::IOErrorMessage cmdChannel(const QString& cmd);

	FSStatus status() const;
	bool setCurrentDirectory(const QString& dir);
protected:
	// File to open, either as for checking its existance before trying another FS, or for reading .PRG native files.
	QFile m_hostFile;

	QString m_filters;
	bool m_listDirectories;

};

#endif // NATIVEFS_HPP
