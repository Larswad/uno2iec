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
		char x00Magic[8];						// $00 - $07 'C64File'
		char originalFileName[17];	// $08 - $18 Original CBM name + nul
		uchar recordSize;						// $19 Record size for REL files
		//uchar Data[n];						// $1A - ...
	};


	x00FS();
	virtual ~x00FS() {}

	const QStringList& extension() const
	{
#if !(defined(__APPLE__) || defined(_MSC_VER))
		static const QStringList ext( { "P00", "R00", "S00" } );
#else
		static QStringList ext;
		ext << "P00" << "R00" << "S00";
#endif
		return ext;
	} // extension

	bool supportsListing() const
	{
		return false;
	}

	bool supportsMediaInfo() const
	{
		return false;
	}

	void unmountHostImage();
	bool fopen(const QString& fileName);
	bool close();
	CBM::IOErrorMessage fopenWrite(const QString& fileName, bool replaceMode);
	// TODO: SHOULD we override the NativsFS sendListing to CBM here, listing only the originalFileName as output?

protected:
	X00Header m_header;
};

#endif // X00FS_HPP
