#include "doscommands.hpp"
#include "interface.hpp"
#include "d64driver.hpp"
#include "logger.hpp"

DosCommands DosCommand::s_allCommands;
using namespace Logging;

namespace {
QString FACDOS("DOSCMD");
NewDisk newDiskCmd;
Scratch scratchFileCmd;
ChangeDirectory chDirCmd;
}


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
	if(!iface.currentFileDriver()->deleteFile(params))
		return CBM::ErrFileNotFound; // should be the proper reason of course.
	return CBM::ErrFilesScratched;
} // Scratch

/*
- CD/MD/RD:
	Subdirectory access is compatible to the syntax used by the CMD drives,
	although drive/partition numbers are completely ignored.

	Quick syntax overview:
		CD:_         changes into the parent dir (_ is the left arrow on the C64)
		CD_          dito
		CD:foo       changes into foo
		CD/foo       dito
		CD//foo      changes into \foo
		CD/foo/:bar  changes into foo\bar
		CD/foo/bar   dito
*/
// This is totally f'ed up right now. Fix so that it works as sd2iec.
// http://sd2iec.de/cgi-bin/gitweb.cgi?p=sd2iec.git;a=blob_plain;f=README

CBM::IOErrorMessage ChangeDirectory::process(const QString &params, Interface &iface)
{
	Log(FACDOS, info, QString("Changing to directory: %1").arg(params));
	return iface.openFile(params);
} // ChangeDirectory
