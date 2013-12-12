#ifndef LOGFILTERSETUP_HPP
#define LOGFILTERSETUP_HPP

#include <QDialog>
#include <QListWidgetItem>
#include <QMap>

namespace Ui {
class LogFilterSetup;
}

typedef QMap<QString, bool> LogFilterMap;

class LogFilterSetup : public QDialog
{
		Q_OBJECT

public:
		explicit LogFilterSetup(LogFilterMap& logFilters, QVector<bool>& logLevels, QWidget *parent = 0);
		~LogFilterSetup();

private slots:
		void on_m_close_clicked();
		void on_m_facilityFilterList_itemChanged(QListWidgetItem *item);
		void on_m_severityFilterList_itemChanged(QListWidgetItem *item);

private:
		LogFilterMap& m_logFilters;
		QVector<bool>& m_logLevels;
		Ui::LogFilterSetup *ui;
};

#endif // LOGFILTERSETUP_HPP
