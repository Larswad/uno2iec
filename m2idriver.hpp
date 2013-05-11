#ifndef M2IDRIVER_H
#define M2IDRIVER_H

#include "filedriverbase.hpp"

//
// Title        : MMC2IEC - M2IDRIVER
// Author       : Lars Pontoppidan
// Version      : 0.1
// Target MCU   : AtMega32(L) at 8 MHz
//
// DESCRIPTION:
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//

// API

class M2I : public FileDriverBase
{
public:
	const QString& extension() const
	{
		static const QString ext("M2I");
		return ext;
	} // extension

	// method below performs init of the driver with the given ATN command string.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	bool sendListing(ISendLine& cb);

	// command channel command
	uchar cmd(char *s);

	bool fopen(const QString& fileName);

	char getc(void);

	bool isEOF(void) const;

	// write char to open file, returns false if failure
	bool putc(char c);

	// close file
	bool close(void);

	// new disk, creates empty M2I file with diskname and opens it
	uchar newDisk(char* diskName);

	uchar newFile(char* fileName);

	bool remove(char* fileName);
	bool rename(char *oldName, char *newName);

private:
	bool readFirstLine(QString* dest = 0);
	uchar parseLine(QString *dosName, QString *cbmName);
	bool createFile(char *fileName);

	bool seekFile(QString *dosName, QString &dirName, const QString &seekName,
								bool doOpen);

	// The real host file system D64 file:
	QFile m_m2iHostFile;

};

#endif
