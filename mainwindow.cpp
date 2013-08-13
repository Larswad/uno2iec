// TODO: Finalize M2I handling. What exactly is the point of that FS, is it to handle 8.3 filenames to cbm 16 byte lengths?
// TODO: Finalize Native FS routines.
// TODO: Handle all data channel stuff. TALK, UNTALK, and so on.
// TODO: T64 / D64 formats should read out entire disk into memory for caching (network performance).

#include <QString>
#include <QFileDialog>
#include <QTextStream>
#include <QDate>
#include <QDebug>
#include <QSettings>

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "aboutdialog.hpp"
#include "version.h"

#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

using namespace Logging;

namespace {
#define CBM_BACK_ARROW 95
const QColor CCS64_BG_COLOR(64,64,224);
const QColor CCS64_FG_COLOR(160,160,255);

const QString OkString = "OK>";
const QColor logLevelColors[] = { QColor(Qt::red), QColor("orange"), QColor(Qt::blue), QColor(Qt::darkGreen) };

QStringList IMAGE_LIST_HEADERS = (QStringList()
															<< QObject::tr("Name")
															<< QObject::tr("Size (KiB)"));

QStringList LOG_LEVELS = (QStringList()
													<< QObject::tr("error  ")
													<< QObject::tr("warning")
													<< QObject::tr("info   ")
													<< QObject::tr("success"));

const uint DEFAULT_BAUDRATE = BAUD115200;

const uint DEFAULT_DEVICE_NUMBER = 8;
const uint DEFAULT_RESET_PIN = 7;
const uint DEFAULT_CLOCK_PIN = 4;
const uint DEFAULT_DATA_PIN = 3;
const uint DEFAULT_ATN_PIN = 5;
//const uint DEFAULT_SRQIN_PIN = 2;

const QString PROGRAM_VERSION_HISTORY = qApp->tr(
	"<hr>"
	"<ul><li>NEW: Added nicely fitting icons for all buttons."
	"<li>NEW: Completely revised UI with configuration dialog making main window much more clean. "
	"The dialog now also keeps settings for file filters and whether directories should be displayed. The settings dialog "
	"is accessed from the menu or with CTRL+O shortcut."
	"<li>NEW: Using Qt theming (styles). The default one will be Windows XP, otherwise Fusion."
	"<li>NEW: Made the event log sit in a dockable window that can be either hidden or detached from the main window. "
	"This will save a lot of screen real estate and gives space for much more files in the file list."
	"<li>NEW: The mounted image directory listing is now displayed in classic C64 colors. The image name selection in the file list "
	"is also automatically updated even if it is the CBM that triggers a new selection."
	"NEW: A refresh button has been added for the file list."
	"NEW: Directories are optionally displayed and in such case in bold font weight."
	"<li>BUGFIX: When application started it always selected the first com-port in the list even if another was selected when "
	"the application exited and the settings was saved. It now selects the right comport when application starts.</ul>"

	"<span style=\" font-weight:600; color:#0404c2;\">Application Version 0.2.1:</span><hr>"
	"<ul><li>FIX: Faster serial handling. The arduino receives a range of bytes instead of a single byte at a time. Should now be as fast as "
	" a real 1541, or at least close to. More optimizations on serial handling can be done."
	"<li>NEW: Added this about dialog. It will automatically show every time a new version is released. There is also an application icon "
	" and a version resource added for windows targets. A C64 truetype font is embedded to the application for some UI controls."
	"<li>CHANGE: Some source and README/NOTES commenting to shape up the things that is left to do."
	"<li>NEW: Added skeleton class for .x00 (e.g. .P00) formats."
	"<li>NEW: Displays current mounted image content, even automatically when CBM does the operations."
	"<li>NEW: Notification interface added so that UI can react to and reflect progress and mounting changes."
	"<li>NEW: Some rearrangement of UI controls for better screen alignment and adaptation to windows resizing.</ul>"
	);
} // unnamed namespace


MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow), m_port(QextSerialPort::EventDriven, this)
, m_isConnected(false), m_iface(m_port), m_isInitialized(false)
{
	ui->setupUi(this);

	m_dirListItemModel = new QStandardItemModel(0, IMAGE_LIST_HEADERS.count(), this);
	Q_ASSERT(m_dirListItemModel);
	ui->dirList->setModel(m_dirListItemModel);
	getLoggerInstance().AddTransport(this);

	// Set up the port basic parameters, these won't change...promise.
	m_port.setDataBits(DATA_8);
	m_port.setParity(PAR_NONE);
	m_port.setFlowControl(FLOW_OFF);
	m_port.setStopBits(STOP_1);

	enumerateComPorts();

	readSettings();
	m_port.setBaudRate(static_cast<BaudRateType>(m_appSettings.baudRate));
	usePortByFriendlyName(m_appSettings.portName);

	m_port.open(QIODevice::ReadWrite);
	Log("MAIN", QString("Application Started, using port %1 @ %2").arg(m_port.portName()).arg(QString::number(m_port.baudRate())), success);
	// we want events from the port.
	connect(&m_port, SIGNAL(readyRead()), this, SLOT(onDataAvailable()));
	ui->dockWidget->toggleViewAction()->setShortcut(QKeySequence("CTRL+L"));
	ui->menuMain->insertAction(ui->menuMain->actions().first(), ui->dockWidget->toggleViewAction());
	// Initialize WiringPI stuff, if we're on the Raspberry Pi platform.
#ifdef HAS_WIRINGPI
	system("/usr/local/bin/gpio -g mode 23 out");
	system("/usr/local/bin/gpio export 23 out");
	if(-1 == wiringPiSetupSys())
		Log("MAIN", "Failed initializing WiringPi. Continuing anyway...", error);
	else
		on_resetArduino_clicked();
#endif

	Log("MAIN", "Application Initialized.", success);
	m_isInitialized = true;
	updateImageList();

	// register ourselves to listen for all CBM events from the Arduino so that we can reflect this on UI controls.
	m_iface.setMountNotifyListener(this);
	m_iface.setImageFilters(m_appSettings.imageFilters, m_appSettings.showDirectories);
} // ctor


void MainWindow::enumerateComPorts()
{
	m_ports = QextSerialEnumerator::getPorts();

	// Manually add the ports that the enumerator can't know about.
#if defined(__arm__)
	// just for the Raspberry PI.
	static QextPortInfo piPort = { "/dev/ttyAMA0", "ttyAMA0", "Arduino AMA0", "", 0, 0 };
	m_ports.insert(0, piPort);
//	m_port.setBaudRate(BAUD57600/*BAUD1152000*/);
#elif defined(Q_OS_LINUX)
	static QextPortInfo unixPort = { "/dev/ttyACM0", "ttyACM0", "Arduino ACM0", "", 0, 0 };
	m_ports.insert(0, unixPort);
#endif
} // enumerateComPorts


void MainWindow::usePortByFriendlyName(const QString& friendlyName)
{
	foreach(QextPortInfo port, m_ports) {
		if(port.friendName == friendlyName) {
			// found it, set it and be done.
			m_port.setPortName(port.portName);
			break;
		}
	}
} // usePortByFriendlyName


MainWindow::~MainWindow()
{
	m_iface.setMountNotifyListener(0);
	if(m_port.isOpen())
		m_port.close();
	delete ui;
} // dtor


void MainWindow::on_actionAbout_triggered()
{
	QString aboutText(tr("<span style=\" font-weight:600; font-size:12pt;\">Qt based host application for Arduino to CBM over IEC.</span><br><br>"
											 "<span style=\" font-weight:600; color:#c20404;\">Application Version ") +
										m_appSettings.programVersion + tr("</span>"
																					"<span style =  font-style:italic; color:#c20404;\">: Current</span>") + PROGRAM_VERSION_HISTORY);

	AboutDialog about(aboutText, this);
	about.exec();
} // on_actionAbout_triggered


void MainWindow::on_actionSettings_triggered()
{
	enumerateComPorts();

	AppSettings oldSettings = m_appSettings;
	SettingsDialog settings(m_ports, m_appSettings);
	if(QDialog::Accepted == settings.exec()) { // user pressed Ok?
		if(m_appSettings.imageFilters not_eq oldSettings.imageFilters
			 or m_appSettings.showDirectories not_eq oldSettings.showDirectories) {
			m_iface.setImageFilters(m_appSettings.imageFilters, m_appSettings.showDirectories);
			updateImageList();
		}
		if(m_appSettings.baudRate not_eq oldSettings.baudRate)
			m_port.setBaudRate(static_cast<BaudRateType>(m_appSettings.baudRate));

		// Was port changed?
		if(m_appSettings.portName not_eq oldSettings.portName) {
			m_isConnected = false;
			if(m_port.isOpen())
				m_port.close();
			usePortByFriendlyName(m_appSettings.portName);
			m_port.open(QIODevice::ReadWrite);
			Log("MAIN", QString("Port name changed to %1").arg(m_port.portName()), info);
		}
	}
} // on_actionSettings_triggered


void MainWindow::checkVersion()
{
	if(m_appSettings.programVersion not_eq VER_PRODUCTVERSION_STR) {
		m_appSettings.programVersion = VER_PRODUCTVERSION_STR;
		on_actionAbout_triggered();
	}
} // checkVersion


void MainWindow::closeEvent(QCloseEvent* event)
{
	writeSettings();
	QMainWindow::closeEvent(event);
} // closeEvent


////////////////////////////////////////////////////////////////////////////
// Settings (persistency) management.
////////////////////////////////////////////////////////////////////////////

void MainWindow::readSettings()
{
	QSettings settings;

	ui->imageDir->setText(settings.value("imageDirectory", QDir::currentPath()).toString());
	ui->singleImageName->setText(settings.value("singleImageName").toString());
	QDir::setCurrent(ui->imageDir->text());
	ui->imageFilter->setText(settings.value("imageFilter", QString()).toString());
	m_appSettings.portName = settings.value("portName", m_ports.isEmpty() ? "COM1" : m_ports.first().friendName).toString();
	m_appSettings.baudRate = settings.value("baudRate", QString::number(DEFAULT_BAUDRATE)).toUInt();
	m_appSettings.deviceNumber = settings.value("deviceNumber", QString::number(DEFAULT_DEVICE_NUMBER)).toUInt();
	m_appSettings.atnPin = settings.value("atnPin", QString::number(DEFAULT_ATN_PIN)).toUInt();
	m_appSettings.clockPin = settings.value("clockPin", QString::number(DEFAULT_CLOCK_PIN)).toUInt();
	m_appSettings.dataPin = settings.value("dataPin", QString::number(DEFAULT_DATA_PIN)).toUInt();
	m_appSettings.resetPin = settings.value("resetPin", QString::number(DEFAULT_RESET_PIN)).toUInt();
	//	ui->srqInPin = settings.value("srqInPin", QString::number(DEFAULT_SRQIN_PIN)).toUInt();

	m_appSettings.imageFilters = settings.value("imageFilters", "*.D64,*.T64,*.M2I,*.PRG,*.P00,*.SID").toString();
	m_appSettings.showDirectories = settings.value("showDirectories", false).toBool();
	m_appSettings.programVersion = settings.value("lastProgramVersion", "unset").toString();
	restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
	restoreState(settings.value("mainWindowState").toByteArray());
} // readSettings


void MainWindow::writeSettings() const
{
	QSettings settings;
	settings.setValue("mainWindowGeometry", saveGeometry());
	settings.setValue("mainWindowState", saveState());
	settings.setValue("lastProgramVersion", m_appSettings.programVersion);

	settings.setValue("imageDirectory", ui->imageDir->text());
	settings.setValue("singleImageName", ui->singleImageName->text());
	settings.setValue("imageFilter", ui->imageFilter->text());
	settings.setValue("portName", m_appSettings.portName);
	settings.setValue("baudRate", QString::number(m_appSettings.baudRate));
	settings.setValue("deviceNumber", QString::number(m_appSettings.deviceNumber));
	settings.setValue("atnPin", QString::number(m_appSettings.atnPin));
	settings.setValue("clockPin", QString::number(m_appSettings.clockPin));
	settings.setValue("dataPin", QString::number(m_appSettings.dataPin));
//	settings.setValue("srqInPin", QString::number(m_appSettings.srqInPin));
	settings.setValue("imageFilters", m_appSettings.imageFilters);
	settings.setValue("showDirectories", m_appSettings.showDirectories);
} // writeSettings


////////////////////////////////////////////////////////////////////////////
// Dispatcher for when something has arrived on the serial port.
////////////////////////////////////////////////////////////////////////////
void MainWindow::onDataAvailable()
{
//	bool wasEmpty = m_pendingBuffer.isEmpty();
	m_pendingBuffer.append(m_port.readAll());

	if(!m_isConnected) {
		if(m_pendingBuffer.contains("CONNECT")) {
			m_pendingBuffer.clear();
			Log("MAIN", "Now connected to Arduino.", success);
			// give the client current date and time in the response string.
			m_port.write((OkString + QString::number(m_appSettings.deviceNumber) + '|' + QString::number(m_appSettings.atnPin) + '|' + QString::number(m_appSettings.clockPin)
										+ '|' + QString::number(m_appSettings.dataPin) + '|' + QString::number(m_appSettings.resetPin) /*+ '|' + QString::number(m_appSettings.srqInPin)*/
										+ '\r').toLatin1().data());
			// Assume connected, maybe a real ack sequence is needed here from the client?
			m_isConnected = true;
			// client is supposed to send it's facilities each start.
			m_clientFacilities.clear();
		}
		else // not yet connected, and no connect string so don't accept anything else now.
			return;
	}

	bool hasDataToProcess = !m_pendingBuffer.isEmpty();
	while(hasDataToProcess) {
		QString cmdString(m_pendingBuffer);
		int crIndex =	cmdString.indexOf('\r');

		// Get the first waiting character, which should be the commend to perform.
		switch(cmdString.at(0).toLatin1()) {
			case '!': // register facility string.
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {
					processAddNewFacility(cmdString.left(crIndex));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
				break;

			case 'D': // debug output.
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {
					processDebug(cmdString.left(crIndex));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
				break;

			case 'S': // request for file size in bytes before sending file to CBM
				m_pendingBuffer.remove(0, 1);
				m_iface.processGetOpenFileSize();
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			case 'O': // open command
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {// Open was issued, string goes from 1 to CRpos - 1
					m_iface.processOpenCommand(cmdString.mid(1, crIndex - 1));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
				break;

			case 'R': // read single byte from current file system mode, note that this command needs no termination, it needs to be short.
				m_pendingBuffer.remove(0, 1);
				m_iface.processReadFileRequest();
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			case 'W': // write single character to file in current file system mode.
				if(cmdString.size() < 1)
					hasDataToProcess = false;
				m_iface.processWriteFileRequest(m_pendingBuffer.at(1));
				m_pendingBuffer.remove(0, 2);
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			case 'L':
				// line request: Just remove the BYTE from queue and do business.
				m_pendingBuffer.remove(0, 1);
				m_iface.processLineRequest();
				break;

			case 'C': // close command
				m_pendingBuffer.remove(0, 1);
				m_iface.processCloseCommand();
				break;

			case 'E': // Ask for error string from error code
				if(cmdString.size() < 1)
					hasDataToProcess = false;
				m_iface.processErrorStringRequest(static_cast<IOErrorMessage>(m_pendingBuffer.at(1)));
				m_pendingBuffer.remove(0, 2);
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			default:
				// Got some command with CR, but not in sync or unrecognized. Take it out of buffer.
				if(-1 not_eq crIndex)
					m_pendingBuffer.remove(0, crIndex + 1);
				else // got something, might be in middle of something and with no CR, just get out.
					hasDataToProcess = false;
				break;
		}
		// if we want to continue processing, but have no data in buffer, get out anyway and wait for more data.
		if(hasDataToProcess)
			hasDataToProcess = !m_pendingBuffer.isEmpty();
	} // while(hasDataToProcess);
} // onDataAvailable


void MainWindow::processAddNewFacility(const QString& str)
{
//	Log("MAIN", QString("Got facility: %1").arg(str.mid(2)), success);
	m_clientFacilities[str.at(1)] = str.mid(2);
} // processAddNewFacility


void MainWindow::processDebug(const QString& str)
{
	LogLevelE level = info;
	switch(str[1].toUpper().toLatin1()) {
		case 'S':
			level = success;
			break;
		case 'I':
			level = success;
			break;
		case 'W':
			level = warning;
			break;
		case 'E':
			level = error;
			break;
	}

	Log(QString("R:") + m_clientFacilities.value(str[2], "GENERAL"), str.mid(3), level);
} // processDebug


void MainWindow::on_resetArduino_clicked()
{
	m_isConnected = false;
	m_iface.reset();
#ifdef HAS_WIRINGPI
	Log("MAIN", "Moving to disconnected state and resetting arduino...", warning);
	// pull pin 23 to reset arduino.
	pinMode(23, OUTPUT);
	digitalWrite(23, 0);
	delay(3000);
	Log("MAIN", "Releasing reset state...", info);
	// set it high again to release reset state.
	digitalWrite(23, 1);
#else
	m_port.close();
	m_port.open(QIODevice::ReadWrite);
#endif
} // on_resetArduino_clicked


////////////////////////////////////////////////////////////////////////////////
// Log related implementation
////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_clearLog_clicked()
{
	ui->log->clear();
	ui->saveHtml->setEnabled(false);
	ui->saveLog->setEnabled(false);
	ui->clearLog->setEnabled(false);
	ui->pauseLog->setEnabled(false);
} // on_m_clearLog_clicked


void MainWindow::on_saveLog_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log"), QString(), tr("Text Files (*.log *.txt)"));
	if(!fileName.isEmpty()) {
		QFile file(fileName);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << ui->log->toPlainText();
		file.close();
	}
} // on_saveLog_clicked


void MainWindow::on_saveHtml_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log as HTML"), QString(), tr("Html Files (*.html *.htm)"));
	if(!fileName.isEmpty()) {
		QFile file(fileName);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << ui->log->toHtml();
		file.close();
	}
} // on_saveHtml_clicked


void MainWindow::on_pauseLog_toggled(bool checked)
{
	ui->pauseLog->setText(checked ? tr("Resume") : tr("Pause"));
} // on_m_pauseLog_toggled


////////////////////////////////////////////////////////////////////////////////
// ILogTransport implementation.
////////////////////////////////////////////////////////////////////////////////
void MainWindow::appendTime(const QString& dateTime)
{
	if(ui->pauseLog->isChecked())
		return;
	QTextCursor cursor = ui->log->textCursor();
	cursor.movePosition(QTextCursor::End);
	ui->log->setTextCursor(cursor);
	ui->log->setTextColor(Qt::darkGray);
	ui->log->insertPlainText(dateTime);
} // appendTime


void MainWindow::appendLevelAndFacility(LogLevelE level, const QString& levelFacility)
{
	if(ui->pauseLog->isChecked())
		return;
	ui->log->setFontWeight(QFont::Bold);
	ui->log->setTextColor(logLevelColors[level]);
	ui->log->insertPlainText(" " + levelFacility + " ");
} // appendLevelAndFacility


void MainWindow::appendMessage(const QString& msg)
{
	if(ui->pauseLog->isChecked())
		return;
	ui->log->setFontWeight(QFont::Normal);
	ui->log->setTextColor(Qt::darkBlue);
	ui->log->insertPlainText(msg + "\n");

	if(ui->log->toPlainText().length() and !ui->saveHtml->isEnabled()) {
		ui->saveHtml->setEnabled(true);
		ui->saveLog->setEnabled(true);
		ui->clearLog->setEnabled(true);
		ui->pauseLog->setEnabled(true);
	}
} // appendMessage


void MainWindow::on_browseImageDir_clicked()
{
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Folder for your D64/T64/PRG/SID images"), ui->imageDir->text());
	if(!dirPath.isEmpty()) {
		ui->imageDir->setText(dirPath);
		m_iface.changeNativeFSDirectory(dirPath);
	}
	updateImageList();
} // on_browseImageDir_clicked()


void MainWindow::on_imageDir_editingFinished()
{
	if(!m_iface.changeNativeFSDirectory(ui->imageDir->text()))
		ui->imageDir->setText(QDir::currentPath());
	updateImageList();
} // on_imageDir_editingFinished


void MainWindow::updateImageList(bool reloadDirectory)
{
	if(!m_isInitialized)
		return;

	QStringList filterList = m_appSettings.imageFilters.split(',', QString::SkipEmptyParts);

	if(reloadDirectory) {
		m_infoList = QDir(ui->imageDir->text()).entryInfoList(filterList, QDir::NoDot bitor QDir::Files
			bitor (m_appSettings.showDirectories ? QDir::AllDirs : QDir::Files),
			QDir::DirsFirst bitor QDir::Name);
	}
	QRegExp filter(ui->imageFilter->text());
	filter.setCaseSensitivity(Qt::CaseInsensitive);

	m_filteredInfoList.clear();
	foreach(QFileInfo file, m_infoList) {
		QString strImage(file.fileName());
		if(strImage.contains(filter))
			m_filteredInfoList.append(file);
	}
	m_dirListItemModel->clear();
	bool hasImages = m_filteredInfoList.count() > 0;
	int numAdded = 0;

	if(hasImages) {
		// fill headers
		m_dirListItemModel->setColumnCount(IMAGE_LIST_HEADERS.count());
		for(int i = 0; i < IMAGE_LIST_HEADERS.count(); ++i)
			m_dirListItemModel->setHeaderData(i, Qt::Horizontal, IMAGE_LIST_HEADERS.at(i));

		foreach(const QFileInfo& fInfo, m_filteredInfoList) {
			QList<QStandardItem*> iList;
			QStandardItem* pItem = new QStandardItem(fInfo.fileName());
			if(fInfo.isDir())
				boldifyItem(pItem);
			iList << pItem;
			pItem = new QStandardItem(fInfo.isDir() ? "<DIR>" : QString::number(float(fInfo.size()) / 1024, 'f', 1));
			if(fInfo.isDir())
				boldifyItem(pItem);
			iList << pItem;

			foreach(QStandardItem* it, iList) {
				it->setEditable(false);
				it->setSelectable(true);
			}
			m_dirListItemModel->appendRow(iList);
			numAdded++;
		}
		for(int i = 0; i < m_dirListItemModel->columnCount(); ++i)
			ui->dirList->resizeColumnToContents(i);
	}
} // updateImageList


void MainWindow::boldifyItem(QStandardItem* pItem)
{
	QFont font = pItem->font();
	font.setWeight(QFont::Bold);
	pItem->setFont(font);
} // boldifyItem


void MainWindow::on_imageFilter_textChanged(const QString& filter)
{
	Q_UNUSED(filter);
	updateImageList(false);
} // on_imageFilter_textChanged


void MainWindow::on_reloadImageDir_clicked()
{
	updateImageList();
	Log("MAIN", "Program / Image directory reloaded.", info);
} // on_reloadImageDir_clicked


void MainWindow::on_mountSelected_clicked()
{
	QModelIndexList selected = ui->dirList->selectionModel()->selectedRows(0);
	QString name = selected.first().data(Qt::DisplayRole).toString();

	m_iface.processOpenCommand(QString("0|") + name, true);
} // on_mountSelected_clicked


void MainWindow::on_browseSingle_clicked()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Please choose image to mount."), ui->singleImageName->text(), tr("CBM Images (*.d64 *.t64 *.m2i);;All files (*)"));
	if(!file.isEmpty())
		ui->singleImageName->setText(file);
} // on_browseSingle_clicked


void MainWindow::on_mountSingle_clicked()
{
	m_iface.processOpenCommand(QString("0|") + ui->singleImageName->text(), true);
//	QFileInfo fileInfo(ui->singleImageName->text());
//	fileInfo.path();
//	fileInfo.baseName();
} // on_mountSingle_clicked


void MainWindow::on_unmountCurrent_clicked()
{
	m_iface.processOpenCommand(QString("0|") + QChar(CBM_BACK_ARROW) + QChar(CBM_BACK_ARROW), true);
} // on_unmountCurrent_clicked


void MainWindow::send(short lineNo, const QString& text)
{
	QString line(text);
	line.prepend(QString::number(lineNo) + ' ');
	// add it to the total dirlisting array.
	m_imageDirListing.append(line);
} // send

//////////////////////////////////////////////////////////////////////////////
// IMountNotifyListener interface implementation
//////////////////////////////////////////////////////////////////////////////
void MainWindow::directoryChanged(const QString& newPath)
{
	ui->imageDir->setText(newPath);
	updateImageList();
} // directoryChanged


void MainWindow::imageMounted(const QString& imagePath, FileDriverBase* pFileSystem)
{
	ui->nowMounted->setText(imagePath);
	ui->imageDirList->clear();
	m_imageDirListing.clear();
	if(pFileSystem->sendListing(*this)) {
		foreach(QString line, m_imageDirListing) {
			if(line.contains('\x12')) {
				line.replace('\x12', ' ');
				QListWidgetItem* a = new QListWidgetItem(line);
				ui->imageDirList->addItem(a);
				a->setTextColor(CCS64_BG_COLOR);
				a->setBackgroundColor(CCS64_FG_COLOR);
			}
			else
				ui->imageDirList->addItem(line);
		}
		m_imageDirListing.clear();
	}
	ui->unmountCurrent->setEnabled(true);

	int ix = 0;
	// select the image name in the directory file list!
	foreach(QFileInfo finfo, m_filteredInfoList) {
		if(!finfo.fileName().compare(imagePath, Qt::CaseInsensitive)) {
			ui->dirList->setFocus();
			ui->dirList->selectionModel()->setCurrentIndex(m_dirListItemModel->index(ix ,0), QItemSelectionModel::Rows bitor QItemSelectionModel::SelectCurrent);
			break;
		}
		++ix;
	}
} // imageMounted


void MainWindow::imageUnmounted()
{
	ui->imageDirList->clear();
	ui->nowMounted->setText("None, Local FS");
	ui->unmountCurrent->setEnabled(false);
} // imageUnmounted


void MainWindow::fileLoading(const QString& fileName, ushort fileSize)
{
	ui->progressInfoText->setText(QString("LOADING: %1").arg(fileName));
	ui->progressInfoText->setEnabled(true);
	ui->loadProgress->setEnabled(true);
	ui->loadProgress->setRange(0, fileSize);
} // fileLoading


void MainWindow::fileSaving(const QString& fileName)
{
	ui->progressInfoText->setText(QString("SAVING: %1").arg(fileName));
} // fileLoading


void MainWindow::bytesRead(uint numBytes)
{
	ui->loadProgress->setValue(numBytes);
} // bytesRead


void MainWindow::bytesWritten(uint numBytes)
{
	ui->loadProgress->setValue(numBytes);
} // bytesWritten


void MainWindow::fileClosed(const QString& lastFileName)
{
	Q_UNUSED(lastFileName);
	ui->progressInfoText->setText("READY.");
	ui->loadProgress->setEnabled(false);
	ui->loadProgress->setValue(0);
} // fileClosed
