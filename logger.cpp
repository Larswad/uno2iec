#include "logger.hpp"
#include <QDate>

namespace Logging {

Logger::Logger(QObject *parent) :
	QObject(parent)
{
}

void Logger::Log(const QString& facility, const QString& message, LogLevelE level)
{
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


bool Logger::AddTransport(ILogTransport* pTransport)
{
	if(m_transports.contains(pTransport))
		return false;

	m_transports.append(pTransport);
	return true;
} // AddTransport

bool Logger::RemoveTransport(ILogTransport* pTransport)
{
	int index = m_transports.indexOf(pTransport);
	if(-1 == index)
		return false;

	m_transports.removeAt(index);
	return true;
} // RemoveTransport

Logger& getLoggerInstance()
{
	static Logger theInstance;
	return theInstance;
}

void Log(const QString &facility, const QString &message, LogLevelE level)
{
	getLoggerInstance().Log(facility, message, level);
} // Log

} // namespace Logging
