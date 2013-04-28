#include "mainwindow.hpp"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow), m_port(0)
{
	ui->setupUi(this);
	m_port = new QextSerialPort("COM1"); // make configurable.
	connect(port, SIGNAL(readyRead()), this, SLOT(onDataAvailable()));
}

void MainWindow::onDataAvailable()
{
	QByteArray data = port->readAll();
	Q_UNUSED(data);
	//processNewData(usbdata);
}

MainWindow::~MainWindow()
{
	delete ui;
}
