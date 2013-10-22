#ifndef X64DRIVER_HPP
#define X64DRIVER_HPP

#include "filedriverbase.hpp"

// DiskType	- Floppy disk type: 1541 = {$01} Other defined values: (not usable for Power20)
// 0..1540, 1..1541, 2..1542, 3..1551,
// 4..1570, 5..1571, 6..1572, 8..1581,
// 16..2031&4031, 17..2040&3040, 18..2041
// 24..4040,
// 32..8050, 33..8060, 34..8061,
// 48..SFD 1001, 49..8250, 50..8280

class x64 : public FileDriverBase
{
public:
	struct X64File
	{
		uchar x64Magic[4];   // $00 - $03 ('C1541')
		uchar version[2];    // $04 - $05 (C1541 Version 2.6 = {$02, $06})
		uchar diskType;      // $06 (Floppy disk type)
		uchar trackCnt;      // $07 (Number of tracks on disk, (side 0) = 35).
		uchar secondSide;    // $08 (bool, double sided or not).
		uchar errorFlag;     // $09
		uchar reserved[22];  // $0A - $1F (Must be $00).
		uchar diskInfo[31];  // $20 - $3E (Description of the Disk Image (in ASCII or ISO Latin/1)
		uchar constZero;     // $3F (Must be $00)
		//uchar DiskImage[683 * 256]; // 683 disk sectors of 256 bytes each.
	};

	x64();
};

#endif // X64DRIVER_HPP
