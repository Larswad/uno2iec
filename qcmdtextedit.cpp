#include <QKeyEvent>
#include <QTextBlock>

#include "qcmdtextedit.h"
#include "logger.hpp"
#include "uno2iec/cbmdefines.h"

QCmdTextEdit::QCmdTextEdit(QWidget* parent) :
	QTextEdit(parent)
{
	// make all text editing look upper case.
	QTextCursor cursor(textCursor());
	QTextCharFormat format(cursor.charFormat());
	format.setFontCapitalization(QFont::AllUppercase);
	cursor.setCharFormat(format);
	setTextCursor(cursor);
	setInputMethodHints(Qt::ImhPreferUppercase);
} // ctor


int QCmdTextEdit::currentLineNumber()
{
	return textCursor().blockNumber() + 1;
} // currentLineNumber


void QCmdTextEdit::keyPressEvent(QKeyEvent* e)
{
	QTextCursor c = textCursor();
	QString command;
	bool processKey = true;

	switch(e->key()) {

		case Qt::Key_Enter:
		case Qt::Key_Return:
			// grab the line where the cursor is. This will be our command.
			command = c.block().text().trimmed().replace(0xe01f, Qt::Key_Underscore).replace(0xe01e, Qt::Key_AsciiCircum).toLatin1().toUpper();

			if(not c.atEnd()) {
				// Don't insert the newline character if we're not at the end of text buffer, just move to beginning of next line and issue the command.
				c.movePosition(QTextCursor::Down);
				c.movePosition(QTextCursor::StartOfLine);
				setTextCursor(c);
				processKey = false;
			}
			//		Logging::Log("test", Logging::info, QString("current line: %1").arg(c.blockNumber() + 1));
			// If we're at the end check whether the newline that will be generated will get us outside of the CBM screen. If so remove the top line so that
			// everything will scroll up one line.
			else if(currentLineNumber() >= CBM::MAX_CBM_SCREEN_ROWS) {
				c.movePosition(QTextCursor::Start);
				c.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, currentLineNumber() - CBM::MAX_CBM_SCREEN_ROWS + 1);
				c.removeSelectedText();
				c.movePosition(QTextCursor::End);
				setTextCursor(c);
			}
			break;

		case Qt::Key_Down:
			// Moving down should cause cursor to actually increase the document number of lines. But it should go only so
			// far that it reaches the terminal bottom, then start to scroll upwards by removing top lines.
			if(c.atEnd()) {
				//			Logging::Log("test", Logging::info, QString("current line: %1").arg(currentLineNumber()));
				c.insertText("\n");
				c.movePosition(QTextCursor::End);
				setTextCursor(c);
				if(currentLineNumber() >= CBM::MAX_CBM_SCREEN_ROWS) {
					c.movePosition(QTextCursor::Start);
					c.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, currentLineNumber() - CBM::MAX_CBM_SCREEN_ROWS);
					c.removeSelectedText();
					c.movePosition(QTextCursor::End);
				}
				setTextCursor(c);
				processKey = false;
			}
			break;

		case Qt::Key_AsciiCircum:
		case Qt::Key_Underscore:
			c.insertText(QChar(0xe000 + e->key() - 0x40));
			processKey = false;
			break;
	}

	// do normal processing, if supposed to.
	if(processKey)
		QTextEdit::keyPressEvent(e);
	if(not command.isEmpty())
		emit commandIssued(command);
} // keyPressEvent
