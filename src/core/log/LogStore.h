#pragma once

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>

// Every job's output and ffgui's own messages, in order: { time, jobId,
// level, text }. Each job's exact command line is kept too. Jobs log from
// worker threads; `appended` arrives in the receiver's thread.
namespace ffgui {

class LogStore : public QObject
{
    Q_OBJECT

public:
    struct Entry
    {
        QDateTime time;
        QString jobId;
        QString level;      // debug, info, warning, error
        QString text;
        QString source;     // probe, analysis, decode, playback, caps, ffgui, ...
    };

    explicit LogStore(QObject *parent = nullptr);

    // A new job id, e.g. "probe-12".
    QString newJob(const QString &source, const QString &commandLine);
    void append(const QString &jobId, const QString &level, const QString &text, const QString &source = {});
    QString command(const QString &jobId) const;

signals:
    void appended(const ffgui::LogStore::Entry &entry);

private:
    mutable QMutex m_mutex;
    QHash<QString, QString> m_commands;
    QHash<QString, QString> m_sources;
    int m_next = 1;
};

} // namespace ffgui
