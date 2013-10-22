#include <QFileInfo>
#include "x00fs.hpp"

////////////////////////////////////////////////////////////////////////////////
//
// X00 (P00,S00,R00) file format implementation
// Reusing most of native format, only overrides opening to handle the file header.
////////////////////////////////////////////////////////////////////////////////

namespace {
 const QString X00MAGIC_STR("C64File");
}


void x00FS::closeHostFile()
{
	NativeFS::closeHostFile();
	// Leave no junk in header when closing file (will be done before open as well.)
	memset(&m_header, 1, sizeof(m_header));
} // closeHostFile


bool x00FS::fopen(const QString& fileName)
{
	bool success = NativeFS::fopen(fileName);
	if(success) {
		// Check that file can read at least header amount of bytes and that the header indicates it actually is a *00 file.
		if(m_hostFile.read((char*)&m_header, sizeof(m_header) < sizeof(m_header)) or QString::compare(QString(m_header.x00Magic), X00MAGIC_STR)) {
			success = false;
			closeHostFile();
		}
	}

	return success;
} // fopen


CBM::IOErrorMessage x00FS::fopenWrite(const QString& fileName, bool replaceMode)
{
	CBM::IOErrorMessage retCode = NativeFS::fopenWrite(fileName, replaceMode);
	if(CBM::ErrOK == retCode) {
		// We must write the header before anything else.
		strcpy((char*)m_header.x00Magic, X00MAGIC_STR.toLocal8Bit().data());
		// Use the given filename stripped of path and trimmed down in size for original CBM file name.
		QFileInfo fi(fileName);
		QString originalName(fi.baseName());
		originalName.truncate(sizeof(m_header.originalFileName));
		strcpy((char*)m_header.originalFileName, originalName.toLocal8Bit().data());

		// write header.
		m_hostFile.write((char*)&m_header, sizeof(m_header));
		// We are now standing at position ready for actual file content to be written.
	}
	return retCode;
} // fopenWrite


x00FS::x00FS()
{
} // ctor
