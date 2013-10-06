#ifndef DOSCOMMANDS_HPP
#define DOSCOMMANDS_HPP

#include <QString>
#include <QList>
#include <QChar>
#include <QStringList>
#include <logger.hpp>

#include "filedriverbase.hpp"

class DosCommand;
class Interface;
using namespace Logging;

typedef QList<DosCommand*> DosCommands;

// Interface and base class to all CBM DOS command parsing / processing implementations.
class DosCommand
{
public:
	// The full command string, case doesn't matter. It is mandatory.
	virtual const QString full() = 0;
	// alternate command string (or abbreviation, returns empty string if the command has no abbreviation).
	virtual const QString abbrev() = 0;
//	{
//		return QString();
//	}

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
	static void registerCmd(DosCommand* cmd)
	{
		s_allCommands.append(cmd);
	}

	static DosCommands& allCommands()
	{
		return s_allCommands;
	}

	static DosCommand* findCmd(const QString& cmdString, QString& params)
	{
		foreach(DosCommand* cmd, s_allCommands) {
			QStringList splits(cmdString.split(cmd->delimeter()));
			if(!splits.isEmpty() and (splits.first() == cmd->full() or splits.first() == cmd->abbrev())) {
				splits.removeFirst();
				params = splits.join(cmd->delimeter());
				return cmd;
			}
		}
		// none found.
		return 0;
	} // findCmd


	static CBM::IOErrorMessage executeCmd(const QString& cmdString, Interface& iface)
	{
		QString params;
		DosCommand* dosCmd = findCmd(cmdString, params);
		if(0 not_eq dosCmd)
			return dosCmd->process(params, iface);

		return CBM::ErrCommandNotFound;
	} // executeCmd

protected:
	virtual void registerMe() {}

private:
	static DosCommands s_allCommands;
};

#define DECLARE_DOSCMD_IMPL(NAME, FULL) \
	public:NAME() { registerCmd(this); } \
	const QString full() { return FULL; } \
	CBM::IOErrorMessage process(const QString& params, Interface& iface); \
	protected: void registerMe() {	registerCmd(this); }

class NewDisk : public DosCommand
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


class ChangeDirectory : public DosCommand
{
public:
	const QString abbrev()
	{
		return "CD";
	}

	const QChar delimeter()
	{
		return ':';
	}

	DECLARE_DOSCMD_IMPL(ChangeDirectory, "CHDIR")
};


class Scratch : public DosCommand
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

#endif // DOSCOMMANDS_HPP
