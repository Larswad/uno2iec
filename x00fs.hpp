#ifndef X00FS_HPP
#define X00FS_HPP

#include "nativefs.hpp"

#define P00MAGIC_STR "C64File"

struct P00File
{
	uchar P00Magic[8];		// $00 - $07 'C64File'
	uchar OrigFName[17];	// $08 - $18 Original CBM name + nul
	uchar recordSize;			// $19 RecordSize for REL files
	//uchar Data[n];			// $1A - ...
};

class x00FS : public NativeFS
{
public:
	x00FS();
	bool putc(char c);
};

#endif // X00FS_HPP
