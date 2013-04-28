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
  // command channel errors
#define ERR_OK               0
#define ERR_READ_ERROR       1
#define ERR_WRITE_PROTECT_ON 2
#define ERR_SYNTAX_ERROR     3
#define ERR_FILE_NOT_FOUND   4
#define ERR_FILE_EXISTS      5
#define ERR_INTRO            6
#define ERR_DRIVE_NOT_READY  7



  bool init(char* s);

  void sendListing(ISendLine& cb);

  // command channel command
  unsigned char  cmd(char *s);

  uchar open(char *fileName);

  unsigned char  newFile(char *s);

  char  getc(void);

  bool isEOF(void);

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
  bool readFirstLine(char* dest);
  uchar parseLine(char* dosName, char* cbmName);
  bool createFile(char *fileName);

  bool seekFile(char *dosName, char *dirName, char *seekName,
                char openClose);
};

#endif
