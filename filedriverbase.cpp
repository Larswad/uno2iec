#include "filedriverbase.hpp"

FileDriverBase::FileDriverBase()
{
}


FileDriverBase::~FileDriverBase()
{
}


FileDriverBase::sendListing(ISendLine& cb)
{
	Q_UNUSED(ISendLine);
	return false;
}


ErrorMessage FileDriverBase::cmdChannel(const QString& cmd)
{
	Q_UNUSED(cmd);
	return ErrWriteProtectOn;
} // cmdChannel


virtual bool FileDriverBase::fopen(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // fopen


virtual bool FileDriverBase::newFile(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // fopen


// returns a character to the open file. If not overridden, returns always true. If implemented returns false on failure.
virtual bool putc(uchar c)
{
	Q_UNUSED(c);
	return true;
} // putc

