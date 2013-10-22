#ifndef M2IDRIVER_H
#define M2IDRIVER_H

#include "filedriverbase.hpp"


class M2I : public FileDriverBase
{
public:
	const QStringList& extension() const
	{
		static const QStringList ext({ "M2I" });
		return ext;
	} // extension

	// method below performs init of the driver with the given ATN command string.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	bool sendListing(ISendLine& cb);

	// command channel command
	uchar cmd(char* s);

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


	bool remove(char* fileName);
	bool rename(char* oldName, char* newName);

private:
	bool readFirstLine(QString* dest = 0);
	uchar parseLine(QString* dosName, QString *cbmName);
	bool createFile(char* fileName);
	bool seekFile(QString* dosName, QString& dirName, const QString& seekName, bool doOpen);


	struct FileEntry {
		enum Type {
			TypePrg,
			TypeDel,
			TypeErased
		};
		QString nativeName; // 12 chars (8.3 format)
		QString cbmName;		// 16 chars
	};
	typedef QList<FileEntry> EntryList;
	QString m_diskTitle; // 16 chars
	EntryList m_entries;

	// The real host file system M2I file:
	QFile m_hostFile;
};

#endif
