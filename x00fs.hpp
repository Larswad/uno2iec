#ifndef X00FS_HPP
#define X00FS_HPP

#include "nativefs.hpp"

// Some documentation about this format:
// http://www.infinite-loop.at/Power20/Documentation/Power20-ReadMe/AE-File_Formats.html

// Note: Reason for class name x00FS is that it is not fixed to .P00 (PRG), it may also be S00 (.SEQ) and .R00 (.REL).
class x00FS : public NativeFS
{
public:
	struct X00Header
	{
		char x00Magic[8];					// $00 - $07 'C64File'
		char originalFileName[17];	// $08 - $18 Original CBM name + nul
		uchar recordSize;						// $19 Record size for REL files
		//uchar Data[n];						// $1A - ...
	};


	x00FS();
	virtual ~x00FS() {}

	// TODO: The extension returned should be a QStringList since we support more than one extension here.
	const QString& extension() const
	{
		static const QString ext("P00");
		return ext;
	} // extension

	void closeHostFile();
	bool fopen(const QString& fileName);
	CBM::IOErrorMessage fopenWrite(const QString& fileName, bool replaceMode);
	// TODO: SHOULD we override the NativsFS sendListing to CBM here, listing only the originalFileName as output?

protected:
	X00Header m_header;
};

#endif // X00FS_HPP
