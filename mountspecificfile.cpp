#include <QFileDialog>
#include "mountspecificfile.h"
#include "ui_mountspecificfile.h"

MountSpecificFile::MountSpecificFile(const QString& lastMounted, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MountSpecificFile)
{
	ui->setupUi(this);
	ui->singleImageName->setText(lastMounted);
} // ctor


MountSpecificFile::~MountSpecificFile()
{
	delete ui;
} // dtor


void MountSpecificFile::on_browseSingle_clicked()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Please choose image to mount."), ui->singleImageName->text()
																							, tr("CBM Images (*.d64 *.t64 *.m2i);;All files (*)"));
	if(not file.isEmpty())
		ui->singleImageName->setText(file);
} // on_browseSingle_clicked


const QString MountSpecificFile::chosenFile()
{
	return ui->singleImageName->text();
} // chosenFile

void MountSpecificFile::on_mountSingle_clicked()
{
	accept();
} // on_mountSingle_clicked


void MountSpecificFile::on_cancel_clicked()
{
	reject();
} // on_cancel_clicked
