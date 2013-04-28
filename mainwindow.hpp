#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <qextserialport.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

public slots:
		void onDataAvailable();

private:
	Ui::MainWindow *ui;
	QextSerialPort* m_port;
};

#endif // MAINWINDOW_HPP
