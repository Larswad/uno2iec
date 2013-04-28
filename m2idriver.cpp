//
// Title        : M2IDRIVER
// Author       : Lars Pontoppidan
// Date         : May. 2007
// Version      : 0.1
// Target MCU   : AtMega32(L) at 8 MHz
//
//
// DESCRIPTION:
// This module implements support for the file format with extension .m2i
// With an .m2i file, it is possible to mimic an unlimited size diskette with
// read/write support and realistic filenames, while actually using FAT files.
//
// .m2i format:
//
// <DISK TITLE (16 chars)><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
// <file type 1 char>:<dos filename 12chars>:<cbm filename 16 chars><cr><lf>
//
// file types: P means prg file
//             D means del file
//             - means deleted file
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//

#include <string.h>
#include "m2idriver.hpp"


// Local vars, driver state:

char m2i_filename[13];

unsigned char m2i_status;

const char pstr_error1[] = "M2I FILE ERROR";

const char pstr_prg[] = "PRG";
const char pstr_del[] = "DEL";
const char pstr_id[] = " M2I";

const char pstr_dot_prg[] = ".PRG\0";
const char pstr_dot_m2i[] = ".M2I\0";

#define M2I_STATUS_FOPEN 1


// Get first line of m2i file, writes 16 chars in dest
// Returns FALSE if there is a problem in the file
bool M2I::readFirstLine(char* dest)
{
  for(uchar i = 0; i < 16; ++i) {
    uchar c = fatFgetc();
    if(0 not_eq dest)
      dest[i] = c;
  }

  return (fatFgetc() == 13 and fatFgetc() == 10);
} // readFirstLine


// Parses one line from the .m2i file.
// File pos must be after reading the initial status char
//
// <file type char>:<dosname>:<cbmname><cr><lf>
//
// at dosname 12 chars is written, at cbmname 16 chars
//
uchar M2I::parseLine(char* dosName, char* cbmName)
{
  unsigned char ftype;
  unsigned char c,i;

  if(fatFeof())
    return 0;  // no more records

  // Get file type
  ftype = fatFgetc();

  // seperator
  if(fatFgetc() not_eq ':')
    return 0;

  // Get dos filename
  for(i = 0; i < 12; i++) {
    c = fatFgetc();
    if (0 not_eq dosName)
      dosName[i] = c;
  }

  // seperator
  if(fatFgetc() not_eq ':')
    return 0;

  // Get cbm filename
  for(i = 0; i < 16; i++) {
    c = fatFgetc();
    if(0 not_eq cbmName)
      cbmName[i] = c;
  }

  // Get line break
  if((fatFgetc() == 13) and (fatFgetc() == 10))
    return ftype; // everything OK

  return 0;
}


bool M2I::init(char* s)
{
  bool ret;
  // Interface has just opened the m2i file, save filename
  if(strlen(s) > 12)
    return false;
  strcpy(m2i_filename, s);

  m2i_status = 0;

  // We accept m2i file if first line is OK:
  ret = readFirstLine(0);
  fatFclose();

  return ret;
} // init


void M2I::sendListing(ISendLine &cb)
{
  unsigned char c;
  char buffer[27];

  m2i_status = 0;

  // Open the .m2i file
  if(!fatFopen(m2i_filename)) {
    memcpy_P(buffer, pstr_error1, 14);
    cb.send(0, 14, buffer);
    return;
  }

  // First line is disk title

  buffer[0] = 0x12;        // Invert face
  buffer[1] = 0x22;        // start "
  c = readFirstLine(buffer+2); // disk title
  buffer[18] = 0x22;       // end "
  memcpy_P(buffer+19, pstr_id, 4);

  if(!c) {
    memcpy_P(buffer, pstr_error1, 14);
    cb.send(0, 14, buffer);
    return;
  }

  cb.send(0, 23, buffer);

  // Prepare buffer
  buffer[0] = ' ';
  buffer[1] = ' ';
  buffer[2] = '"';
  buffer[19] = '"';
  buffer[20] = ' ';

  while(!fatFeof()) {
    // Write lines
    c = parseLine(NULL, buffer+3);

    if((c == 'P') or (c == 'D')) {
      // This is a valid line, add ending and send
      memcpy(buffer+21, (c == 'P') ? pstr_prg : pstr_del, 3);
      cb.send(0, 24, buffer);
    }
  }

  fatFclose();
} // sendListing


// Seek through m2i index. dosname and dirname must be pointers to
// 12 and 16 byte buffers. Dosname can be NULL.
//
bool M2I::seekFile(char* dosName, char* dirName, char *seekName,
                   char openClose)
{
  uchar i;
  bool found = false;
  uchar seeklen, dirlen;

  if(open_close) {
    // Open the .m2i file
    if (!fatFopen(m2i_filename))
      return false;
  }
  else {
    // Just ensure we are at beginning
    fatFseek(0, SEEK_SET);
  }

  if(!read_first_line(NULL))
    return false;

  // Find length of seek name, disregard ending spaces
  seeklen = strnlen(seekname, 16);
  for(; (seeklen > 0) and (seekname[seeklen-1] == ' '); seeklen--);

  while(!found and !fatFeof()) {
    i = parseLine(dosname, dirname);

    if(i == 'P') {
      // determine length of dir filename
      for(dirlen = 16; (dirlen > 0) and (dirname[dirlen - 1] == ' '); dirlen--);

      // Compare filename respecting * and ? wildcards
      found = true;
      for(i = 0; i < seeklen and found; i++) {
        if(seekname[i] == '?') {
          // This character is ignored
        }
        else if(seekname[i] == '*') {
          // No need to check more chars
          break;
        }
        else
          found = seekname[i] == dirname[i];
      }

      // If searched to end of filename, dir filename must end here also
      if(found and (i == seeklen))
        found = (seeklen == dirlen);
    }
  }

  // Close m2i file
  if (open_close)
    fatFclose();

  return found;
} // seekFile


bool M2I::remove(char* fileName)
{
  char dosname[12];
  char dirname[16];
  unsigned char found;
  bool ret = false;

  m2i_status and_eq compl M2I_STATUS_FOPEN;

  // Open m2i index
  if (!fatFopen(m2i_filename)) return FALSE;

  do {
    // Seek for filename
    found = seekFile(dosname, dirname, filename, false);

    if (found) {
      ret = true;
      // File found, delete entry in m2i, seek back length of one entry
      fatFseek((unsigned long)(-33), SEEK_CUR);
      // write a -
      fatFputc('-');
      // Close m2i
      fatFclose();
      // Delete fat file
      fatRemove(dosname);
      // Open m2i again
      fatFopen(m2i_filename);
    }

  } while (found);

  // Finally, close m2i
  fatFclose();

  return ret;
} // remove


bool M2I::rename(char *oldName, char *newName)
{
  char dirname[16];
  bool ret = FALSE;
  uchar j;

  m2i_status and_eq compl M2I_STATUS_FOPEN;

  // Open m2i index
  if (!fatFopen(m2i_filename))
    return false;

  // Look for oldname
  if(seekFile(NULL, dirname, oldname, false)) {

    // File found, rename entry in m2i
    // Seek back until name
    fatFseek((ulong)(-33 + 15), SEEK_CUR);

    // write new name, space padded
    j = newname[0];
    for(uchar i = 0; i < 16;i++) {
      if (j) {
        fatFputc(j);
        j = newname[i + 1];
      }
      else
        fatFputc(' ');
    }
    ret = true;
  }

  // Close m2i
  fatFclose();

  return ret;
} // rename


uchar M2I::newDisk(char* diskName)
{
  char dosName[13];
  uchar i,j;

  m2i_status and_eq compl M2I_STATUS_FOPEN;

  // Find new .m2i filename
  // Take starting 8 chars padded with space ending with .m2i
  strncpy(dosName, diskName, 8);
  i = strnlen(dosName, 8);
  if (i < 8)
    memset(dosName + i, ' ', 8 - i);
  memcpy(dosName + 8, pstr_dot_m2i, 5);

  // Is this filename free
  if(fatFopen(dosName)) {
    fatFclose();
    return ERR_FILE_EXISTS;
  }

  // It is free, create new file
  if (!fatFcreate(dosName)) return ERR_WRITE_PROTECT_ON;

  // Write m2i header with the diskname
  j = diskName[0];
  for(i = 0; i < 16; i++) {
    if(j) {
      fatFputc(j);
      j = diskName[i + 1];
    }
    else
      fatFputc(' ');
  }

  // Write newline
  fatFputc(13);
  fatFputc(10);
  fatFclose();

  // Now this is the open m2i
  memcpy(m2i_filename, dosName, 13);

  return ERR_OK;
} // newDisk


// Respond to command channel command
//
uchar M2I::cmd(char* cmd)
{
  // Get command letter and argument
  char cmdLetter;
  char *arg, *p;
  char ret = ERR_OK;

  cmdLetter = cmd[0];

  arg = strchr(cmd,':');

  if(0 not_eq arg) {
    arg++;

    if(cmdLetter == 'S') {
      // Scratch file(s)
      if(!remove(arg))
        ret = ERR_FILE_NOT_FOUND;
    }
    else if(cmdLetter == 'R') {
      // Rename, find =  and replace with 0 to get cstr
      p = strchr(arg, '=');

      if(0 not_eq p) {
        *p = 0;
        p++;
        if(!rename(p, arg))
          ret = ERR_FILE_NOT_FOUND;
      }

    }
    else if(cmdLetter == 'N')
      ret = newDisk(arg);
    else
      ret = ERR_SYNTAX_ERROR;
  }

  return ret;
} // cmd


uchar M2I::open(char* fileName)
{
  char dirname[16];
  char dosname[12];

  m2i_status = 0;

  // Look for filename
  if(seekFile(dosname, dirname, filename, true)) {
    // Open the dos name corresponding!
    if(fatFopen(dosname))
      m2i_status or_eq M2I_STATUS_FOPEN;

  }
  return (m2i_status bitand M2I_STATUS_FOPEN);
} // open


bool M2I::createFile(char* fileName)
{
  m2i_status = 0;

  // Find dos filename
  char dosName[13];
  uchar i,j;

  // Initial guess is starting 8 chars padded with space ending with .prg
  strncpy(dosName, fileName, 8);
  i = strnlen(dosName, 8);
  if(i < 8)
    memset(dosName + i, ' ', 8 - i);
  memcpy(dosName + 8, pstr_dot_prg, 5);

  // Is this filename free?
  i = 0;
  while(fatFopen(dosName)) {
    if(i > 99)
      return false;

    // No, modify it
    j = i % 10;
    dosName[6] = i - j + '0';
    dosName[7] = j + '0';

    i++;
  }

  // Now we have a suitable dos filename, create entry in m2i index
  if(!fatFopen(m2i_filename))
    return false;

  if(!read_first_line(NULL))
    return false;

  fatFseek(0,SEEK_END);

  fatFputc('P');
  fatFputc(':');

  for(i = 0; i < 12; i++)
    fatFputc(dosName[i]);

  fatFputc(':');

  j = fileName[0];
  for(i = 0; i < 16; i++) {
    if(j) {
      fatFputc(j);
      j = fileName[i + 1];
    }
    else
      fatFputc(' ');
  }

  fatFputc(13);
  fatFputc(10);
  fatFclose();

  // Finally, create the target file
  fatFcreate(dosName);

  m2i_status or_eq M2I_STATUS_FOPEN;
  return true;
} // createFile


// The new function ensures filename exists and is empty.
//
uchar M2I::newFile(char* fileName)
{
  char dirName[16];
  char dosName[12];

  m2i_status = 0;

  // Look for filename
  if (seekFile(dosName, dirName, fileName, TRUE)) {
    // File exists, delete it
    fatRemove(dosName);
    // And create empty
    fatFcreate(dosName);
    m2i_status or_eq M2I_STATUS_FOPEN;

  }
  else {
    // File does not exist, create it
    createFile(fileName);
  }

  return (m2i_status bitand M2I_STATUS_FOPEN);
} // newFile


char M2I::getc(void)
{
  if (m2i_status bitand M2I_STATUS_FOPEN)
    return fatFgetc();

  return 0;
} // getc


bool M2I::isEOF(void)
{
  if(m2i_status bitand M2I_STATUS_FOPEN)
    return fatFeof();

  return true;
} // isEOF


// write char to open file, returns false if failure
bool M2I::putc(char c)
{
  if(m2i_status bitand M2I_STATUS_FOPEN)
    return fatFputc(c);
  return false;
} // putc


// close file
bool M2I::close(void)
{
  m2i_status and_eq compl M2I_STATUS_FOPEN;
  fatFclose();

  return true;
} // close
