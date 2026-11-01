#pragma once

#include "ProbeResult.h"

#include <QObject>
#include <QPointer>

// Runs ffprobe on one input, asynchronously.
namespace ffgui {

class LogStore;
class ProcessJob;

class Prober : public QObject
{
    Q_OBJECT

public:
    explicit Prober(const QString &input, LogStore *log = nullptr, QObject *parent = nullptr);
    void start();
    QString input() const { return m_input; }

signals:
    // `warnings` are ffprobe's own complaints (demuxer warnings, junk data).
    void finished(bool ok, const ffgui::ProbeResult &result, const QString &error, const QStringList &warnings);

private:
    QString m_input;
    QPointer<LogStore> m_log;
    ProcessJob *m_job = nullptr;
};

} // namespace ffgui
