#include <QFileInfo>
#include "x00fs.hpp"
#include "logger.hpp"

using namespace Logging;

////////////////////////////////////////////////////////////////////////////////
// X00 (P00,S00,R00) file format implementation
// Reusing most of native format, only overrides opening to handle the file header.
////////////////////////////////////////////////////////////////////////////////

namespace {
const QString X00MAGIC_STR("C64File");
}


void x00FS::unmountHostImage()
{
	NativeFS::unmountHostImage();
	// Leave no junk in header when closing file (will be done before open as well.)
	memset(&m_header, 1, sizeof(m_header));
} // closeHostFile


bool x00FS::fopen(const QString& fileName)
{
	bool success = NativeFS::fopen(fileName);
	if(success) {
		// Check that file can read at least header amount of bytes and that the header indicates it actually is a *00 file.
		if(m_hostFile.read((char*)&m_header, sizeof(m_header)) not_eq sizeof(m_header) or QString::compare(QString(m_header.x00Magic), X00MAGIC_STR)) {
			Log("X00FS", warning, QString("Couldn't open file %1, it is not of P00/R00/S00 format.").arg(fileName));
			success = false;
			unmountHostImage();
		}
		else
			Log("X00FS", warning, QString("Opened x00 file %1, original CBM name is: %2.").arg(fileName, m_header.originalFileName));
	}

	return success;
} // fopen


bool x00FS::close()
{
	NativeFS::close();
	// NOTE: Should not keep this image mounted here since we're done. Fall back to native FS.
	return false;
} // close


CBM::IOErrorMessage x00FS::fopenWrite(const QString& fileName, bool replaceMode)
{
	CBM::IOErrorMessage retCode = NativeFS::fopenWrite(fileName, replaceMode);
	if(CBM::ErrOK == retCode) {
		// We must write the header before anything else.
#ifdef _MSC_VER
		strcpy_s((char*)m_header.x00Magic, sizeof(m_header.x00Magic), X00MAGIC_STR.toLocal8Bit().data());
#else
		strcpy((char*)m_header.x00Magic, X00MAGIC_STR.toLocal8Bit().data());
#endif
		// Use the given filename stripped of path and trimmed down in size for original CBM file name.
		QFileInfo fi(fileName);
		QString originalName(fi.baseName());
		originalName.truncate(sizeof(m_header.originalFileName));
#ifdef _MSC_VER
		strcpy_s((char*)m_header.originalFileName, sizeof(m_header.originalFileName), originalName.toLocal8Bit().data());
#else
		strcpy((char*)m_header.originalFileName, originalName.toLocal8Bit().data());
#endif

		// write header.
		m_hostFile.write((char*)&m_header, sizeof(m_header));
		// We are now standing at position ready for actual file content to be written.
	}
	return retCode;
} // fopenWrite


x00FS::x00FS()
{
} // ctor
