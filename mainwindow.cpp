// TODO: Finalize M2I handling. What exactly is the point of that FS, is it to handle 8.3 filenames to cbm 16 byte lengths?
// TODO: Finalize Native FS routines.
// TODO: Handle all data channel stuff. TALK, UNTALK, and so on.
// TODO: Parse date and time on connection at arduino side.
// TODO: T64 / D64 formats should read out entire disk into memory for caching (network performance).
// DONE: Check the port name to use on the PI in the port constructor / setPortName(QLatin1String("/dev/ttyS0"));

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <QString>
#include <QFileDialog>
#include <QTextStream>
#include <QDate>
#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

const QString OkString = "OK>";
const QColor logLevelColors[] = { QColor(Qt::red), QColor("orange"), QColor(Qt::blue), QColor(Qt::darkGreen) };

QStringList LOG_LEVELS = (QStringList()
													<< QObject::tr("error  ")
													<< QObject::tr("warning")
													<< QObject::tr("info   ")
													<< QObject::tr("success"));


MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow), m_port(QextSerialPort::EventDriven, this)
	, m_isConnected(false), m_iface(m_port)
{
	ui->setupUi(this);

	// just for the PI.

#ifdef __arm__
	m_port.setPortName(QLatin1String("/dev/ttyAMA0"));
	m_port.setBaudRate(BAUD1152000);
#else
	m_port.setPortName(QLatin1String("COM1"));
	m_port.setBaudRate(BAUD115200);
#endif
	Log("MAIN", QString("Application Started, using port %1 @ %2").arg(m_port.portName()).arg(QString::number(115200)), success);
	m_port.setDataBits(DATA_8);
	m_port.setParity(PAR_NONE);
	m_port.setFlowControl(FLOW_OFF);

	m_port.open(QIODevice::ReadWrite);

	connect(&m_port, SIGNAL(readyRead()), this, SLOT(onDataAvailable()));
#ifdef HAS_WIRINGPI
	system("/usr/local/bin/gpio -g mode 23 out");
	system("/usr/local/bin/gpio export 23 out");
	if(-1 == wiringPiSetupSys())
		Log("MAIN", "Failed initializing WiringPi. Continuing anyway...", error);
	else
		on_resetArduino_clicked();
#endif
	Log("MAIN", "Application Initialized.", success);
} // MainWindow


void MainWindow::onDataAvailable()
{
//	bool wasEmpty = m_pendingBuffer.isEmpty();
	m_pendingBuffer.append(m_port.readAll());

	if(!m_isConnected) {
		if(m_pendingBuffer.contains("CONNECT")) {
			m_pendingBuffer.clear();
			Log("MAIN", "Now connected to Arduino.", success);
			// give the client current date and time in the response string.
			m_port.write((OkString + QDate::currentDate().toString("yyyy-MM-dd") +
										 QTime::currentTime().toString(" hh:mm:ss:zzz") + '\r').toLatin1().data());
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
			case 'R': // read single byte from current file system mode, note that this command needs no termination, it needs to be short.
				m_pendingBuffer.remove(0, 1);
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			case 'S': // set read / write size in number of bytes.
				break;

			case 'W': // write single character to file in current file system mode.
				break;

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

			case 'M': // set mode and include command string.
				break;

			case 'O': // open command
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {// Open was issued, string goes from 1 to CRpos - 1
					Log("OPEN", QString("CMD: \"%1\"").arg(cmdString.mid(1, crIndex - 1)), info);
					m_iface.processOpenCommand(cmdString.mid(1, crIndex - 1));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
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


MainWindow::~MainWindow()
{
	if(m_port.isOpen())
		m_port.close();
	delete ui;
} // dtor


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


void MainWindow::Log(const QString& facility, const QString& message, LogLevelE level)
{
	if(!ui->pauseLog->isChecked()) {
		QTextCursor cursor = ui->log->textCursor();
		cursor.movePosition(QTextCursor::End);
		ui->log->setTextCursor(cursor);

		ui->log->setTextColor(Qt::darkGray);

		ui->log->insertPlainText(QDate::currentDate().toString("yyyy-MM-dd") +
											QTime::currentTime().toString(" hh:mm:ss:zzz"));
		ui->log->setFontWeight(QFont::Bold);
		ui->log->setTextColor(logLevelColors[level]);
		// The logging levels are: [E]RROR [W]ARNING [I]NFORMATION [S]UCCESS.
		ui->log->insertPlainText(" " + QString("EWIS")[level] + " " + facility + " ");
		ui->log->setFontWeight(QFont::Normal);
		ui->log->setTextColor(Qt::darkBlue);
		ui->log->insertPlainText(message + "\n");

		if(ui->log->toPlainText().length() and !ui->saveHtml->isEnabled()) {
			ui->saveHtml->setEnabled(true);
			ui->saveLog->setEnabled(true);
			ui->clearLog->setEnabled(true);
			ui->pauseLog->setEnabled(true);
		}
	}
} // Log


void MainWindow::on_pauseLog_toggled(bool checked)
{
	ui->pauseLog->setText(checked ? tr("Resume") : tr("Pause"));
} // on_m_pauseLog_toggled


void MainWindow::on_resetArduino_clicked()
{
#ifdef HAS_WIRINGPI
	Log("MAIN", "Moving to disconnected state and resetting arduino...", warning);
	m_isConnected = false;
	// pull pin 23 to reset arduino.
	pinMode(23, OUTPUT);
	digitalWrite(23, 0);
	delay(3000);
	Log("MAIN", "Releasing reset state...", info);
	// set it high again to release reset state.
	digitalWrite(23, 1);
#endif
} // on_resetArduino_clicked
