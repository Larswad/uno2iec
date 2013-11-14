#include "doscommands.hpp"
#include "interface.hpp"
#include "d64driver.hpp"
#include "logger.hpp"

using namespace Logging;

namespace CBMDos {

CommandList Command::s_attached;

namespace {
QString FACDOS("DOSCMD");
// These static commands will auto-attach (register) themselves upon start.
InitDrive initDriveCmd;
ValidateDisk validateDiskCmd;
NewDisk newDiskCmd;
Scratch scratchFileCmd;
RenameFile renameFileCmd;
CopyFiles copyFilesCmd;
SetPosition setPositionCmd;
BlockRead blockReadCmd;
BlockWrite blockWriteCmd;
MemoryRead memoryReadCmd;
MemoryWrite memoryWriteCmd;
BufferPointer bufferPointerCmd;
BlockAllocate blockAllocateCmd;
BlockFree blockFreeCmd;
BlockExecute blockExecuteCmd;
MemoryExecute memoryExecuteCmd;
VC20ModeOnOff vc20ModeOnOffCmd;
DeviceAddress deviceAddressCmd;
ChangeDirectory chDirCmd;
MakeDirectory makeDirCmd;
RemoveDirectory rmDirCmd;
}


CBM::IOErrorMessage InitDrive::process(const QString& params, Interface& iface)
{
	// Either ignore or check that no parameters are given here!
	Q_UNUSED(params);
	return iface.reset();
} // InitDrive


CBM::IOErrorMessage ValidateDisk::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // ValidateDisk


CBM::IOErrorMessage NewDisk::process(const QString &params, Interface &iface)
{
	QStringList newParams(params.split(',', QString::SkipEmptyParts));

	if(newParams.empty() or newParams.count() > 2)
		return CBM::ErrSyntaxError;
	// TODO: Native file system should here create a new D64 file by using D64 helper, and then CALL that D64 to format/initialize it.
	// If the current file driver already IS a D64, then it just initializes the current disk, samt goes for T64.
	//iface.currentFileDriver->newDisk(newParams[0], newParams.count() > 1 ? params[1] : QString());
	Q_UNUSED(iface);

	return CBM::ErrNotImplemented;
} // NewDisk


CBM::IOErrorMessage Scratch::process(const QString &params, Interface &iface)
{
	// TODO: check write protect here!
	// TODO: Check that there is no path stuff in the name, we don't like that here.
	Log(FACDOS, info, QString("About to scratch file: %1").arg(params));
	if(not iface.currentFileDriver()->deleteFile(params))
		return CBM::ErrFileNotFound; // should be the proper reason of course.
	return CBM::ErrFilesScratched;
} // Scratch


CBM::IOErrorMessage RenameFile::process(const QString& params, Interface& iface)
{
	const QStringList names(params.split(QChar('=')));

	// old name and new name must be present, no more no less.
	if(2 not_eq names.count())
			return CBM::ErrSyntaxError;

	const QString oldName(names[0]);
	const QString newName(names[1]);

	// None of the names may be empty.
	if(oldName.isEmpty() or newName.isEmpty())
		return CBM::ErrNoFileGiven;

	// Check for any illegal characters. This is VERY platform specific for the native file systems, and only the real
	// rename will tell if this succeeds or not. But we may precheck in same way as sd2iec.
	if(isIllegalCBMName(newName))
		return CBM::ErrSyntaxError;

	// The new file must NOT already exist.
	if(iface.currentFileDriver()->fileExists(newName))
		return CBM::ErrFileExists;
	// Check if old file exists.
	if(not iface.currentFileDriver()->fileExists(oldName))
		return CBM::ErrFileNotFound;

	// NOTE: Need to check here whether the file is renamed across paths or not?

	Log(FACDOS, info, QString("About to rename file: %1 to %2").arg(oldName, newName));

	// Let the current file system decide whether it goes well by doing the rename operation.
	return iface.currentFileDriver()->renameFile(oldName, newName);
} // RenameFile


CBM::IOErrorMessage CopyFiles::process(const QString& params, Interface& iface)
{
	const QStringList paramList(params.split(QChar('=')));
	// destination name and source name(s) parameters must be present, no more no less.
	if(2 not_eq paramList.count())
			return CBM::ErrSyntaxError;
	const QString destName(paramList[0]);
	// destination name must not be empty it they must have legal characters.
	if(destName.isEmpty())
		return CBM::ErrNoFileGiven;
	if(isIllegalCBMName(destName))
		return CBM::ErrSyntaxError;

	// check availability and validity of source file name(s).
	const QStringList sourceList(paramList[1].split(QChar(',')));
	foreach(const QString& srcName, sourceList) {
		if(srcName.isEmpty())
			return CBM::ErrNoFileGiven;
		if(isIllegalCBMName(srcName))
			return CBM::ErrSyntaxError;
	}
	// The destination file must NOT already exist.
	if(iface.currentFileDriver()->fileExists(destName))
		return CBM::ErrFileExists;

	Log(FACDOS, info, QString("About to copy file(s): %1 to %2").arg(paramList[1], destName));

	// It is up to the specific file system to do the copy and rest of validation for what file type(s) are possible to concatenate.
	return iface.currentFileDriver()->copyFiles(sourceList, destName);
} // CopyFiles


CBM::IOErrorMessage SetPosition::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // SetPosition


CBM::IOErrorMessage BlockRead::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockRead


CBM::IOErrorMessage BlockWrite::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockWrite


CBM::IOErrorMessage MemoryRead::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MemoryReadCmd


CBM::IOErrorMessage MemoryWrite::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MemoryWrite


CBM::IOErrorMessage BufferPointer::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BufferPointer


CBM::IOErrorMessage BlockAllocate::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockAllocate


CBM::IOErrorMessage BlockFree::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockFree


CBM::IOErrorMessage BlockExecute::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockExecute


CBM::IOErrorMessage MemoryExecute::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MemoryExecute


CBM::IOErrorMessage VC20ModeOnOff::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // VC20ModeOnOff


CBM::IOErrorMessage DeviceAddress::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // DeviceAddress

// This is totally f'ed up right now. Fix so that it works as sd2iec.
// http://sd2iec.de/cgi-bin/gitweb.cgi?p=sd2iec.git;a=blob_plain;f=README
CBM::IOErrorMessage ChangeDirectory::process(const QString &params, Interface &iface)
{
	Log(FACDOS, info, QString("Changing to directory: %1").arg(params));
	return iface.openFile(params);
} // ChangeDirectory


CBM::IOErrorMessage MakeDirectory::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MakeDirectory


CBM::IOErrorMessage RemoveDirectory::process(const QString& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // RemoveDirectory

} // namespace CBMDos

