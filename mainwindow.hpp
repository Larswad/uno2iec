#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QStandardItemModel>
#include <QFileInfo>
#include <qextserialport.h>
#include "qextserialenumerator.h"
#include <QMap>
#include "interface.hpp"
#include "logger.hpp"

namespace Ui {
class MainWindow;
}

typedef QMap<QChar, QString> FacilityMap;

class MainWindow : public QMainWindow, public Logging::ILogTransport, public Interface::IMountNotify
{
	Q_OBJECT

	typedef QMap<QString, QFileInfo> QFileInfoMap;

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void processAddNewFacility(const QString &str);
	void closeEvent(QCloseEvent *event);

	// IMountNotifyListener interface implementation
	void directoryChanged(const QString &);
	void imageMounted(const QString &, FileDriverBase *);
	void fileLoading(const QString &);
	void bytesRead(uint);
	void bytesWritten(uint);
	void fileClosed(const QString &);

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

		void on_comPort_currentIndexChanged(int index);

		void on_browseImageDir_clicked();

		void on_imageDir_editingFinished();

		void on_imageFilter_textChanged(const QString &arg1);

		void on_mountSelected_clicked();

		void on_browseSingle_clicked();

		void on_mountSingle_clicked();

		void on_baudRate_currentIndexChanged(const QString& baudRate);

private:
	void processDebug(const QString &str);
	void updateImageList();
	void readSettings();
	void writeSettings() const;

	Ui::MainWindow *ui;
	QextSerialPort m_port;
	QByteArray m_pendingBuffer;
	bool m_isConnected;
	FacilityMap m_clientFacilities;
	Interface m_iface;
	QList<QextPortInfo> m_ports;
	QStandardItemModel* m_dirListItemModel;
	QFileInfoMap m_imageInfoMap;
	bool m_isInitialized;
};

#endif // MAINWINDOW_HPP
