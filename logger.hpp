#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <QObject>
#include "logfiltersetup.hpp"

class QSettings;

namespace Logging {

typedef enum {
	error,
	warning,
	info,
	success,
	NUM_SEVERITY_LEVELS
} LogLevelE;

struct ILogTransport
{
	virtual void appendTime(const QString& dateTime) = 0;
	virtual void appendLevelAndFacility(LogLevelE level, const QString& levelFacility) = 0;
	virtual void appendMessage(const QString& msg) = 0;
};

class Logger : public QObject
{
	Q_OBJECT

	typedef QList<ILogTransport*> LogTransportList;
public:
	explicit Logger(QObject *parent = 0);
	void log(const QString &facility, const QString &message, LogLevelE level);
	bool addTransport(ILogTransport* pTransport);
	bool removeTransport(ILogTransport* pTransport);

	// filters
	void configureFilters(QWidget *parent);
	void saveFilters(QSettings& sets);
	void loadFilters(QSettings& sets);

	static Logger& getLoggerInstance();
signals:

public slots:

private:
	LogTransportList m_transports;
	LogFilterMap m_filters;
	QVector<bool> m_levels;
};

Logger& loggerInstance();
void Log(const QString& facility, Logging::LogLevelE level, const QString& message);

} // namespace Logger


#endif // LOGGER_HPP
