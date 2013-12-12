#include "logger.hpp"
#include <QDate>
#include <QSettings>

namespace Logging {

Logger::Logger(QObject *parent) : QObject(parent)
{
	m_levels.fill(true, NUM_SEVERITY_LEVELS);
} // ctor


void Logger::log(const QString& facility, const QString& message, LogLevelE level)
{
	LogFilterMap::const_iterator it(m_filters.find(facility));

	if(it == m_filters.end())
		m_filters[facility] = true; // add to existing facilities, enabled by default.
	else
		if(!it.value())
			return; // filtered out!

	// manage unlikely out-of-range value.
	level = level >= NUM_SEVERITY_LEVELS ? info : level;

	if(!m_levels[level]) // check if severity level is filtered out.
		return;

	QString dateTime(QDate::currentDate().toString("yyyy-MM-dd") +
									 QTime::currentTime().toString(" hh:mm:ss:zzz"));

	// The logging levels are: [E]RROR [W]ARNING [I]NFORMATION [S]UCCESS.
	QString levelFacility(QString("EWIS")[level] + " " + facility);

	foreach(ILogTransport* transport, m_transports) {
		transport->appendTime(dateTime);
		transport->appendLevelAndFacility(level, levelFacility);
		transport->appendMessage(message);
	}
} // Log


bool Logger::addTransport(ILogTransport* pTransport)
{
	if(m_transports.contains(pTransport))
		return false;

	m_transports.append(pTransport);
	return true;
} // addTransport


bool Logger::removeTransport(ILogTransport* pTransport)
{
	int index = m_transports.indexOf(pTransport);
	if(-1 == index)
		return false;

	m_transports.removeAt(index);
	return true;
} // removeTransport


void Logger::configureFilters(QWidget* parent = 0)
{
	LogFilterSetup dlgSetup(m_filters, m_levels, parent);
	dlgSetup.exec();
} // configureFilters


void Logger::saveFilters(QSettings& sets)
{
	sets.beginGroup("logFilters");
	LogFilterMap::const_iterator i = m_filters.constBegin();
	while(i not_eq m_filters.constEnd()) {
		sets.setValue(i.key(), i.value());
		++i;
	}
	sets.endGroup();

	// write severity levels.
	sets.beginWriteArray("logLevels");
	for(int i = 0; i < m_levels.size(); ++i) {
		sets.setArrayIndex(i);
		sets.setValue("isEnabled", m_levels[i]);
	}
	sets.endArray();
} // saveFilters


void Logger::loadFilters(QSettings& sets)
{
	sets.beginGroup("logFilters");
	QStringList keys = sets.childKeys();
	foreach(QString key, keys)
		m_filters[key] = sets.value(key).toBool();
	sets.endGroup();

	// read severity levels.
	int numEntries = sets.beginReadArray("logLevels");
	m_levels.resize(qMax(numEntries, m_levels.size()));
	for(int i = 0; i < numEntries; ++i) {
		sets.setArrayIndex(i);
		m_levels[i] = sets.value("isEnabled", true).toBool();
	}
	sets.endArray();
} // loadFilters


Logger& loggerInstance()
{
	static Logger theInstance;
	return theInstance;
} // loggerInstance


void Log(const QString &facility, LogLevelE level, const QString &message)
{
	loggerInstance().log(facility, message, level);
} // Log

} // namespace Logging
