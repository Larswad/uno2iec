#include <QKeyEvent>
#include <QTextBlock>

#include "qcmdtextedit.h"

QCmdTextEdit::QCmdTextEdit(QWidget* parent) :
	QTextEdit(parent)
{
	// make all text editing look upper case.
	QTextCursor cursor(textCursor());
	QTextCharFormat format(cursor.charFormat());
	format.setFontCapitalization(QFont::AllUppercase);
	cursor.setCharFormat(format);
	setTextCursor(cursor);
} // ctor


void QCmdTextEdit::keyPressEvent(QKeyEvent* e)
{
	QTextCursor c = textCursor();
	Q_UNUSED(c);

	switch (e->key()) {
//		case Qt::Key_Escape:
//		// whatever
//		break;
		case Qt::Key_Enter:
		case Qt::Key_Return:
			emit commandIssued(textCursor().block().text().trimmed().toLatin1().toUpper());
		break;
	}
	QTextEdit::keyPressEvent(e);
} // keyPressEvent
