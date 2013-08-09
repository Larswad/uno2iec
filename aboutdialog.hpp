#ifndef ABOUTDIALOG_HPP
#define ABOUTDIALOG_HPP

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AboutDialog(const QString& aboutText, QWidget* parent = 0);
	~AboutDialog();

private:
	Ui::AboutDialog *ui;
};

#endif // ABOUTDIALOG_HPP
