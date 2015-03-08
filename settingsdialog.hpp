#ifndef SETTINGSDIALOG_HPP
#define SETTINGSDIALOG_HPP

#include <QDialog>
#include <QString>
#include <QIntValidator>

#include <QtSerialPort/QtSerialPort>

typedef QList<QSerialPortInfo> QPortInfoList;

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
	uint srqInPin;
	QString imageFilters;
	QString imageDirectory;
	QString lastSpecificMounted;
	bool showDirectories;
	QString programVersion;
	QString cbmMachine;
	QString emulatorPalette;
	ushort cbmBorderWidth;
};


class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QPortInfoList& ports, AppSettings& settings, QWidget *parent = 0);
	~SettingsDialog();

private slots:
	void on_Ok_clicked();

	void on_browseImageDir_clicked();

private:
	Ui::SettingsDialog* ui;
	AppSettings& m_settings;
	QIntValidator m_borderValidator;
};

#endif // SETTINGSDIALOG_HPP
