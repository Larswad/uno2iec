#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <qextserialport.h>
#include <QMap>
#include "interface.hpp"
#include "logger.hpp"

namespace Ui {
class MainWindow;
}

typedef QMap<QChar, QString> FacilityMap;

class MainWindow : public QMainWindow, public Logging::ILogTransport
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void processAddNewFacility(const QString &str);

public slots:
		void onDataAvailable();

		// ILogTransport implementation.
		void appendTime(const QString& dateTime);
		void appendLevelAndFacility(Logging::LogLevelE level, const QString& levelFacility);
		void appendMessage(const QString& msg);

private slots:
		void on_clearLog_clicked();
		void on_pauseLog_toggled(bool checked);
		void on_saveLog_clicked();
		void on_saveHtml_clicked();
		void on_resetArduino_clicked();

private:
	void processDebug(const QString &str);

	Ui::MainWindow *ui;
	QextSerialPort m_port;
	QByteArray m_pendingBuffer;
	bool m_isConnected;
	FacilityMap m_clientFacilities;
	Interface m_iface;
};

#endif // MAINWINDOW_HPP
