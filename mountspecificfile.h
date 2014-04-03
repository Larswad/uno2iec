#ifndef MOUNTSPECIFICFILE_H
#define MOUNTSPECIFICFILE_H

#include <QDialog>

namespace Ui {
class MountSpecificFile;
}

class MountSpecificFile : public QDialog
{
	Q_OBJECT

public:
	explicit MountSpecificFile(const QString& lastMounted, QWidget *parent = 0);
	~MountSpecificFile();

	const QString chosenFile();

private slots:
	void on_browseSingle_clicked();

	void on_mountSingle_clicked();

	void on_cancel_clicked();

private:
	Ui::MountSpecificFile* ui;
};

#endif // MOUNTSPECIFICFILE_H
