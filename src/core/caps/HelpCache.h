#pragma once

#include "HelpParser.h"
#include <QObject>
#include <QQueue>
#include <QSet>

namespace ffgui
{
class LogStore;
class ProcessJob;

// Serial, asynchronous help requests, cached by binary path, size and mtime.
// Keeps startup cheap: callers request only the options they display.
class HelpCache : public QObject
{
    Q_OBJECT
public:
    explicit HelpCache(LogStore *log = nullptr, QObject *parent = nullptr);
    ~HelpCache() override;
    void request(const QString &kind, const QString &name);
    void reset();
    void setLog(LogStore *log) { m_log = log; }
signals:
    void ready(const QString &key, const QList<ffgui::OptionSpec> &options);

private:
    void next();
    int m_generation = 0;
    LogStore *m_log;
    QQueue<QString> m_pending;
    QSet<QString> m_requested;
    ProcessJob *m_job = nullptr;
};
} // namespace ffgui
