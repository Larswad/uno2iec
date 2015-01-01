// TODO: The send line request/answer 'L' can be factored out into a regular read 'R' request/answer. All special things for preparation
// like basic address and all lines can be put into one single QByteArray that is transferred in chunks for each request. There is no
// meaning in making this a special command.
// TODO: The error channel can be factored into a regular read-return of a buffer. The buffer is always by the result of the last command
// executed, which might either be resulting error message or some result from a dos command, like M-R and so on.

// TODO: Case handling in Linux, how to support this? Just returning all file entries as they are will not look to good, so best is
// to keep toUpper() for this. But all CD operations and fopen operations need to be able to open file in case insensitive mode somehow.
// TODO: Finalize M2I handling. What exactly is the point of that FS, is it to handle 8.3 filenames to cbm 16 byte lengths?
// TODO: Finalize x00fs handling (P00, S00, R00). The x00fs format can actually handle multiple files as a sort of container to replace M2I. This should be supported.
// TODO: Support x64 format, it should in fact be the preferred format today before D64, D71, D81.
// TODO: Support of images in ZIP archives. Use "osdab" library for zip handling:
//			https://code.google.com/p/osdab/downloads/detail?name=OSDaB-Zip-20130623.tar.bz2&can=2&q=
// TODO: Finalize Native FS routines.
// TODO: Finalize ALL doscommands (pretty huge job!)
// TODO: Handle all data channel stuff. TALK, UNTALK, and so on.
// TODO: Display the current error channel status on the UI!
// TODO: T64 / D64 formats could/should read out entire disk into memory for caching (network performance).
// TODO: T64 / D64 write support!
// TODO: Finalize handling of write protected disk.
// TODO: If arduino is reset with a physical button on the board and it tries to resync, the PC-host application should automatically resync without having to press the 'Reset Arduino' button, meaning: listen to unexpecte "connect-request" even in connected mode.
// TODO: When loading directories or media list, the dirlist view doesn't reflect this status.
// TODO: When a file/directory is attempted for loading or saving and this fails for some reason this isn't reflected on the dirlist view.
// TODO: When executing the command channel this isn't reflected on the dirlist view.
// TODO: Native/D64/T64: Show TRUE (actual) blocks free. Will be nice with all the space available on a harddisk or network share!
// TODO: Add icons for disk images in image list view (T64, D64, x64, .PRG, normal folders). This make it much easier to distinguish cbm related files from each other.

// DONE: Rename openHostFile() and closeHostFile() to "mount"/"unmount" respectively on all file systems?
// DONE: Needs to be verified/tested: Need to refactor the open command 'O' so that it is not passed as a CR terminated string. This will never
// work when issued for the CBMDOS command buffer where there can be zeroes and any other character in the buffer that could be
// treated as a termination character. Therefore the 'O' command has to be transferred with a length character in second position.

#include <QString>
#include <QFileDialog>
#include <QTextStream>
#include <QDate>
#include <QDebug>
#include <QSettings>
#include <QTimer>
#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "aboutdialog.hpp"
#include "mountspecificfile.h"
#include "version.h"

using namespace Logging;

namespace {

// These definitions are in a header, just not to clutter down this file with too many constant definitions.
#include "dirlistthemingconsts.hpp"

EmulatorPaletteMap emulatorPalettes;
CbmMachineThemeMap machineThemes;

const QString OkString = "OK>%1|%2|%3|%4|%5|%6|%7.%8\r";
const QString NOkString = "NOK>\r";
const QString ConnectionString = "connect_arduino:";
const QColor logLevelColors[] = { QColor(Qt::red), QColor("orange"), QColor(Qt::blue), QColor(Qt::darkGreen) };

QStringList IMAGE_LIST_HEADERS = (QStringList()
																	<< QObject::tr("Name")
																	<< QObject::tr("Size (KiB)"));

QStringList LOG_LEVELS = (QStringList()
													<< QObject::tr("error  ")
													<< QObject::tr("warning")
													<< QObject::tr("info   ")
													<< QObject::tr("success"));


// Default Device and Arduino PIN configuration.
const uint DEFAULT_BAUDRATE = BAUD115200;
const uint DEFAULT_DEVICE_NUMBER = 8;
const uint DEFAULT_RESET_PIN = 7;
const uint DEFAULT_CLOCK_PIN = 4;
const uint DEFAULT_DATA_PIN = 3;
const uint DEFAULT_ATN_PIN = 5;
const uint DEFAULT_SRQIN_PIN = 2;


const QString PROGRAM_VERSION_HISTORY = qApp->tr(
		"<hr>"
		"<ul><li>NEW: Saving support in 1541 normal speed."
		"<li>CHG: Changed much on the UI. Nice icons for all buttons. Separate configuration dialog from main window. "
		"Now using Qt Theming, choosing best fit depending on OS. Event log is now dockable and can be hidden or detached. "
		"Progress bar now for loading files. The directory image listing is now displayed as a real CBM, can be set to look "
		"either like a c64, c128 (40/80 modes), a Plus/4 or a VIC20. Refresh button for file list. Directories in bold font."
		"<li>FIX: The sprintf formatting for initial handshake string didn't work for some Arduino Due. Thanks to Lemon64 users for seeing this."
		"<li>NEW: Some work has been done on error channel and started work on command channel. In progress."
		"<li>NEW: Much work started on M2I support. Now finished, in progress."
		"<li>NEW: Settings for file filters and whether directories should be visible or not."
		"<li>NEW: Supporting configuration of arduino bluetooth device making the whole project go wireless ! See Readme for info."
		"<li>FIX: A LOT of memory optimization on the Arduino side. Reduced ram use to fraction of earlier version. Using PGM memory!"
		"<li>NEW: Support for RESET line. Conditional compiling define whether this line is soldered or not."
		"<li>FIX: Build location of intermediate files on PC side now much better in .PRO file."
		"<li>FIX: Serial optimization for file reading from CBM by keeping interrupts enabled and issuing read before transferring to CBM."
		" This is not enabled by default just to be sure, but it can be enabled in global_defines.h header."
		"<li>FIX: Size calculation of file and proper transfer of file size to arduino so that progress can be visualized on both sides."
		"</ul>"
		"<span style=\" font-weight:600; color:#0404c2;\">Application Version 0.3.0:</span><hr>"
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


MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m_port(QextSerialPort::EventDriven, this)
	, m_isConnected(false)
	, m_iface()
	, m_isInitialized(false)
	,	m_fsWatcher(this)
	, m_simulatedState(simsOff)
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
	connect(ui->imageDirList, SIGNAL(commandIssued(const QString&)), this, SLOT(onCommandIssued(const QString&)));
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

	m_isInitialized = true;
	directoryChanged(m_appSettings.imageDirectory);

	// register ourselves to listen for all CBM events from the Arduino so that we can reflect this on UI controls.
	m_iface.setMountNotifyListener(this);
	m_iface.setImageFilters(m_appSettings.imageFilters, m_appSettings.showDirectories);
	// This will also reset the device!
	updateDirListColors();
	// We want notifications when the local file system changes so that we can update the image directory list.
	connect(&m_fsWatcher, SIGNAL(directoryChanged(const QString&)), this, SLOT(on_directoryChanged(const QString&)));
	watchDirectory(m_appSettings.imageDirectory);
	Log("MAIN", success, "Application Initialized.");
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
		if(m_appSettings.cbmBorderWidth not_eq oldSettings.cbmBorderWidth)
			updateDirListColors();
		if(m_appSettings.imageFilters not_eq oldSettings.imageFilters
			 or m_appSettings.showDirectories not_eq oldSettings.showDirectories
			 or m_appSettings.imageDirectory not_eq oldSettings.imageDirectory) {
			m_iface.setImageFilters(m_appSettings.imageFilters, m_appSettings.showDirectories);
			m_iface.changeNativeFSDirectory(m_appSettings.imageDirectory);
			watchDirectory(m_appSettings.imageDirectory);
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


void MainWindow::on_actionSingle_file_mount_triggered()
{
	MountSpecificFile mountDialog(m_appSettings.lastSpecificMounted, this);
	if(QDialog::Accepted == mountDialog.exec()) {
		m_appSettings.lastSpecificMounted = mountDialog.chosenFile();
		m_iface.processOpenCommand(CBM::READPRG_CHANNEL, m_appSettings.lastSpecificMounted.toLocal8Bit(), true);
	}
} // on_actionSingle_file_mount_triggered


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
	QSettings sets;

	m_appSettings.imageDirectory = sets.value("imageDirectory", QDir::currentPath()).toString();
	m_appSettings.lastSpecificMounted = sets.value("singleImageName").toString();
	QDir::setCurrent(m_appSettings.imageDirectory);
	ui->imageFilter->setText(sets.value("imageFilter", QString()).toString());
	m_appSettings.portName = sets.value("portName", m_ports.isEmpty() ? "COM1" : m_ports.first().friendName).toString();
	m_appSettings.baudRate = sets.value("baudRate", QString::number(DEFAULT_BAUDRATE)).toUInt();
	m_appSettings.deviceNumber = sets.value("deviceNumber", QString::number(DEFAULT_DEVICE_NUMBER)).toUInt();
	m_appSettings.atnPin = sets.value("atnPin", QString::number(DEFAULT_ATN_PIN)).toUInt();
	m_appSettings.clockPin = sets.value("clockPin", QString::number(DEFAULT_CLOCK_PIN)).toUInt();
	m_appSettings.dataPin = sets.value("dataPin", QString::number(DEFAULT_DATA_PIN)).toUInt();
	m_appSettings.resetPin = sets.value("resetPin", QString::number(DEFAULT_RESET_PIN)).toUInt();
	m_appSettings.srqInPin = sets.value("srqInPin", QString::number(DEFAULT_SRQIN_PIN)).toUInt();

	m_appSettings.imageFilters = sets.value("imageFilters", "*.D64,*.T64,*.M2I,*.PRG,*.P00,*.SID").toString();
	m_appSettings.showDirectories = sets.value("showDirectories", false).toBool();
	m_appSettings.programVersion = sets.value("lastProgramVersion", "unset").toString();

	m_appSettings.emulatorPalette = sets.value("emulatorPalette", "ccs64").toString();
	m_appSettings.cbmMachine = sets.value("cbmMachine", "C 64").toString();
	m_appSettings.cbmBorderWidth = sets.value("cbmBorderWidth", 60).toUInt();
	ui->actionDisk_Write_Protected->setChecked(sets.value("diskWriteProtected", false).toBool());

	restoreGeometry(sets.value("mainWindowGeometry").toByteArray());
	restoreState(sets.value("mainWindowState").toByteArray());
	QList<int> splits;
	splits.append(sets.value("splitterpos1_size", 0).toInt());
	splits.append(sets.value("splitterpos2_size", 0).toInt());
	if(splits.at(0) and splits.at(1))
		ui->splitter->setSizes(splits);
	Logging::loggerInstance().loadFilters(sets);
} // readSettings


void MainWindow::writeSettings() const
{
	QSettings sets;
	sets.setValue("mainWindowGeometry", saveGeometry());
	sets.setValue("mainWindowState", saveState());
	QList<int> splits = ui->splitter->sizes();
	if(splits.size() >= 2) {
		sets.setValue("splitterpos1_size", splits.at(0));
		sets.setValue("splitterpos2_size", splits.at(1));
	}
	sets.setValue("lastProgramVersion", m_appSettings.programVersion);

	sets.setValue("imageDirectory", m_appSettings.imageDirectory);
	sets.setValue("singleImageName", m_appSettings.lastSpecificMounted);
	sets.setValue("imageFilter", ui->imageFilter->text());
	sets.setValue("portName", m_appSettings.portName);
	sets.setValue("baudRate", QString::number(m_appSettings.baudRate));
	sets.setValue("deviceNumber", QString::number(m_appSettings.deviceNumber));
	sets.setValue("atnPin", QString::number(m_appSettings.atnPin));
	sets.setValue("clockPin", QString::number(m_appSettings.clockPin));
	sets.setValue("dataPin", QString::number(m_appSettings.dataPin));
	sets.setValue("resetPin", QString::number(m_appSettings.resetPin));
	sets.setValue("srqInPin", QString::number(m_appSettings.srqInPin));
	sets.setValue("imageFilters", m_appSettings.imageFilters);
	sets.setValue("showDirectories", m_appSettings.showDirectories);

	sets.setValue("emulatorPalette", m_appSettings.emulatorPalette);
	sets.setValue("cbmMachine", m_appSettings.cbmMachine);
	sets.setValue("cbmBorderWidth", m_appSettings.cbmBorderWidth);

	sets.setValue("diskWriteProtected", ui->actionDisk_Write_Protected->isChecked());
	Logging::loggerInstance().saveFilters(sets);
} // writeSettings


// Debugging helper for logging raw recieved serial bytes as HEX strings to file.
void LogHexData(const QByteArray& bytes, const QString& header = QString("R#%1:"))
{
	if(!bytes.length())
		return;
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
	int connectPos = m_pendingBuffer.indexOf(ConnectionString);
	if(-1 == connectPos)
		return false;
	int crPos = m_pendingBuffer.indexOf('\r', connectPos);
	if(-1 == connectPos)
		return false;

	// extract version number.
	const QString verString(m_pendingBuffer.mid(connectPos + ConnectionString.length(), crPos - connectPos));
	ushort receivedProtoVersion = verString.toInt();
	if(CURRENT_UNO2IEC_PROTOCOL_VERSION not_eq receivedProtoVersion) {
		Log("MAIN", error, QString("Received connection string from arduino, but the protocol version (%1) mismatched our "
				"version (%2). Not accepting connection, please upgrade the Arduino!")
				.arg(receivedProtoVersion).arg(CURRENT_UNO2IEC_PROTOCOL_VERSION));
		m_pendingBuffer.clear();
		// Negative response, make it stop connection attempts.
		m_port.write(NOkString.toLatin1().data());
		return false;
	}

	m_pendingBuffer.clear();
	// Assume connected, maybe a real ack sequence is needed here from the client?
	// Are we already connected? If so,
	if(not m_isConnected) {
		m_isConnected = true;
		Log("MAIN", success, "Now connected to Arduino.");
	}
	else
		Log("MAIN", warning, "Got reconnection attempt from Arduino for unknown reason. Accepting new connection.");

	// give the client the version, pin configuration, current date and time in the response string.
	const QString response = OkString.arg(QString::number(m_appSettings.deviceNumber))
			.arg(QString::number(m_appSettings.atnPin))
			.arg(QString::number(m_appSettings.clockPin))
			.arg(QString::number(m_appSettings.dataPin))
			.arg(QString::number(m_appSettings.resetPin))
			.arg(QString::number(m_appSettings.srqInPin))
			.arg(QDate::currentDate().toString("yyyy-MM-dd"))
			.arg(QTime::currentTime().toString("hh:mm:ss"));

	m_port.write(response.toLatin1().data());
	// client is supposed to send it's facilities each start.
	m_clientFacilities.clear();
	return true;
} // checkConnectRequest


////////////////////////////////////////////////////////////////////////////
// Dispatcher for when something has arrived on the serial port / simulated data.
////////////////////////////////////////////////////////////////////////////
void MainWindow::onDataAvailable()
{
	m_pendingBuffer.append(m_port.readAll());
//	if(not m_isConnected) {
		checkConnectRequest();
//		return;
//	}
	if(m_isConnected)
		processData();
} // onDataAvailable


#ifdef QT_DEBUG
void MainWindow::simulateData(const QByteArray& data)
{
	m_pendingBuffer.append(data);
	processData();
} // simulateData


void MainWindow::delayedSimulate(ProcessingState newState, const QByteArray& data)
{
	m_simulatedState = newState;
	m_delayedData = data;
	QTimer::singleShot(20, Qt::CoarseTimer, this, SLOT(simTimerExpired()));
} // delayedSimulate


void MainWindow::delayedSimNoResponse(ProcessingState newState, const QByteArray& data)
{
	m_simulatedState = newState;
	if(data.size())
		simulateData(data);
	QTimer::singleShot(20, Qt::CoarseTimer, this, SLOT(simTimerExpiredNoResp()));
} // delayedSimNoResponse

void MainWindow::simTimerExpired()
{
	simulateData(m_delayedData);
}

void MainWindow::simTimerExpiredNoResp()
{
	writePort(QByteArray(), false);
}

#else
void MainWindow::simulateData(const QByteArray&) {}
void MainWindow::delayedSimulate(ProcessingState, const QByteArray&) {}
void MainWindow::simTimerExpired() {}
void MainWindow::delayedSimNoResponse(ProcessingState, const QByteArray&) {}
void MainWindow::simTimerExpiredNoResp() {}
#endif

void MainWindow::writePort(const QByteArray &data, bool flush)
{
	if(simsOff == m_simulatedState) {
		if(m_port.isOpen()) {
			m_port.write(data);
			if(flush)
				m_port.flush();
		}
	}
	else {
		LogHexData(data, "W#%1:");
		switch(m_simulatedState) {

			case simsDriveStat:
				delayedSimulate(simsDriveStatString, QByteArray().append('E').append(data.at(1)));
				break;

			case simsDriveStatString:
				m_simulatedState = simsOff;
				// Write out the drive status.
				writeTextToDirList(QString(data.data()) + "\nREADY.\n");
				break;

			case simsOpenResponse:
				if('>' == data.at(0)) {
					if(O_DIR == data.at(1) or O_INFO == data.at(1))
						delayedSimulate(simsDisplayDirEntry, QByteArray().append('L'));
					else if(O_FILE == data.at(1))
						delayedSimulate(simsLoadCmd, QByteArray().append('N').append(64));
					else if(O_NOTHING == data.at(1)) {
						writeTextToDirList("?FILE NOT FOUND\n");
						writeTextToDirList("READY.\n");
						m_simulatedState = simsOff;
					}
					else if(O_FILE_ERR == data.at(1)) {
						writeTextToDirList("?FILE ERROR\n");
						writeTextToDirList("READY.\n");
						m_simulatedState = simsOff;
					}
					// TODO: check more return codes!
				}
				break;

			case simsDisplayDirEntry:
				if('L' == data.at(0)) {
					uint fSize = (((ushort)data.at(3)) << 8) + (uchar)data.at(2);
					writeTextToDirList(QString::number(fSize) + ' ' + data.mid(4).data());
					delayedSimulate(simsDisplayDirEntry, QByteArray().append('L'));
				}
				else {
					if('l' == data.at(0))
						writeTextToDirList("READY.\n");
					else
						writeTextToDirList("LOADING ERROR.\n");
					if(m_simFile.isOpen())
						m_simFile.close();
					m_simulatedState = simsOff;
				}
				break;

			case simsDriveCmd:
				// Note: This doesn't return anything and isn't supposed to.
				writeTextToDirList("READY.\n");
				m_simulatedState = simsOff;
				break;

			case simsLoadCmd:
				if('B' == data.at(0)) {
					// TODO: Write data.at(1) number of bytes starting at data.at(2) to simulated result binary file!
					// Still got bytes to read.
					m_simFile.write(data.mid(2, (int)(uchar)data.at(1)));
					delayedSimulate(simsLoadCmd, QByteArray().append('R'));
				}
				else {
					if('E' == data.at(0)) { // Last chunk
						m_simFile.write(data.mid(2, (int)(uchar)data.at(1)));
						// simulate closing.
						delayedSimulate(simsCloseCmd, QByteArray().append('C'));
					}
					else {
						writeTextToDirList("LOADING ERROR.\n");
						m_simulatedState = simsOff;
						m_simFile.close();
					}
				}
				break;

			case simsOpenSaveResponse:
				if('>' == data.at(0) && CBM::ErrOK == (uchar)data.at(1))
					delayedSimNoResponse(simsSaveCmd, QByteArray());
				else {
					writeTextToDirList("?SAVING ERROR.\nREADY.\n");
					m_simulatedState = simsOff;
					m_simFile.close();
				}

			break;
			case simsSaveCmd:
				{
					QByteArray readBytes = m_simFile.read(qMin(m_simFile.size() - m_simFile.pos(), (qint64)254));
					readBytes.prepend((uchar)readBytes.size() + 2);
					readBytes.prepend('W');
					if(m_simFile.atEnd())
						delayedSimulate(simsCloseCmd, readBytes.append('C'));
					else
						delayedSimNoResponse(simsSaveCmd, readBytes);
				}
				break;

			case simsCloseCmd:
					Log("SIM", success, QString("Got close response of file: %1, last operation was: %2").arg(QString(data.mid(2, data.at(1))))
							.arg(data.at(0) == 'n' ? "SAVE" : data.at(0) == 'N' ? "LOAD" : "Unknown"));
					// TODO: Close the simulated result binary file.
					m_simFile.close();
					m_simulatedState = simsOff;
				break;

			default:
				Log("SIM", error, QString("Unexpected simulation state: %1").arg(QString::number(uint(m_simulatedState))));
				break;
		}
	}
} // writePort


void MainWindow::onCommandIssued(const QString& cmd)
{
	//	QByteArray request;
	if(cmd.isEmpty())
		return;
	QString params(cmd.mid(1));

	Log("MAIN", info, QString("Command issued: %1").arg(cmd));
	// simulate dos-wedge like commands.
	if('@' == cmd[0]) {
		if(params.isEmpty()) {
			// Display (and clear) the disk drive status
			m_simulatedState = simsDriveStat;
			simulateData(QByteArray().append(QChar('O')).append(3).append(CBM::CMD_CHANNEL));
		}
		else if("$" == params) {
			// "Display the disk directory without overwriting the BASIC program in memory"
			m_simulatedState = simsOpenResponse;
			simulateData(QByteArray().append(QChar('O')).append(3 + params.length()).append(CBM::READPRG_CHANNEL).append(params.toLocal8Bit()));
		}
		else {
			// Execute a disk drive command (e.g. S0:filename, V0:, I0:)
			if(params.startsWith("M-W")) {
				params.append(0xF0);
				params.append(0x17);
				params.append(0x30);
				params.append("ARNE BJARNE_    0123456789ABCDEFG");
			}
			delayedSimNoResponse(simsDriveCmd, QByteArray().append(QChar('O')).append(3 + params.length()).append(CBM::CMD_CHANNEL).append(params.toLocal8Bit()));
		}
	}
	else if((cmd[0] == '/' or cmd[0] == '%')) {
		// Load a basic program into ram.
		 if(params.isEmpty())
			 writeTextToDirList("?SYNTAX ERROR\nREADY.");
		 else {
			 m_simulatedState = simsOpenResponse;
			 m_simFile.setFileName("simulated.prg");
			 m_simFile.open(QIODevice::WriteOnly);
			 simulateData(QByteArray().append(QChar('O')).append(3 + params.length()).append(CBM::READPRG_CHANNEL).append(params.toLocal8Bit()));
		 }
	}
	else if(cmd[0] == CBM_BACK_ARROW) {
		// Save a BASIC program to disk
		m_simFile.setFileName("simulated.prg");
		if(m_simFile.open(QIODevice::ReadOnly)) {
			m_simulatedState = simsOpenSaveResponse;
			simulateData(QByteArray().append(QChar('O')).append(3 + params.length()).append(CBM::WRITEPRG_CHANNEL).append(params.toLocal8Bit()));
		}
	}
	else {
		// unknown command, send syntax error.
		writeTextToDirList("?SYNTAX ERROR\nREADY.");
	}
} // onCommandIssued


void MainWindow::processData(void)
{
	bool hasDataToProcess = not m_pendingBuffer.isEmpty();
	//	if(hasDataToProcess)
	//		LogHexData(buffer);
	while(hasDataToProcess) {
		QString cmdString(m_pendingBuffer);
		int crIndex =	cmdString.indexOf('\r');

		// Get the first waiting character, which should be the command to perform.
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
				break;

			case 'O': // open command
				if(m_pendingBuffer.size() > 1) {
					uchar length = (uchar)m_pendingBuffer.at(1);
					if(length < 3) // sanity: can't be a valid command if total length is less than first control chars.
						m_pendingBuffer.remove(0, 2); // remove strange garbage and keep processing.
					else if(m_pendingBuffer.size() >= length) { // only if we got at least as much as length specifies.
						// Open was issued, string goes from m_pendingBuffer[2] with length - 2
						m_iface.processOpenCommand((uchar)m_pendingBuffer.at(2), m_pendingBuffer.mid(3, length - 3));
						m_pendingBuffer.remove(0, length);
					}
					else
						hasDataToProcess = false; // not all chars yet
				}
				else
					hasDataToProcess = false; // not all chars yet
				break;

			case 'R':
				// read byte(s) from current file system driver, note that this command needs no termination char,
				// because it needs to be short.
				// The payload given back will be the current size, it is by default MAX_BYTES_PER_REQUEST (or as many left to
				// read) but may be changed with 'N' command.
				m_pendingBuffer.remove(0, 1);
				m_iface.processReadFileRequest();
				break;

			case 'N': // same as 'N', but we are also given the expected read size. All succeeding 'R' will be with this size.
				if(m_pendingBuffer.size() < 2)
					hasDataToProcess = false;
				else {
					uchar length = (uchar)m_pendingBuffer.at(1);
					m_pendingBuffer.remove(0, 2);
					m_iface.processReadFileRequest(length);
				}
				break;

			case 'W': // write characters to file in current file system mode.
				if(m_pendingBuffer.size() > 1) {
					uchar length = (uchar)m_pendingBuffer.at(1);
					if(m_pendingBuffer.size() >= length) {
						m_iface.processWriteFileRequest(m_pendingBuffer.mid(2, length - 2));
						// discard all processed (written) bytes from buffer.
						m_pendingBuffer.remove(0, length);
					}
					else
						hasDataToProcess = false; // not all chars yet
				}
				else
					hasDataToProcess = false; // not all chars yet
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
				if(m_pendingBuffer.size() < 2) // must have both characters, otherwise request is incomplete.
					hasDataToProcess = false;
				else {
					m_iface.processErrorStringRequest(static_cast<CBM::IOErrorMessage>(m_pendingBuffer.at(1)));
					m_pendingBuffer.remove(0, 2);
				}
				break;

			default:
				// See if it is a reconnection attempt.
			if(not checkConnectRequest()) {
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

	//	if(not buffer.isEmpty())
	//		LogHexData(m_pendingBuffer, "U:#%1");
} // processData


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


void MainWindow::on_filterSetup_clicked()
{
	Logging::loggerInstance().configureFilters(this);
} // on_filterSetup_clicked


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

	if(ui->log->toPlainText().length() and not ui->saveHtml->isEnabled()) {
		ui->saveHtml->setEnabled(true);
		ui->saveLog->setEnabled(true);
		ui->clearLog->setEnabled(true);
		ui->pauseLog->setEnabled(true);
	}
} // appendMessage


void MainWindow::watchDirectory(const QString& dir)
{
	// If the directory is not already under monitoring, we remove all present ones and start monitoring it.
	if(not m_fsWatcher.directories().contains(dir, Qt::CaseInsensitive)) {
		if(not m_fsWatcher.directories().isEmpty())
			m_fsWatcher.removePaths(m_fsWatcher.directories());
		m_fsWatcher.addPath(dir);
	}
} // watchDirectory


void MainWindow::updateImageList(bool reloadDirectory)
{
	if(not m_isInitialized)
		return;

	QStringList filterList = m_appSettings.imageFilters.split(',', QString::SkipEmptyParts);

	if(reloadDirectory) {
		m_infoList = QDir(m_appSettings.imageDirectory).entryInfoList(filterList, QDir::NoDot bitor QDir::Files
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
	//	on_dirList_doubleClicked
}

void MainWindow::on_directoryChanged(const QString& path)
{
	if(0 == path.compare(m_appSettings.imageDirectory, Qt::CaseInsensitive)) {
		updateImageList();
		Log("MAIN", info, "Directory of monitored CBM image list was changed, updated all items in view.");
	}
} // on_directoryChanged


void MainWindow::on_mountSelected_clicked()
{
	QModelIndexList selected = ui->dirList->selectionModel()->selectedRows(0);
	if(not selected.size())
		return;
	QString name = selected.first().data(Qt::DisplayRole).toString();

	m_iface.processOpenCommand(CBM::READPRG_CHANNEL, name.toLocal8Bit(), true);
} // on_mountSelected_clicked


void MainWindow::on_unmountCurrent_clicked()
{
	m_iface.processOpenCommand(CBM::READPRG_CHANNEL, QByteArray().append(QChar(CBM_BACK_ARROW))
														 .append(QChar(CBM_BACK_ARROW)), true);
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
	m_appSettings.imageDirectory = newPath;
	ui->nowMounted->setText("None, Local FS: " + newPath);
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
				rvs xor_eq true;
			}
			ui->imageDirList->insertPlainText("\n");
		}
		m_imageDirListing.clear();
	}
	ui->unmountCurrent->setEnabled(true);

	int ix = 0;
	// select the image name in the directory file list!
	foreach(QFileInfo finfo, m_filteredInfoList) {
		if(not finfo.fileName().compare(imagePath, Qt::CaseInsensitive)) {
			ui->dirList->setFocus();
			ui->dirList->selectionModel()->setCurrentIndex(m_dirListItemModel->index(ix ,0),
																										 QItemSelectionModel::Rows bitor QItemSelectionModel::SelectCurrent);
			break;
		}
		++ix;
	}
	// Note: Doing this depends whether user really want to:
	// 1. Keep focus on the last clicked image, more clear when it is shown in active blue color.
	// 2. Or keep the Image's directory listing active meaning that the cursor blinks.
	ui->imageDirList->setFocus();
} // imageMounted


void MainWindow::imageUnmounted()
{
	ui->imageDirList->clear();
	ui->nowMounted->setText("None, Local FS: " + m_appSettings.imageDirectory);
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
	writeTextToDirList(QString("LOAD\"%1\",%2\n\nSEARCHING FOR %1\nLOADING").arg(fileName, QString::number(m_appSettings.deviceNumber)));
	cbmCursorVisible(false);
} // fileLoading


void MainWindow::fileSaving(const QString& fileName)
{
	m_loadSaveName = fileName;
	ui->loadProgress->hide();
	ui->progressInfoText->clear();
	ui->progressInfoText->setEnabled(true);
	writeTextToDirList(QString("SAVE\"%1\",%2\n\nSAVING %1").arg(fileName, QString::number(m_appSettings.deviceNumber)));
	QTextCursor cursor = ui->imageDirList->textCursor();
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


void MainWindow::writeTextToDirList(const QString& text, bool atCursorPos)
{
	QColor bgColor, frColor, fgColor;
	getBgFrAndFgColors(bgColor, frColor, fgColor);
	QTextCursor c = ui->imageDirList->textCursor();
	QStringList lines = text.split(QRegExp("[\r\n]"), QString::KeepEmptyParts);
	if(not atCursorPos)
		c.movePosition(QTextCursor::End);
	foreach(QString line, lines) {
//		c.deleteChar();
		c.select(QTextCursor::LineUnderCursor);

		// handle parts of string: Might have reverse video chars!
		QStringList lineInverses = line.split('\x12', QString::SkipEmptyParts);
		bool rvs = false;
		foreach(QString linePart, lineInverses) {
			QTextCharFormat fmt(c.charFormat());
			if(rvs) {
				fmt.setForeground(bgColor);
				fmt.setBackground(fgColor);
			}
			else {
				fmt.setForeground(fgColor);
				fmt.setBackground(bgColor);
			}
			c.setCharFormat(fmt);
			c.insertText(linePart);
			rvs xor_eq true;
		}

		if(not c.atEnd()) {
			c.movePosition(QTextCursor::Down);
			c.movePosition(QTextCursor::StartOfLine);
		}
		else {
			c.insertText("\n");
			ui->imageDirList->setTextCursor(c);
			if(ui->imageDirList->currentLineNumber() >= CBM::MAX_CBM_SCREEN_ROWS) {
				c.movePosition(QTextCursor::Start);
				c.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, ui->imageDirList->currentLineNumber() - CBM::MAX_CBM_SCREEN_ROWS);
				c.removeSelectedText();
				c.movePosition(QTextCursor::End);
			}
		}
	}
//	int height = ui->imageDirList->geometry().height() - 120;
//	Log("test", info, QString("wrows: %1").arg(QString::number(height / QFontMetrics(ui->imageDirList->font()).lineSpacing())));
	ui->imageDirList->setTextCursor(c);
} // addTextToDirList


void MainWindow::fileClosed(const QString& lastFileName)
{
	Q_UNUSED(lastFileName);
	ui->progressInfoText->setText(QString("READY. (%1 %2 bytes)").arg(m_loadSaveName).arg(m_totalReadWritten));
	ui->loadProgress->setEnabled(false);
	ui->loadProgress->setValue(0);
	ui->loadProgress->show();
	writeTextToDirList("READY.");
	cbmCursorVisible();
} // fileClosed


bool MainWindow::isWriteProtected() const
{
	return ui->actionDisk_Write_Protected->isChecked();
} // isWriteProtected


ushort MainWindow::deviceNumber() const
{
	return m_appSettings.deviceNumber;
} // deviceNumber


void MainWindow::setDeviceNumber(ushort deviceNumber)
{
	m_appSettings.deviceNumber = deviceNumber;
} // setDeviceNumber


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
	EmulatorPaletteMap::iterator itEmulatorPalette(emulatorPalettes.find(m_appSettings.emulatorPalette));
	CbmMachineThemeMap::iterator itMachineTheme(machineThemes.find(m_appSettings.cbmMachine));
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

	// Construct and apply a stylesheet that matches the chosen machine type and emulator palette.
	QString sheet = QString("background-color: rgb(%1);\ncolor: rgb(%2);\n"
													"border: %3px solid %4;\npadding: -4px;\n"
													"font: %5;\n")
				.arg(QString::number(bgColor.red()) + "," + QString::number(bgColor.green()) + "," + QString::number(bgColor.blue()))
				.arg(QString::number(fgColor.red()) + "," + QString::number(fgColor.green()) + "," + QString::number(fgColor.blue()))
				.arg(m_appSettings.cbmBorderWidth)
				.arg(frColor.name())
				.arg(pMachine->font);
	ui->imageDirList->setStyleSheet(sheet);
	ui->imageDirList->setCursorWidth(pMachine->cursorWidth);
	// TODO: Do we really need to issue reset here. Might just well remember the contents and then restore it.
	deviceReset();
} // updateDirListColors
