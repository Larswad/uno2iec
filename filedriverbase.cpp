#include "filedriverbase.hpp"

FileDriverBase::FileDriverBase()
	: m_status(NOT_READY)
{
} // ctor


FileDriverBase::~FileDriverBase()
{
}


bool FileDriverBase::sendListing(ISendLine& /*cb*/)
{
	return false;
} // sendListing

bool FileDriverBase::supportsListing() const
{
	return false;
}


IOErrorMessage FileDriverBase::cmdChannel(const QString& cmd)
{
	Q_UNUSED(cmd);
	return ErrWriteProtectOn;
} // cmdChannel


bool FileDriverBase::fopen(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // fopen


bool FileDriverBase::newFile(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // fopen


// returns a character to the open file. If not overridden, returns always true. If implemented returns false on failure.
bool FileDriverBase::putc(char c)
{
	Q_UNUSED(c);
	return true;
} // putc


FileDriverBase::FSStatus FileDriverBase::status(void) const
{
	return static_cast<FSStatus>(m_status);
} // status


bool FileDriverBase::setCurrentDirectory(const QString& dir)
{
	Q_UNUSED(dir);
	return false;
}
