#include "logger.hpp"
#include <QDate>

namespace Logging {

Logger::Logger(QObject *parent) : QObject(parent)
{}

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


void Logger::configureFilters()
{
	LogFilterSetup dlgSetup(m_filters, m_levels);
	dlgSetup.exec();
} // configureFilters


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
