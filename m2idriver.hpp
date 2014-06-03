#ifndef M2IDRIVER_H
#define M2IDRIVER_H

#include "filedriverbase.hpp"


class M2I : public FileDriverBase
{
public:
	M2I();

	const QStringList& extension() const
	{
#if !(defined(__APPLE__) || defined(_MSC_VER))
		static const QStringList ext({ "M2I" });
#else
		static QStringList ext;
		ext << "M2I";
#endif
		return ext;
	} // extension

	// method below performs init of the driver with the given ATN command string.
	bool mountHostImage(const QString& fileName);
	void unmountHostImage();

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

	bool deleteFile(const QString& fileName);

	CBM::IOErrorMessage renameFile(const QString& oldName, const QString& newName);

	bool fileExists(const QString &filePath);

	// new disk, creates empty M2I file with diskname and opens it
	CBM::IOErrorMessage newDisk(const QString& name, const QString& id);

private:
	// The M2I file records represtented in internal form.
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
			return this->fileType == rhs.fileType and this->nativeName == rhs.nativeName and this->cbmName == rhs.cbmName;
		}
	};
	typedef QList<FileEntry> EntryList;

	bool createFile(char* fileName);
	bool findEntry(const QString& findName, FileEntry& entry, bool allowWildcards = true) const;
	const QString generateFile();

	QString m_diskTitle; // 16 chars
	EntryList m_entries;
	// The real host file system M2I index file.
	QFile m_hostFile;
	// The current CBM file being read or written from/to the index.
	QFile m_nativeFile;
	FileEntry m_openedEntry;
};

#endif
