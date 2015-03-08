#include <QFileDialog>
#include <iso646.h>

#include "settingsdialog.hpp"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QPortInfoList& ports, AppSettings& settings, QWidget *parent) :
	QDialog(parent), ui(new Ui::SettingsDialog), m_settings(settings), m_borderValidator(0, 100, this)
{
	ui->setupUi(this);

	foreach(QSerialPortInfo info, ports)
		ui->comPort->addItem(info.portName());

	ui->comPort->setCurrentIndex(ui->comPort->findText(m_settings.portName));
	ui->baudRate->setCurrentIndex(ui->baudRate->findText(QString::number(m_settings.baudRate)));
	ui->deviceNumber->setCurrentIndex(ui->deviceNumber->findText(QString::number(m_settings.deviceNumber)));
	ui->atnPin->setCurrentIndex(ui->atnPin->findText(QString::number(m_settings.atnPin)));
	ui->dataPin->setCurrentIndex(ui->dataPin->findText(QString::number(m_settings.dataPin)));
	ui->clockPin->setCurrentIndex(ui->clockPin->findText(QString::number(m_settings.clockPin)));
	ui->resetPin->setCurrentIndex(ui->resetPin->findText(QString::number(m_settings.resetPin)));
	ui->imageFilters->setText(m_settings.imageFilters);
	ui->showDirs->setChecked(m_settings.showDirectories);
	ui->imageDir->setText(m_settings.imageDirectory);
	ui->borderWidth->setValidator(&m_borderValidator);
	ui->borderWidth->setText(QString::number(m_settings.cbmBorderWidth));
} // ctor


SettingsDialog::~SettingsDialog()
{
	delete ui;
} // dtor


void SettingsDialog::on_Ok_clicked()
{
	m_settings.deviceNumber = ui->deviceNumber->currentText().toUInt();
	m_settings.baudRate = ui->baudRate->currentText().toUInt();
	m_settings.portName = ui->comPort->currentText();
	m_settings.atnPin = ui->atnPin->currentText().toUInt();
	m_settings.dataPin = ui->dataPin->currentText().toUInt();
	m_settings.clockPin = ui->clockPin->currentText().toUInt();
	m_settings.resetPin = ui->resetPin->currentText().toUInt();
	m_settings.imageFilters = ui->imageFilters->text();
	m_settings.showDirectories = ui->showDirs->isChecked();
	m_settings.imageDirectory = ui->imageDir->text();
	m_settings.cbmBorderWidth = ui->borderWidth->text().toUShort();

	accept();
} // on_Ok_clicked


void SettingsDialog::on_browseImageDir_clicked()
{
	const QString dirPath = QFileDialog::getExistingDirectory(this, tr("Folder for your D64/T64/PRG/SID images"), ui->imageDir->text());
	if(not dirPath.isEmpty())
		ui->imageDir->setText(dirPath);
} // on_browseImageDir_clicked()
