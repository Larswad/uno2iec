#ifndef M2IDRIVER_H
#define M2IDRIVER_H

#include "filedriverbase.hpp"


class M2I : public FileDriverBase
{
public:
	M2I();

	const QStringList& extension() const
	{
#if !(defined(TARGET_OS_X) || defined(_MSC_VER))
		static const QStringList ext({ "M2I" });
#else
		static QStringList ext;
		ext << "M2I";
#endif
		return ext;
	} // extension

	// method below performs init of the driver with the given ATN command string.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	bool supportsListing() const
	{
		return true;
	}

	bool sendListing(ISendLine& cb);

	bool fopen(const QString& fileName);
	CBM::IOErrorMessage fopenWrite(const QString& fileName, bool replaceMode = false);

	const QString openedFileName() const;
	ushort openedFileSize() const;

	char getc(void);

	bool isEOF(void) const;

	// write char to open file, returns false if failure
	bool putc(char c);

	// close file
	bool close(void);

	// new disk, creates empty M2I file with diskname and opens it
	uchar newDisk(char* diskName);

	bool deleteFile(const QString& fileName);

	CBM::IOErrorMessage renameFile(const QString& oldName, const QString& newName);

private:
	struct FileEntry {
		enum Type {
			TypePrg,
			TypeDel,
			TypeErased
		} fileType;
		QString nativeName; // 12 chars (8.3 format) padded with spaces
		QString cbmName;		// 16 chars padded with spaces.

		// Needed for QList::removeOne
		bool operator==(const FileEntry& rhs)
		{
			return this->fileType == rhs.fileType and this->nativeName == rhs.nativeName and this->cbmName == rhs.nativeName;
		}
	};
	typedef QList<FileEntry> EntryList;

	bool createFile(char* fileName);
	bool findEntry(const QString& findName, FileEntry& entry) const;
	const QString generateFile();

	QString m_diskTitle; // 16 chars
	EntryList m_entries;
	// The real host file system M2I index file.
	QFile m_hostFile;
	QFile m_nativeFile;
	FileEntry m_openedEntry;
};

#endif
