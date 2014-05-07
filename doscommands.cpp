#include "doscommands.hpp"
#include "interface.hpp"
#include "d64driver.hpp"
#include "logger.hpp"
#include "utils.hpp"

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


CBM::IOErrorMessage InitDrive::process(const QByteArray& params, Interface& iface)
{
	// Either ignore or check that no parameters are given here!
	Q_UNUSED(params);
	return iface.reset();
} // InitDrive


CBM::IOErrorMessage ValidateDisk::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // ValidateDisk


CBM::IOErrorMessage NewDisk::process(const QByteArray &params, Interface &iface)
{
	CBM::IOErrorMessage result;
	QStringList newParams(QString(params).split(',', QString::SkipEmptyParts));

	if(newParams.empty() or newParams.count() > 2)
		return CBM::ErrSyntaxError;
	if(iface.isDiskWriteProtected())
		return CBM::ErrWriteProtectOn;

	FileDriverBase* driver = NULL;
	const QString label(newParams[0]);
	bool hasExt = hasExtension(label);
	// prefer using current driver...
	if(NULL not_eq iface.currentFileDriver()) {
		// ...but only if it supports the extension or the disk label was specified without extension.
		if(iface.currentFileDriver()->supportsType(label) or not hasExt)
			driver = iface.currentFileDriver();
	}
	if(NULL == driver and hasExt)
		driver = iface.driverForFile(label);

	if(driver) {
		// if we got a driver, use that to do the newdisk but without extension.
		QString id(newParams.count() > 1 ? newParams[1] : QString());
		result = driver->newDisk(withoutExtension(label), id);
	}
	else
		result = CBM::ErrDriveNotReady;	// Probably best error suitable for this situation.
	return result;
} // NewDisk


CBM::IOErrorMessage Scratch::process(const QByteArray &params, Interface &iface)
{
	const QString file(params);
	if(iface.isDiskWriteProtected())
		return CBM::ErrWriteProtectOn;
	// TODO: Check that there is no path stuff in the name, we don't like that here.
	// TODO: Support drive number (e.g. S0:<file>)
	Log(FACDOS, info, QString("About to scratch file: %1").arg(file));
	return iface.currentFileDriver()->deleteFile(file) ? CBM::ErrFilesScratched : CBM::ErrFileNotFound;
} // Scratch


CBM::IOErrorMessage RenameFile::process(const QByteArray& params, Interface& iface)
{
	const QStringList names(QString(params).split(QChar('=')));

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

	if(iface.isDiskWriteProtected())
		return CBM::ErrWriteProtectOn;

	// NOTE: Need to check here whether the file is renamed across paths or not?

	Log(FACDOS, info, QString("About to rename file: %1 to %2").arg(oldName, newName));

	// Let the current file system decide whether it goes well by doing the rename operation.
	return iface.currentFileDriver()->renameFile(oldName, newName);
} // RenameFile


CBM::IOErrorMessage CopyFiles::process(const QByteArray& params, Interface& iface)
{
	Log(FACDOS, info, QString("Reached copy1"));

	const QStringList paramList(QString(params).split(QChar('=')));
	// destination name and source name(s) parameters must be present, no more no less.
	if(2 not_eq paramList.count())
		return CBM::ErrSyntaxError;
	const QString destName(paramList[0]);
	// destination name must not be empty it they must have legal characters.
	if(destName.isEmpty())
		return CBM::ErrNoFileGiven;
	Log(FACDOS, info, QString("Reached copy2:%1").arg(destName));
	if(isIllegalCBMName(destName))
		return CBM::ErrSyntaxError;
	Log(FACDOS, info, QString("Reached copy3"));
	if(iface.isDiskWriteProtected())
		return CBM::ErrWriteProtectOn;

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


CBM::IOErrorMessage SetPosition::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // SetPosition


CBM::IOErrorMessage BlockRead::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockRead


CBM::IOErrorMessage BlockWrite::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockWrite


CBM::IOErrorMessage MemoryRead::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MemoryReadCmd


CBM::IOErrorMessage MemoryWrite::process(const QByteArray& params, Interface& iface)
{
	if(params.length() < 3)
		return CBM::ErrSyntaxError;
	ushort address = ((ushort)params.at(1)) << 8 bitor ((uchar)params.at(0));
	ushort length = (uchar)params.at(2);
	QByteArray bytes(params.mid(3));
	Log(FACDOS, warning, QString("M-W 0x%1:%2").arg(QString::number(address, 16))
		.arg(QString::number(length)));
	// What to do if the designated length doesn't match with the actual received amount of bytes?
	if(bytes.length() not_eq length) {
		// resize and don't expect more, temporary solution?
		bytes.resize(length);
		// TODO: if we fix this, here we need to know that succeeding bytes are meant to be written with M-W.
		// and we need to know how many is left and where.
	}
	// Do the actual memory write.
	iface.writeDriveMemory(address, bytes);

	return CBM::ErrOK;
} // MemoryWrite


CBM::IOErrorMessage BufferPointer::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BufferPointer


CBM::IOErrorMessage BlockAllocate::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockAllocate


CBM::IOErrorMessage BlockFree::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockFree


CBM::IOErrorMessage BlockExecute::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // BlockExecute


CBM::IOErrorMessage MemoryExecute::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MemoryExecute


CBM::IOErrorMessage VC20ModeOnOff::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // VC20ModeOnOff


CBM::IOErrorMessage DeviceAddress::process(const QByteArray& params, Interface& iface)
{
	bool status;
	int newDriveNum(QString(params).toInt(&status));
	// only allow the valid range for this.
	if(not status or newDriveNum < 4 or newDriveNum > 30)
		return CBM::ErrSyntaxError;
	Log(FACDOS, success, QString("Now changing device adress from #%1 to #%2").arg(iface.deviceNumber()).arg(newDriveNum));
	iface.setDeviceNumber(newDriveNum);

	return CBM::ErrOK;
} // DeviceAddress


// This is totally f'ed up right now. Fix so that it works as sd2iec.
// http://sd2iec.de/cgi-bin/gitweb.cgi?p=sd2iec.git;a=blob_plain;f=README
CBM::IOErrorMessage ChangeDirectory::process(const QByteArray &params, Interface &iface)
{
	Log(FACDOS, info, QString("Changing to directory: %1").arg(QString(params)));
	return iface.openFile(params);
} // ChangeDirectory


CBM::IOErrorMessage MakeDirectory::process(const QByteArray& params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // MakeDirectory


CBM::IOErrorMessage RemoveDirectory::process(const QByteArray &params, Interface& iface)
{
	Q_UNUSED(params);
	Q_UNUSED(iface);
	return CBM::ErrNotImplemented;
} // RemoveDirectory

} // namespace CBMDos

