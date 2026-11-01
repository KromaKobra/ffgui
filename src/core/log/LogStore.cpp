#include "LogStore.h"

namespace ffgui {

LogStore::LogStore(QObject *parent)
    : QObject(parent)
{
}

QString LogStore::newJob(const QString &source, const QString &commandLine)
{
    QString id;
    {
        QMutexLocker lock(&m_mutex);
        id = source + u'-' + QString::number(m_next++);
        m_commands.insert(id, commandLine);
        m_sources.insert(id, source);
    }
    append(id, QStringLiteral("info"), QStringLiteral("$ ") + commandLine, source);
    return id;
}

QString LogStore::command(const QString &jobId) const
{
    QMutexLocker lock(&m_mutex);
    return m_commands.value(jobId);
}

void LogStore::append(const QString &jobId, const QString &level, const QString &text, const QString &source)
{
    QString src = source;
    if (src.isEmpty()) {
        QMutexLocker lock(&m_mutex);
        src = m_sources.value(jobId, QStringLiteral("ffgui"));
    }
    emit appended({ QDateTime::currentDateTime(), jobId, level, text, src });
}

} // namespace ffgui
