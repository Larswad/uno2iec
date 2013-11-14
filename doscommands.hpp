#ifndef DOSCOMMANDS_HPP
#define DOSCOMMANDS_HPP

#include <QString>
#include <QList>
#include <QChar>
#include <QStringList>
#include <logger.hpp>

#include "filedriverbase.hpp"

class Interface;

namespace CBMDos {

class Command;

typedef QList<Command*> CommandList;

// Interface and base class to all CBM DOS command parsing / processing implementations.
class Command
{
public:
	// The full command string, case doesn't matter. It is mandatory.
	virtual const QString full() = 0;
	// alternate command string (or abbreviation, returns empty string if the command has no abbreviation).
	virtual const QString abbrev()
	{
		return QString();
	}

	// If this one returns a normal character, like ':', it specifies the ending of the command and
	// where the actual parameters begin. If is returns zero (base default) the command has NO parameters.
	// If it returns a QChar in the valuerange 1 - 31 it specifies the number of expected raw parameter
	// bytes following right after the command itself.
	virtual const QChar delimeter()
	{
		return QChar();
	}

	// perform the actual processing of the command itself.
	virtual CBM::IOErrorMessage process(const QString& params, Interface& iface) = 0;

	// Static methods for command implementations.
	static void attach(Command* cmd)
	{
		s_attached.append(cmd);
	}

	static CommandList& attached()
	{
		return s_attached;
	}

	// find a command implementation that matches the given command string either by full name or abbreviation
	// and return it. The command will be stripped by leading command identifier and delimeter and returned in params.
	static Command* find(const QString& cmdString, QString& params)
	{
		bool found = false;

		foreach(Command* cmd, s_attached) {
			if(cmd->delimeter().isNull()) {
				if(cmdString.startsWith(cmd->full())) {
					 params = cmdString.mid(cmd->full().length());
					 found = true;
				}
				else if(not cmd->abbrev().isEmpty() and cmdString.startsWith(cmd->abbrev(), Qt::CaseInsensitive)) {
					params = cmdString.mid(cmd->abbrev().length());
					found = true;
				}
			}
			else {
				QStringList splits(cmdString.split(cmd->delimeter()));
				if(not splits.isEmpty() and (splits.first() == cmd->full() or splits.first() == cmd->abbrev())) {
					splits.removeFirst();
					params = splits.join(cmd->delimeter());
					found = true;
				}
			}
			if(found)
				return cmd;
		}

		// none found.
		return 0;
	} // find


	// Find and execute the given command.
	static CBM::IOErrorMessage execute(const QString& cmdString, Interface& iface)
	{
		QString params, stripped(cmdString);
		// Strip of trailing whitespace (in fact, CR for e.g. OPEN 1,8,15,"I:" which generates a CR.
		while(stripped.endsWith(QChar('\r')) or stripped.endsWith(QChar(' ')))
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
		return name.indexOf(QRegExp("m[=\"*?,]"));
	}

private:
	static CommandList s_attached;
};

#define DECLARE_DOSCMD_IMPL(NAME, FULL) \
	public:NAME() { attach(this); } \
	const QString full() { return FULL; } \
	CBM::IOErrorMessage process(const QString& params, Interface& iface); \
	protected: void attachMeB() {	attach(this); }


// Reset the 1541 to power-up condition.
// Syntax: "INITIALIZE"
class InitDrive : public Command
{
public:
	const QString abbrev()
	{
		return "I"; // or "UJ"
	}

	DECLARE_DOSCMD_IMPL(InitDrive, "INITIALIZE")
};


// Reset the 1541 to power-up condition.
// Syntax: "VALIDATE"
// Validate will fix inconsistencies that can be caused by files that where opened but never closed.
// Beware: Validate also erases all random files!
class ValidateDisk : public Command
{
public:
	const QString abbrev()
	{
		return "V";
	}

	DECLARE_DOSCMD_IMPL(ValidateDisk, "VALIDATE")
};


// Format a floppy disk
// Syntax: "NEW:<diskname>,<id>"
// Where <diskname> can be up to 16 characters long and <id> can either be omitted
// (only the directory is erased on a pre-formatted disk) or must be exactly 2 characters long.
class NewDisk : public Command
{
public:
	const QString abbrev()
	{
		return "N";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(NewDisk, "NEW")
};


// Delete files
// Syntax: "SCRATCH:<file>"
// You can use the wild cards '?' and '*' to delete several files at once.
class Scratch : public Command
{
public:
	const QString abbrev()
	{
		return "S";
	}
	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(Scratch, "SCRATCH")
};


// Rename a file
// Syntax: "RENAME:<newname>=<oldname>"
class RenameFile : public Command
{
public:
	const QString abbrev()
	{
		return "R";
	}
	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(RenameFile, "RENAME")
};


// Rename a file
// Syntax: "COPY:<destfile>=<sourcefile>" or "COPY:<destfile>=<sourcefile1>, <sourcefile2>, ..."
// If several source files are listed, than the destination file will contain the concatenated contents of all source files.
class CopyFiles : public Command
{
public:
	const QString abbrev()
	{
		return "C";
	}
	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(CopyFiles, "COPY")
};


// Set Position - Change the Read/Write Position in a Relative File
// Syntax: "P"+CHR$(Channel)+CHR$(RecLow)+CHR$(RecHi)+CHR$(Pos)
class SetPosition : public Command
{
public:
	const QString abbrev()
	{
		return "P";
	}

	DECLARE_DOSCMD_IMPL(SetPosition, "POSITION")
};


// BLOCK-READ - Read a Disk Block into the internal floppy RAM
// Abbreviation: U1 (superseded by USER1, U1)
// USER1 works like BLOCK-READ with the exception that U1 considers the link to the next block to be part of the data.
// Thus a block read with U1 will be 256 (rather than max. 254) bytes long.
// Syntax: "B-R:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
class BlockRead : public Command
{
public:
	const QString abbrev()
	{
		return "U1";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BlockRead, "B-R")
};



// BLOCK-WRITE - Write a Disk Block from the internal floppy RAM
// Abbreviation: U1 (superseded by USER1, U1)
// USER2 works like BLOCK-WRITE with the exception that U2 considers the link to the next block to be part of the data.
// Thus a block written with U2 has to be 256 (rather than max. 254) bytes long.
// Syntax: "B-W:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
class BlockWrite : public Command
{
public:
	const QString abbrev()
	{
		return "U2";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BlockWrite, "B-W")
};


// MEMORY-READ - Read Data from the floppy RAM
// Abbreviation: M-R (You must use the abbreviation, the full form is not legal).
// Syntax: "M-R"+CHR$(LowAddress)+CHR$(HighAddress)+CHR$(Size)
class MemoryRead : public Command
{
public:
	DECLARE_DOSCMD_IMPL(MemoryRead, "M-R")
};


// MEMORY-WRITE - Write Data to the floppy RAM
// Abbreviation: M-W (You must use the abbreviation, the full form is not legal).
// Syntax: "M-W"+CHR$(LowAddress)+CHR$(HighAddress)+CHR$(Size)
class MemoryWrite : public Command
{
public:
	DECLARE_DOSCMD_IMPL(MemoryWrite, "M-W")
};


// BUFFER-POINTER - Set the pointer for a buffered block
// Abbreviation: B-P
// Syntax: "B-P:"+STR$(Channel)+STR$(Pos)
class BufferPointer : public Command
{
public:
	const QString abbrev()
	{
		return "B-P";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BufferPointer, "BUFFER-POINTER")
};

// BLOCK-ALLOCATE - Mark a disk block as used
// Abbreviation: B-A
// Syntax: "B-A:"+STR$(Drive)+STR$(Track)+STR$(Sector)
class BlockAllocate : public Command
{
public:
	const QString abbrev()
	{
		return "B-A";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BlockAllocate, "BLOCK-ALLOCATE")
};

// BLOCK-FREE - Mark a disk block as unused
// Abbreviation: B-F
// Syntax: "B-F:"+STR$(Channel)+STR$(Pos)
class BlockFree : public Command
{
public:
	const QString abbrev()
	{
		return "B-F";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BlockFree, "BLOCK-FREE")
};

// BLOCK-EXECUTE - Read a Disk Block into the internal floppy and execute it. Abbreviation: B-E
// Syntax: "B-E:"+STR$(Channel)+STR$(Drive)+STR$(Track)+STR$(Sector)
// Note: Block Execute obviously requires the complete emulation of the MOS 6502 CPU in the 1541.
// This is the place to check if a turbo loader should be enabled.
class BlockExecute : public Command
{
public:
	const QString abbrev()
	{
		return "B-E";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(BlockExecute, "BLOCK-EXECUTE")
};


// MEMORY-EXECUTE - Run a User Program on the Floppy
// Abbreviation: M-E (You must use the abbreviation, the full form is not legal).
// Abbreviation: U3..U8
// For U3..U8: Program execution starts at $0500 + 3*(x-3) (i.e. $0500 for U3, $0503 for U4...)
// Syntax: "M-E"+CHR$(LowAddress)+CHR$(HighAddress)
// Note: Memory Execute obviously requires the complete emulation of the MOS 6502 CPU in the 1541.
// This is the place to check if a turbo loader should be enabled.
class MemoryExecute : public Command
{
public:
	const QString abbrev()
	{
		return "USER3"; // USER3 U3 USER4 U4 USER5 U5 USER6 U6 USER7 U7
	}

	DECLARE_DOSCMD_IMPL(MemoryExecute, "M-E")
};


// USERI - Switch the C1541 between C64 to VC20 mode
// Abbreviation: UI
// Syntax: "UI+" or "UI-"
// This is a dummy function in Power64. It causes a slight adjustment of transfer speeds on a real C1541.
class VC20ModeOnOff : public Command
{
public:
	const QString abbrev()
	{
		return "UI";
	}
	DECLARE_DOSCMD_IMPL(VC20ModeOnOff, "USERI")
};


//////////////////////////////////////////////////////////////////////////////////
// SD2IEC Specific command support follows here:
//////////////////////////////////////////////////////////////////////////////////

// U0>
// Syntax: "U0>"+CHR$(new address)
class DeviceAddress : public Command
{
public:
	DECLARE_DOSCMD_IMPL(DeviceAddress, "U0>")
};


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
class ChangeDirectory : public Command
{
public:
	const QString abbrev()
	{
		return "CD";
	}

	DECLARE_DOSCMD_IMPL(ChangeDirectory, "CHDIR")
};

// MD uses a syntax similiar to CD and will create the directory listed
// after the colon (:) relative to any directory listed before it.
//  MD/foo/:bar  creates bar in foo
//  MD//foo/:bar creates bar in \foo
class MakeDirectory : public Command
{
public:
	const QString abbrev()
	{
		return "MD";
	}

	DECLARE_DOSCMD_IMPL(MakeDirectory, "MAKEDIR")
};

// RD can only remove subdirectories of the current directory.
// RD:foo       deletes foo
class RemoveDirectory : public Command
{
public:
	const QString abbrev()
	{
		return "RD";
	}

	DECLARE_DOSCMD_IMPL(RemoveDirectory, "RMDIR")
};


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
