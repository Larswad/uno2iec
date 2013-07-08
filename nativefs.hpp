#ifndef NATIVEFS_HPP
#define NATIVEFS_HPP

#include "filedriverbase.hpp"

class NativeFS : public FileDriverBase
{
public:
	NativeFS();
	virtual ~NativeFS()
	{}

	const QString& extension() const
	{
		static const QString ext;
		return ext;
	} // extension


	// method below performs init of the driver with the given ATN command string.
	bool openHostFile(const QString& fileName);
	void closeHostFile();

	// Send realistic $ file basic listing, line by line (returning false means not supported).
	bool sendListing(ISendLine& cb);
	bool supportsListing() const
	{
		return true;
	}

	bool newFile(const QString& fileName);
	bool fopen(const QString& fileName);
	char getc();
	bool isEOF() const;
	bool putc();
	bool close();

	FSStatus status() const;
	bool setCurrentDirectory(const QString& dir);

};

#endif // NATIVEFS_HPP
