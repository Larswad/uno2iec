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

class MainWindow : public QMainWindow, public Logging::ILogTransport, public Interface::IFileOpsNotify,
		public ISendLine
{
	Q_OBJECT

	typedef QMap<QString, QFileInfo> QFileInfoMap;

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void processAddNewFacility(const QString &str);
	void checkVersion();
	void closeEvent(QCloseEvent *event);

	// IMountNotifyListener interface implementation
	void directoryChanged(const QString& newPath);
	void imageMounted(const QString&imagePath, FileDriverBase*pFileSystem);
	void imageUnmounted();
	void fileLoading(const QString &fileName, ushort fileSize);
	void fileSaving(const QString& fileName);
	void bytesRead(uint numBytes);
	void bytesWritten(uint numBytes);
	void fileClosed(const QString &lastFileName);

	// ISendLine interface implementation.
	void send(short lineNo, const QString &text);

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

		void on_actionAbout_triggered();

		void on_unmountCurrent_clicked();

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
	QString m_programVersion;
	QStringList m_imageDirListing;
};

#endif // MAINWINDOW_HPP
