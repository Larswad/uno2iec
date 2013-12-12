#ifdef _MSC_VER
#include <iso646.h>
#endif

#include "logfiltersetup.hpp"
#include "ui_logfiltersetup.h"

LogFilterSetup::LogFilterSetup(LogFilterMap& logFilters, QVector<bool>& logLevels, QWidget* parent) :
		QDialog(parent),
		m_logFilters(logFilters),
		m_logLevels(logLevels),
		ui(new Ui::LogFilterSetup)
{
		ui->setupUi(this);
		LogFilterMap::const_iterator it;
		int ix = 0;
		for(it = m_logFilters.begin(); it not_eq m_logFilters.end(); ++it, ++ix) {
				ui->m_facilityFilterList->addItem(it.key());
				ui->m_facilityFilterList->item(ix)->setCheckState(it.value() ? Qt::Checked : Qt::Unchecked);
		}

		for(ix = 0; ix < m_logLevels.size(); ++ix)
				ui->m_severityFilterList->item(ix)->setCheckState(m_logLevels[ix] ? Qt::Checked : Qt::Unchecked);
} // ctor


LogFilterSetup::~LogFilterSetup()
{
		delete ui;
} // dtor


void LogFilterSetup::on_m_close_clicked()
{
		close();
} // close


void LogFilterSetup::on_m_facilityFilterList_itemChanged(QListWidgetItem *item)
{
		m_logFilters[item->text()] = item->checkState() == Qt::Checked;
}


void LogFilterSetup::on_m_severityFilterList_itemChanged(QListWidgetItem *item)
{
		m_logLevels[ui->m_severityFilterList->row(item)] = item->checkState() == Qt::Checked;
}
