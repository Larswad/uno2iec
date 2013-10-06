#include "filedriverbase.hpp"

FileDriverBase::FileDriverBase()
	: m_status(NOT_READY)
{
} // ctor


FileDriverBase::~FileDriverBase()
{
} // dtor


bool FileDriverBase::supportsListing() const
{
	return false;
} // supportsListing


bool FileDriverBase::sendListing(ISendLine& /*cb*/)
{
	return false;
} // sendListing


bool FileDriverBase::supportsMediaInfo() const
{
	return false;
} // supportsMediaInfo


bool FileDriverBase::sendMediaInfo(ISendLine& /*cb*/)
{
	return false;
} // sendMediaInfo


CBM::IOErrorMessage FileDriverBase::cmdChannel(const QString& cmd)
{
	Q_UNUSED(cmd);
	return CBM::ErrWriteProtectOn;
} // cmdChannel


bool FileDriverBase::fopen(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // fopen


CBM::IOErrorMessage FileDriverBase::fopenWrite(const QString &fileName, bool replaceMode)
{
	Q_UNUSED(fileName);
	Q_UNUSED(replaceMode);
	return CBM::ErrNotImplemented;
} // fopenWrite


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
} // setCurrentDirectory


bool FileDriverBase::deleteFile(const QString& fileName)
{
	Q_UNUSED(fileName);
	return false;
} // deleteFile
