#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <qextserialport.h>
#include <QMap>
#include "interface.hpp"
#include "logger.hpp"
#include "settingsdialog.hpp"

namespace Ui {
class MainWindow;
}


struct CbmMachineTheme {
		QString bootText;
		QString font;
		uchar fgColorIndex;
		uchar frColorIndex;
		uchar bgColorIndex;
		uchar cursorWidth;
};

typedef QMap<QString, const QRgb*> EmulatorPaletteMap;
typedef QMap<QString, CbmMachineTheme*> CbmMachineThemeMap;
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
		void imageMounted(const QString& imagePath, FileDriverBase* pFileSystem);
		void imageUnmounted();
		void fileLoading(const QString &fileName, ushort fileSize);
		void fileSaving(const QString& fileName);
		void bytesRead(uint numBytes);
		void bytesWritten(uint numBytes);
		void fileClosed(const QString &lastFileName);
		bool isWriteProtected() const;
		void deviceReset();

		// ISendLine interface implementation.
		void send(short lineNo, const QString &text);

		// ILogTransport implementation.
		void appendTime(const QString& dateTime);
		void appendLevelAndFacility(Logging::LogLevelE level, const QString& levelFacility);
		void appendMessage(const QString& msg);

private slots:
				void onDirListColorSelected(QAction *pAction);
				void onCbmMachineSelected(QAction *pAction);
				void onDataAvailable();
				void on_clearLog_clicked();
				void on_pauseLog_toggled(bool checked);
				void on_saveLog_clicked();
				void on_saveHtml_clicked();
				void on_resetArduino_clicked();
				void on_browseImageDir_clicked();
				void on_imageDir_editingFinished();
				void on_imageFilter_textChanged(const QString &filter);
				void on_mountSelected_clicked();
				void on_browseSingle_clicked();
				void on_mountSingle_clicked();
				void on_actionAbout_triggered();
				void on_unmountCurrent_clicked();
				void on_actionSettings_triggered();
				void on_reloadImageDir_clicked();
				void on_dirList_doubleClicked(const QModelIndex &index);
				void on_directoryChanged(const QString& path);
private:
		bool checkConnectRequest();
		void enumerateComPorts();
		void usePortByFriendlyName(const QString &friendlyName);
		void processDebug(const QString &str);
		void watchDirectory(const QString& dir);
		void updateImageList(bool reloadDirectory = true);
		void boldifyItem(QStandardItem *pItem);
		void readSettings();
		void writeSettings() const;

		void setupActionGroups();
		void selectActionByName(const QList<QAction *>& actions, const QString& name) const;
		void updateDirListColors();
		void getBgFrAndFgColors(QColor &bgColor, QColor& frColor, QColor &fgColor);
		void getMachineAndPaletteTheme(CbmMachineTheme*& pMachine, const QRgb *&pEmulatorPalette);
		void cbmCursorVisible(bool visible = true);

		Ui::MainWindow *ui;
		QextSerialPort m_port;
		QByteArray m_pendingBuffer;
		bool m_isConnected;
		FacilityMap m_clientFacilities;
		Interface m_iface;
		QextPortInfoList m_ports;
		QStandardItemModel* m_dirListItemModel;
		QFileInfoList m_filteredInfoList;
		QFileInfoList m_infoList;
		bool m_isInitialized;
		QStringList m_imageDirListing;
		AppSettings m_appSettings;
		ushort m_totalReadWritten;
		QString m_loadSaveName;
		QFileSystemWatcher m_fsWatcher;
};

#endif // MAINWINDOW_HPP
