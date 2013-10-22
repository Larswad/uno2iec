// TODO: Finalize M2I handling. What exactly is the point of that FS, is it to handle 8.3 filenames to cbm 16 byte lengths?
// TODO: Finalize x00fs handling (P00, S00, R00)
// TODO: Support x64 format.
// TODO: Support ZIP archives. Use "osdab" library for zip handling: https://code.google.com/p/osdab/downloads/detail?name=OSDaB-Zip-20130623.tar.bz2&can=2&q=
// TODO: Finalize Native FS routines.
// TODO: Finalize ALL doscommands (pretty huge job!)
// TODO: Handle all data channel stuff. TALK, UNTALK, and so on.
// TODO: Display the error channel status on the UI!
// TODO: T64 / D64 formats could/should read out entire disk into memory for caching (network performance).
// TODO: T64 / D64 write support!
// TODO: Finalize handling of write protected disk.
// TODO: If arduino is reset with a physical button on the board and it tries to resync, the PC-host application should automatically resync without having to press the 'Reset Arduino' button, meaning listen to connect even in connected mode.
// TODO: Rename openHostFile() and closeHostFile() to "mount"/"unmount" respectively on all file systems?
// TODO: When loading directories or media list, the dirlist view doesn't reflect this status.
// TODO: When a file/directory is attempted for loading or saving and this fails for some reason this isn't reflected on the dirlist view.
// TODO: When executing the command channel this isn't reflected on the dirlist view.
// TODO: Native/D64/T64: Show true blocks free. Will be nice with all the space available on a harddisk or network share!

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

// These definitions are in a header, just not to clutter down this file with too many constant definitions.
#include "dirlistthemingconsts.hpp"

EmulatorPaletteMap emulatorPalettes;
CbmMachineThemeMap machineThemes;


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

// Default Device and Arduino PIN configuration.
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
	QMainWindow(parent)
, ui(new Ui::MainWindow)
, m_port(QextSerialPort::EventDriven, this)
, m_isConnected(false)
, m_iface(m_port)
, m_isInitialized(false)
{
	ui->setupUi(this);

	m_dirListItemModel = new QStandardItemModel(0, IMAGE_LIST_HEADERS.count(), this);
	Q_ASSERT(m_dirListItemModel);
	ui->dirList->setModel(m_dirListItemModel);
	loggerInstance().addTransport(this);

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
	Log("MAIN", success, QString("Application Started, using port %1 @ %2").arg(m_port.portName()).arg(QString::number(m_port.baudRate())));
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
	emulatorPalettes["vice"] = viceColors;
	emulatorPalettes["ccs64"] = ccs64Colors;
	emulatorPalettes["frodo"] = frodoColors;
	emulatorPalettes["c64s"] = c64sColors;
	emulatorPalettes["c64hq"] = c64hqColors;
	emulatorPalettes["godot"] = godotColors;
	emulatorPalettes["pc64"] = pc64Colors;
	emulatorPalettes["default"] = defaultColors;
	emulatorPalettes["vic20"] = vic20Colors;
	emulatorPalettes["plus4"] = plus4Colors;

	machineThemes["VIC 20"] = &Vic20Theme;
	machineThemes["C 64"] = &C64Theme;
	machineThemes["C 128"] = &C128Theme;
	machineThemes["C 128 (80 Col)"] = &C128_80Theme;
	machineThemes["Plus 4"] = &Plus4Theme;

	setupActionGroups();

	Log("MAIN", success, "Application Initialized.");
	m_isInitialized = true;
	updateImageList();

	// register ourselves to listen for all CBM events from the Arduino so that we can reflect this on UI controls.
	m_iface.setMountNotifyListener(this);
	m_iface.setImageFilters(m_appSettings.imageFilters, m_appSettings.showDirectories);
	// This will also reset the device!
	updateDirListColors();
} // ctor


void MainWindow::setupActionGroups()
{
	QActionGroup* dirListColorGroup = new QActionGroup(this);
	dirListColorGroup->addAction(ui->actionC64hq);
	dirListColorGroup->addAction(ui->actionCcs64);
	dirListColorGroup->addAction(ui->actionDefault);
	dirListColorGroup->addAction(ui->actionC64s);
	dirListColorGroup->addAction(ui->actionPc64);
	dirListColorGroup->addAction(ui->actionVice);
	dirListColorGroup->addAction(ui->actionInternal);
	dirListColorGroup->addAction(ui->actionFrodo);
	dirListColorGroup->addAction(ui->actionVic20);
	dirListColorGroup->addAction(ui->actionPlus4);

	connect(dirListColorGroup, SIGNAL(triggered(QAction*)), this, SLOT(onDirListColorSelected(QAction*)));
	selectActionByName(ui->menuEmulator_Palette->actions(), m_appSettings.emulatorPalette);

	QActionGroup* cbmMachineGroup = new QActionGroup(this);
	cbmMachineGroup->addAction(ui->actionCommodore_64);
	cbmMachineGroup->addAction(ui->actionC128);
	cbmMachineGroup->addAction(ui->actionVic_20);
	cbmMachineGroup->addAction(ui->actionC_128_80_Col);
	cbmMachineGroup->addAction(ui->actionPlus_4);
	connect(cbmMachineGroup, SIGNAL(triggered(QAction*)), this, SLOT(onCbmMachineSelected(QAction*)));
	selectActionByName(ui->menuMachine->actions(), m_appSettings.cbmMachine);
} // setupActionGroup


void MainWindow::selectActionByName(const QList<QAction*>& actions, const QString& name) const
{
	foreach(QAction* action, actions) {
		if(not action->text().compare(name, Qt::CaseInsensitive)) {
			action->setChecked(true);
			return;
		}
	}
} // selectActionByName


void MainWindow::onDirListColorSelected(QAction* pAction)
{
	m_appSettings.emulatorPalette = pAction->text();
	updateDirListColors();
} // dirListColorSelected


void MainWindow::onCbmMachineSelected(QAction* pAction)
{
	m_appSettings.cbmMachine = pAction->text();
	updateDirListColors();
} // cbmMachineSelected


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
#elif defined(Q_OS_MAC)
	QDir dev("/dev","tty.usbmodem*", QDir::Name,QDir::Files bitor QDir::Readable bitor QDir::Writable bitor QDir::System);
	foreach(const QFileInfo entry, dev.entryInfoList()) {
		static QextPortInfo unixPort = { entry.absoluteFilePath(), entry.fileName(), entry.fileName(), "", 0, 0 };
		m_ports.insert(0, unixPort);
	}
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
			Log("MAIN", info, QString("Port name changed to %1").arg(m_port.portName()));
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

	m_appSettings.emulatorPalette = settings.value("emulatorPalette", "ccs64").toString();
	m_appSettings.cbmMachine = settings.value("cbmMachine", "C 64").toString();

	ui->actionDisk_Write_Protected->setChecked(settings.value("diskWriteProtected", false).toBool());

	restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
	restoreState(settings.value("mainWindowState").toByteArray());
	QList<int> splits;
	splits.append(settings.value("splitterpos1_size", 0).toInt());
	splits.append(settings.value("splitterpos2_size", 0).toInt());
	if(splits.at(0) and splits.at(1))
		ui->splitter->setSizes(splits);
} // readSettings


void MainWindow::writeSettings() const
{
	QSettings settings;
	settings.setValue("mainWindowGeometry", saveGeometry());
	settings.setValue("mainWindowState", saveState());
	QList<int> splits = ui->splitter->sizes();
	if(splits.size() >= 2) {
		settings.setValue("splitterpos1_size", splits.at(0));
		settings.setValue("splitterpos2_size", splits.at(1));
	}
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

	settings.setValue("emulatorPalette", m_appSettings.emulatorPalette);
	settings.setValue("cbmMachine", m_appSettings.cbmMachine);

	settings.setValue("diskWriteProtected", ui->actionDisk_Write_Protected->isChecked());
} // writeSettings


// Debugging helper for logging raw recieved serial bytes as HEX strings to file.
void LogHexData(const QByteArray& bytes, const QString& header = QString("R#%1:"))
{
	QFile fh("received.txt");

	if(not fh.open(QFile::Append bitor QFile::WriteOnly))
		return;
	QTextStream out(&fh);
	out << header.arg(bytes.length());
	QString ascii;
	foreach(uchar byte, bytes)
	{
		out << QString("%1 ").arg((ushort)byte, 2, 16, QChar('0'));
		ascii.append(QChar(byte).isPrint() ? byte : '.');
	}
	out <<  ascii << '\n';
	fh.close();
} // LogHexData


bool MainWindow::checkConnectRequest()
{
	if(!m_pendingBuffer.contains("connect"))
		return false;
	m_pendingBuffer.clear();
	// Assume connected, maybe a real ack sequence is needed here from the client?
	// Are we already connected? If so,
	if(!m_isConnected) {
		m_isConnected = true;
		Log("MAIN", success, "Now connected to Arduino.");
	}
	else
		Log("MAIN", warning, "Got reconnection attempt from Arduino for unknown reason. Accepting new connection.");

	// give the client current date and time in the response string.
	m_port.write((OkString + QString::number(m_appSettings.deviceNumber) + '|' + QString::number(m_appSettings.atnPin) + '|' + QString::number(m_appSettings.clockPin)
								+ '|' + QString::number(m_appSettings.dataPin) + '|' + QString::number(m_appSettings.resetPin) /*+ '|' + QString::number(m_appSettings.srqInPin)*/
								+ '\r').toLatin1().data());
	// client is supposed to send it's facilities each start.
	m_clientFacilities.clear();
	return true;
} // checkConnectRequest


////////////////////////////////////////////////////////////////////////////
// Dispatcher for when something has arrived on the serial port.
////////////////////////////////////////////////////////////////////////////
void MainWindow::onDataAvailable()
{
	m_pendingBuffer.append(m_port.readAll());
	if(not m_isConnected) {
		checkConnectRequest();
		return;
	}

	bool hasDataToProcess = !m_pendingBuffer.isEmpty();
	//	if(hasDataToProcess)
	//		LogHexData(m_pendingBuffer);
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

		case 'R': // read byte(s) from current file system mode, note that this command needs no termination, it needs to be short.
			m_pendingBuffer.remove(0, 1);
			m_iface.processReadFileRequest();
			hasDataToProcess = !m_pendingBuffer.isEmpty();
			break;

		case 'W': // write single character to file in current file system mode.
			hasDataToProcess = false; // assume we may need to wait for additional data.
			if(m_pendingBuffer.size() > 1) {
				uchar length = (uchar)m_pendingBuffer.at(1);
				if(m_pendingBuffer.size() >= length) {
					m_iface.processWriteFileRequest(m_pendingBuffer.mid(2, length - 2));
					// discard all processed (written) bytes from buffer.
					m_pendingBuffer.remove(0, length);
					hasDataToProcess = !m_pendingBuffer.isEmpty();
				}
			}
			break;

		case 'L': // directory/media info Line request:
			// Just remove the BYTE from queue and do business.
			m_pendingBuffer.remove(0, 1);
			m_iface.processLineRequest();
			break;

		case 'C': // close FILE command
			m_pendingBuffer.remove(0, 1);
			m_iface.processCloseCommand();
			break;

		case 'E': // Ask for translation of error string from error code
			if(cmdString.size() < 1)
				hasDataToProcess = false;
			m_iface.processErrorStringRequest(static_cast<CBM::IOErrorMessage>(m_pendingBuffer.at(1)));
			m_pendingBuffer.remove(0, 2);
			hasDataToProcess = !m_pendingBuffer.isEmpty();
			break;

		default:
			// See if it is a reconnection attempt.
			if(!checkConnectRequest()) {
				// Got some command with CR, but not in sync or unrecognized. Take it out of buffer.
				if(-1 not_eq crIndex) {
						m_pendingBuffer.remove(0, crIndex + 1);
				}
				else // got something, might be in middle of something and with no CR, just get out.
					m_pendingBuffer.remove(0, 1);
				//				Log("MAIN", warning, QString("Got unknown char %1").arg(cmdString.at(0).toLatin1()));
			}
			break;
		}
		// if we want to continue processing, but have no data in buffer, get out anyway and wait for more data.
		if(hasDataToProcess)
			hasDataToProcess = not m_pendingBuffer.isEmpty();
	} // while(hasDataToProcess);

	//	if(not m_pendingBuffer.isEmpty())
	//		LogHexData(m_pendingBuffer, "U:#%1");
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
						level = info;
		break;
	case 'W':
		level = warning;
		break;
	case 'E':
		level = error;
		break;
	}

	Log(QString("R:") + m_clientFacilities.value(str[2], "GENERAL"), level, str.mid(3));
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
	if(not fileName.isEmpty()) {
		QFile file(fileName);
		if(not file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << ui->log->toPlainText();
		file.close();
	}
} // on_saveLog_clicked


void MainWindow::on_saveHtml_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log as HTML"), QString(), tr("Html Files (*.html *.htm)"));
	if(not fileName.isEmpty()) {
		QFile file(fileName);
		if(not file.open(QIODevice::WriteOnly | QIODevice::Text))
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
	if(not dirPath.isEmpty()) {
		ui->imageDir->setText(dirPath);
		m_iface.changeNativeFSDirectory(dirPath);
	}
	updateImageList();
} // on_browseImageDir_clicked()


void MainWindow::on_imageDir_editingFinished()
{
	if(not m_iface.changeNativeFSDirectory(ui->imageDir->text()))
		ui->imageDir->setText(QDir::currentPath());
	updateImageList();
} // on_imageDir_editingFinished


void MainWindow::updateImageList(bool reloadDirectory)
{
	if(not m_isInitialized)
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
	Log("MAIN", info, "Program / Image directory reloaded.");
} // on_reloadImageDir_clicked


void MainWindow::on_dirList_doubleClicked(const QModelIndex &index)
{
	Q_UNUSED(index);
	ui->mountSelected->animateClick(250);
	//	on_mountSelected_clicked();
} // on_dirList_doubleClicked


void MainWindow::on_mountSelected_clicked()
{
	QModelIndexList selected = ui->dirList->selectionModel()->selectedRows(0);
	if(!selected.size())
		return;
	QString name = selected.first().data(Qt::DisplayRole).toString();

	m_iface.processOpenCommand(QString("0|") + name, true);
} // on_mountSelected_clicked


void MainWindow::on_browseSingle_clicked()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Please choose image to mount."), ui->singleImageName->text(), tr("CBM Images (*.d64 *.t64 *.m2i);;All files (*)"));
	if(not file.isEmpty())
		ui->singleImageName->setText(file);
} // on_browseSingle_clicked


void MainWindow::on_mountSingle_clicked()
{
	m_iface.processOpenCommand(QString("0|") + ui->singleImageName->text(), true);
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
	QColor bgColor, frColor, fgColor;
	getBgFrAndFgColors(bgColor, frColor, fgColor);

	ui->nowMounted->setText(imagePath);
	ui->imageDirList->clear();
	m_imageDirListing.clear();
	if(pFileSystem->supportsListing() and pFileSystem->sendListing(*this)) {
		foreach(QString line, m_imageDirListing) {
			QStringList lineInverses = line.split('\x12', QString::SkipEmptyParts);
			bool rvs = false;
			foreach(QString linePart, lineInverses) {
				if(rvs) {
					ui->imageDirList->setTextColor(bgColor);
					ui->imageDirList->setTextBackgroundColor(fgColor);
				}
				else {
					ui->imageDirList->setTextColor(fgColor);
					ui->imageDirList->setTextBackgroundColor(bgColor);
				}
				ui->imageDirList->insertPlainText(linePart);
				rvs ^= true;
			}
			ui->imageDirList->insertPlainText("\n");
		}
		m_imageDirListing.clear();
	}
	ui->unmountCurrent->setEnabled(true);
	//	ui->imageDirList->setFocus();

	int ix = 0;
	// select the image name in the directory file list!
	foreach(QFileInfo finfo, m_filteredInfoList) {
		if(not finfo.fileName().compare(imagePath, Qt::CaseInsensitive)) {
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
	m_loadSaveName = fileName;
	ui->progressInfoText->clear();
	ui->progressInfoText->setEnabled(true);
	ui->loadProgress->setEnabled(true);
	ui->loadProgress->setRange(0, fileSize);
	ui->loadProgress->show();
	m_totalReadWritten = 0;
	ui->loadProgress->setValue(m_totalReadWritten);
	QTextCursor cursor = ui->imageDirList->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.deleteChar();
	cursor.insertText(QString("LOAD\"%1\",%2\nSEARCHING FOR %1\nLOADING\n").arg(fileName, QString::number(m_appSettings.deviceNumber)));
	ui->imageDirList->setTextCursor(cursor);
	cbmCursorVisible(false);
} // fileLoading


void MainWindow::fileSaving(const QString& fileName)
{
	m_loadSaveName = fileName;
	ui->loadProgress->hide();
	ui->progressInfoText->clear();
	ui->progressInfoText->setEnabled(true);
	QTextCursor cursor = ui->imageDirList->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.deleteChar();
	cursor.insertText(QString("SAVE\"%1\",%2\n\nSAVING %1\n").arg(fileName, QString::number(m_appSettings.deviceNumber)));
	m_totalReadWritten = 0;
	cbmCursorVisible(false);
} // fileLoading


void MainWindow::bytesRead(uint numBytes)
{
	m_totalReadWritten += numBytes;
	ui->loadProgress->setValue(m_totalReadWritten);
	ui->progressInfoText->setText(QString("LOADING: %1 (%2 bytes)").arg(m_loadSaveName).arg(m_totalReadWritten));
} // bytesRead


void MainWindow::bytesWritten(uint numBytes)
{
	m_totalReadWritten += numBytes;
	ui->progressInfoText->setText(QString("SAVING: %1 (%2 bytes)").arg(m_loadSaveName).arg(m_totalReadWritten));
} // bytesWritten


void MainWindow::fileClosed(const QString& lastFileName)
{
	Q_UNUSED(lastFileName);
	ui->progressInfoText->setText(QString("READY. (%1 %2 bytes)").arg(m_loadSaveName).arg(m_totalReadWritten));
	ui->loadProgress->setEnabled(false);
	ui->loadProgress->setValue(0);
	ui->loadProgress->show();
	QTextCursor cursor = ui->imageDirList->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.deleteChar();
	cursor.insertText("READY.\n");
	ui->imageDirList->setTextCursor(cursor);
	cbmCursorVisible();
} // fileClosed


bool MainWindow::isWriteProtected()
{
	return ui->actionDisk_Write_Protected->isChecked();
} // isWriteProtected


void MainWindow::deviceReset()
{
	QColor bgColor, frColor, fgColor;
	getBgFrAndFgColors(bgColor, frColor, fgColor);
	ui->imageDirList->setTextColor(fgColor);
	ui->imageDirList->setTextBackgroundColor(bgColor);
	CbmMachineTheme* pMachine;
	const QRgb* pEmulatorPalette;
	getMachineAndPaletteTheme(pMachine, pEmulatorPalette);
	ui->imageDirList->setText(pMachine ? pMachine->bootText : C64Theme.bootText);
	QTextCursor cursor = ui->imageDirList->textCursor();
	cursor.movePosition(QTextCursor::End);
	ui->imageDirList->setTextCursor(cursor);
	ui->imageDirList->setFocus();
	ui->imageDirList->setCursorWidth(pMachine ? pMachine->cursorWidth : DEFAULT_CBM_BLOCK_CURSOR_WIDTH);
} // fileClosed


//////////////////////////////////////////////////////////////////////////////
// Handling of directory list theming.
//////////////////////////////////////////////////////////////////////////////
void MainWindow::getBgFrAndFgColors(QColor& bgColor, QColor& frColor, QColor& fgColor)
{
	bgColor = DEFAULT_CCS64_BG_COLOR;
	fgColor = DEFAULT_CCS64_FG_COLOR;
	frColor = DEFAULT_CCS64_FR_COLOR;

	CbmMachineTheme* pMachine;
	const QRgb* pEmulatorPalette;
	getMachineAndPaletteTheme(pMachine, pEmulatorPalette);

	if(0 not_eq pMachine and 0 not_eq pEmulatorPalette) {
		fgColor = QColor(pEmulatorPalette[pMachine->fgColorIndex]);
		frColor = QColor(pEmulatorPalette[pMachine->frColorIndex]);
		bgColor = QColor(pEmulatorPalette[pMachine->bgColorIndex]);
	}
} // getBgAndFgColors


void MainWindow::cbmCursorVisible(bool visible)
{
	int width = 0;
	if(visible) {
		CbmMachineTheme* pMachine;
		const QRgb* pEmulatorPalette;
		getMachineAndPaletteTheme(pMachine, pEmulatorPalette);
		width = pMachine ? pMachine->cursorWidth : DEFAULT_CBM_BLOCK_CURSOR_WIDTH;
	}
	ui->imageDirList->setCursorWidth(width);
} // cbmCursorVisible


void MainWindow::getMachineAndPaletteTheme(CbmMachineTheme*& pMachine, const QRgb*& pEmulatorPalette)
{
	pMachine = 0;
	pEmulatorPalette = 0;
	EmulatorPaletteMap::iterator itEmulatorPalette = emulatorPalettes.find(m_appSettings.emulatorPalette);
	CbmMachineThemeMap::iterator itMachineTheme = machineThemes.find(m_appSettings.cbmMachine);
	if(itEmulatorPalette not_eq emulatorPalettes.end())
		pEmulatorPalette = *itEmulatorPalette;
	if(itMachineTheme not_eq machineThemes.end())
		pMachine = *itMachineTheme;
} // getMachineAndPaletteTheme


void MainWindow::updateDirListColors()
{
	CbmMachineTheme* pMachine;
	const QRgb* pEmulatorPalette;
	getMachineAndPaletteTheme(pMachine, pEmulatorPalette);

	QColor bgColor, frColor, fgColor;
	getBgFrAndFgColors(bgColor, frColor, fgColor);

	QString sheet = QString("background-color: rgb(%1);\ncolor: rgb(%2);\n"
													"border: 60px solid %3;\npadding: -4px;\n"
													"font: %4;\n").arg(
				QString::number(bgColor.red()) + "," + QString::number(bgColor.green()) + "," + QString::number(bgColor.blue())).arg(
				QString::number(fgColor.red()) + "," + QString::number(fgColor.green()) + "," + QString::number(fgColor.blue())).arg(
				frColor.name()).arg(
				pMachine->font);
	ui->imageDirList->setStyleSheet(sheet);
	ui->imageDirList->setCursorWidth(pMachine->cursorWidth);
	deviceReset();
} // updateDirListColors
