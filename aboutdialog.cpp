#include "aboutdialog.hpp"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(const QString& aboutText, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->m_aboutText->setText(aboutText);
} // ctor


AboutDialog::~AboutDialog()
{
	delete ui;
} // dtor
