#ifndef QCMDTEXTEDIT_H
#define QCMDTEXTEDIT_H

#include <QTextEdit>

class QCmdTextEdit : public QTextEdit
{
	Q_OBJECT
public:
	explicit QCmdTextEdit(QWidget* parent = 0);

	int currentLineNumber();

signals:
	void commandIssued(const QString& cmd);

public slots:

protected:
	void keyPressEvent(QKeyEvent* e);
};

#endif // QCMDTEXTEDIT_H
