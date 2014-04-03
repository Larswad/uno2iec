#ifndef SETTINGSDIALOG_HPP
#define SETTINGSDIALOG_HPP

#include <QDialog>
#include <QString>
#include "qextserialenumerator.h"

typedef QList<QextPortInfo> QextPortInfoList;

namespace Ui {
class SettingsDialog;
}


struct AppSettings {
	ushort deviceNumber;
	uint baudRate;
	QString portName;
	uint atnPin;
	uint clockPin;
	uint dataPin;
	uint resetPin;
	QString imageFilters;
	QString imageDirectory;
	QString lastSpecificMounted;
	bool showDirectories;
	QString programVersion;
	QString cbmMachine;
	QString emulatorPalette;
};


class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QextPortInfoList& ports, AppSettings& settings, QWidget *parent = 0);
	~SettingsDialog();

private slots:
	void on_Ok_clicked();

	void on_browseImageDir_clicked();

private:
	Ui::SettingsDialog* ui;
	AppSettings& m_settings;
};

#endif // SETTINGSDIALOG_HPP
