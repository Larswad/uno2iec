#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <QObject>

namespace Logging {

typedef enum {
	error,
	warning,
	info,
	success
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
	void Log(const QString &facility, const QString &message, LogLevelE level);
	bool AddTransport(ILogTransport* pTransport);
	bool RemoveTransport(ILogTransport* pTransport);

	static Logger& getLoggerInstance();
signals:

public slots:

private:
	LogTransportList m_transports;
};

Logger& getLoggerInstance();
void Log(const QString& facility, const QString& message, Logging::LogLevelE level);

} // namespace Logger


#endif // LOGGER_HPP
