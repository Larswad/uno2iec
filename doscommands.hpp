#ifndef DOSCOMMANDS_HPP
#define DOSCOMMANDS_HPP

#include <QString>
#include <QList>
#include <QChar>
#include <QStringList>

#include "logger.hpp"
#include "filedriverbase.hpp"

class Interface;

namespace CBMDos {

class Command;

typedef QList<Command*> CommandList;

// Interface and base class to all CBM DOS command parsing / processing implementations.
class Command
{
public:
	// The full command string, case doesn't matter. It is mandatory. Variants separated by pipe characters.
	virtual const QString full() = 0;
	// If this one returns a normal character, like ':', it specifies the ending of the command and
	// where the actual parameters begin. If is returns zero (base default) the command has NO parameters.
	// If it returns a QChar in the valuerange 1 - 31 it specifies the number of expected raw parameter
	// bytes following right after the command itself.
	virtual const QChar delimeter()
	{
		return QChar();
	}

	// perform the actual processing of the command itself.
	virtual CBM::IOErrorMessage process(const QByteArray& params, Interface& iface) = 0;

	// Static methods for command implementations.
	static void attach(Command* cmd)
	{
		s_attached.append(cmd);
	}

	static CommandList& attached()
	{
		return s_attached;
	}

	// find a command implementation that matches the given command string either by full name or abbreviation variants
	// and return it. The command will be stripped by leading command identifier, delimeter and returned in params.
	static Command* find(const QByteArray& cmdArray, QByteArray& params)
	{
		bool found = false; // be pessimistic

		// Get an instance as a string to make it easier to compare with.
		const QString cmdString(cmdArray);
		foreach(Command* cmd, s_attached) {
			QStringList variants(cmd->full().split('|'));
			if(cmd->delimeter().isNull()) {
				bool isFirst = true;
				foreach (const QString& v, variants) {
					if(cmdString.startsWith(v, isFirst ? Qt::CaseSensitive : Qt::CaseInsensitive)) {
						params = cmdArray.mid(v.length());
						found = true;
						break;
					}
					isFirst = false;
				}
			}
			else {
				QList<QByteArray> splits(cmdArray.split(cmd->delimeter().toLatin1()));
				if(not splits.isEmpty()) {
					foreach (const QString& v, variants) {
						if(splits.first() == v) {
							found = true;
							break;
						}
					}
					if(found) {
						splits.removeFirst();
						// join the parts again, wbut without first command part and its delimeter.
						foreach(const QByteArray& split, splits) {
							if(not params.isEmpty())
								params.append(cmd->delimeter());
							params.append(split);
						}
					}
				}
			}
			if(found)
				return cmd;
		}

		// none found.
		return 0;
	} // find


	// Find and execute the given command.
	static CBM::IOErrorMessage execute(const QByteArray& cmdString, Interface& iface)
	{
		QByteArray params, stripped(cmdString);
		// Strip off any trailing whitespace (in fact, CR for e.g. OPEN 1,8,15,"I:" which generates a CR.
		while(stripped.endsWith(QChar('\r').toLatin1()) or stripped.endsWith(QChar(' ').toLatin1()))
			stripped.chop(1);
		Command* dosCmd = find(stripped, params);
		if(0 not_eq dosCmd)
			return dosCmd->process(params, iface);

		return CBM::ErrCommandNotFound;
	} // execute


protected:
	// Helpers
	virtual void attachMe() {}
	static bool isIllegalCBMName(const QString& name)
	{
		return name.indexOf(QRegExp("m[=\"*?,]")) not_eq -1;
	}

private:
	static CommandList s_attached;
};

#define DECLARE_DOSCMD_IMPL(NAME, FULL, DELIM) \
	class NAME : public Command { \
		public:NAME() { attach(this); } \
		const QString full() { return FULL; } \
		const QChar delimeter() { return DELIM; } \
		CBM::IOErrorMessage process(const QByteArray& params, Interface& iface); \
	}


// Reset the 1541 to power-up condition.
// Syntax: "INITIALIZE"
DECLARE_DOSCMD_IMPL(InitDrive, "INITIALIZE|I|UJ|I0", QChar());

// Reset the 1541 to power-up condition.
// Syntax: "VALIDATE"
// Validate will fix inconsistencies that can be caused by files that where opened but never closed.
// Beware: Validate also erases all random files!
DECLARE_DOSCMD_IMPL(ValidateDisk, "VALIDATE|V|V0", QChar());


// Format a floppy disk
// Syntax: "NEW:<diskname>,<id>"
// Where <diskname> can be up to 16 characters long and <id> can either be omitted
// (only the directory is erased on a pre-formatted disk) or must be exactly 2 characters long.
DECLARE_DOSCMD_IMPL(NewDisk, "NEW|N|N0", ':');


// Delete files
// Syntax: "SCRATCH:<file>"
// You can use the wild cards '?' and '*' to delete several files at once.
DECLARE_DOSCMD_IMPL(Scratch, "SCRATCH|S|S0", ':');


// Rename a file
// Syntax: "RENAME:<newname>=<oldname>"
DECLARE_DOSCMD_IMPL(RenameFile, "RENAME|R|R0", ':');


// Rename a file
// Syntax: "COPY:<destfile>=<sourcefile>" or "COPY:<destfile>=<sourcefile1>, <sourcefile2>, ..."
// If several source files are listed, than the destination file will contain the concatenated contents of all source files.
DECLARE_DOSCMD_IMPL(CopyFiles, "COPY|C|C0", ':');


// Set Position - Change the Read/Write Position in a Relative File
// Syntax: "P"+CHR$(Channel)+CHR$(RecLow)+CHR$(RecHi)+CHR$(Pos)
DECLARE_DOSCMD_IMPL(SetPosition, "POSITION|P", QChar());

// BLOCK-READ - Read a Disk Block into the internal floppy RAM
// Abbreviation: U1 (superseded by USER1, U1)
// USER1 works like BLOCK-READ with the exception that U1 considers the link to the next block to be part of the data.
// Thus a block read with U1 will be 256 (rather than max. 254) bytes long.
// Syntax: "B-R:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
DECLARE_DOSCMD_IMPL(BlockRead, "B-R|U1", ':');



// BLOCK-WRITE - Write a Disk Block from the internal floppy RAM
// Abbreviation: U1 (superseded by USER1, U1)
// USER2 works like BLOCK-WRITE with the exception that U2 considers the link to the next block to be part of the data.
// Thus a block written with U2 has to be 256 (rather than max. 254) bytes long.
// Syntax: "B-W:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
DECLARE_DOSCMD_IMPL(BlockWrite, "B-W|U2", ':');


// MEMORY-READ - Read Data from the floppy RAM
// Abbreviation: M-R (You must use the abbreviation, the full form is not legal).
// Syntax: "M-R"+CHR$(LowAddress)+CHR$(HighAddress)+CHR$(Size)
DECLARE_DOSCMD_IMPL(MemoryRead, "M-R", QChar());


// MEMORY-WRITE - Write Data to the floppy RAM
// Abbreviation: M-W (You must use the abbreviation, the full form is not legal).
// Syntax: "M-W"+CHR$(LowAddress)+CHR$(HighAddress)+CHR$(Size)+payload[Size]
DECLARE_DOSCMD_IMPL(MemoryWrite, "M-W", QChar());


// BUFFER-POINTER - Set the pointer for a buffered block
// Abbreviation: B-P
// Syntax: "B-P:"+STR$(Channel)+STR$(Pos)
DECLARE_DOSCMD_IMPL(BufferPointer, "BUFFER-POINTER|B-P", ':');

// BLOCK-ALLOCATE - Mark a disk block as used
// Abbreviation: B-A
// Syntax: "B-A:"+STR$(Drive)+STR$(Track)+STR$(Sector)
DECLARE_DOSCMD_IMPL(BlockAllocate, "BLOCK-ALLOCATE|B-A", ':');

// BLOCK-FREE - Mark a disk block as unused
// Abbreviation: B-F
// Syntax: "B-F:"+STR$(Channel)+STR$(Pos)
DECLARE_DOSCMD_IMPL(BlockFree, "BLOCK-FREE|B-F", ':');

// BLOCK-EXECUTE - Read a Disk Block into the internal floppy and execute it. Abbreviation: B-E
// Syntax: "B-E:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
// Note: Block Execute obviously requires the complete emulation of the MOS 6502 CPU in the 1541.
// This is the place to check if a turbo loader should be enabled.
DECLARE_DOSCMD_IMPL(BlockExecute, "BLOCK-EXECUTE|B-E", ':');


// MEMORY-EXECUTE - Run a User Program on the Floppy
// Abbreviation: M-E (You must use the abbreviation, the full form is not legal).
// Abbreviation: U3..U8
// For U3..U8: Program execution starts at $0500 + 3*(x-3) (i.e. $0500 for U3, $0503 for U4...)
// Syntax: "M-E"+CHR$(LowAddress)+CHR$(HighAddress)
// Note: Memory Execute obviously requires the complete emulation of the MOS 6502 CPU in the 1541.
// This is the place to check if a turbo loader should be enabled.
DECLARE_DOSCMD_IMPL(MemoryExecute, "M-E|USER3|U3|USER4|U4|USER5|U5|USER6|U6|USER7|U7", QChar());


// USERI - Switch the C1541 between C64 to VC20 mode
// Abbreviation: UI
// Syntax: "UI+" or "UI-"
// This is a dummy function in Power64. It causes a slight adjustment of transfer speeds on a real C1541.
DECLARE_DOSCMD_IMPL(VC20ModeOnOff, "USERI|UI", QChar());


//////////////////////////////////////////////////////////////////////////////////
// SD2IEC Specific command support follows here:
//////////////////////////////////////////////////////////////////////////////////

// U0>
// Syntax: "U0>"+CHR$(new address)
DECLARE_DOSCMD_IMPL(DeviceAddress, "U0>", QChar());


// CD is also used to mount/unmount image files. Just change into them
// as if they were a directory and use CD:_ (left arrow on the C64) to leave.
// Please note that image files are detected by file extension and file size
// and there is no reliable way to see if a file is a valid image file.
//Subdirectory access is compatible to the syntax used by the CMD drives,
// although drive/partition numbers are completely ignored.

// Quick syntax overview:
//  CD:_         changes into the parent dir (_ is the left arrow on the C64)
//  CD_          dito
//  CD:foo       changes into foo
//  CD/foo       dito
//  CD//foo      changes into \foo
//  CD/foo/:bar  changes into foo\bar
//  CD/foo/bar   dito
DECLARE_DOSCMD_IMPL(ChangeDirectory, "CHDIR|CD", QChar());

// MD uses a syntax similiar to CD and will create the directory listed
// after the colon (:) relative to any directory listed before it.
//  MD/foo/:bar  creates bar in foo
//  MD//foo/:bar creates bar in \foo
DECLARE_DOSCMD_IMPL(MakeDirectory, "MAKEDIR|MD", QChar());

// RD can only remove subdirectories of the current directory.
// RD:foo       deletes foo
DECLARE_DOSCMD_IMPL(RemoveDirectory, "RMDIR|RD", QChar());


// PARTITION - Create or Select a Partition on a 1581 floppy disk
// This command works only on 1581 disks, but not on 1541 or 1571 disks.
// Abbreviation: / (You must use the abbreviation, the full form is not legal).
// Depending on the parameters given to the command it has a different semantic.
// Syntax:
// a) "/:<Partition>" - Select a given partition
// b) "/" - Return to the root directory of the floppy. Going up just one level is not possible.
// c) "/:<Partition>,"+CHR$(StartTrack)+CHR$(StartSector)+
// CHR$(BlockCntLow)+CHR$(BlockCntHigh)+",C"
// Create a new partition of BlockCnt blocks starting at StartTrack/StartSector.
// This partition is originally just an unstructured collection of blocks. To actually use a partition it is necessary to change to that partition and format it first.
// A partition has to meet some requirements to be suitable for a subdirectory
// .) It must be at least 120 blocks in size
// .) Its size must be a multiple of 40 blocks
// .) It must start at sector 0 of a track

// Not Implemented!

// T-R, T-W: Well, we'll see if that makes sense for implementation. Perhaps T-R (RTC time read) for compatability. T-W would
// in such case only modify time on the arduino side, not on host system.
// Obviously this project doesn't allow for the DI / DR / DW commands, it would breach host system file system security.

} // namespace CBMDos

#endif // DOSCOMMANDS_HPP
