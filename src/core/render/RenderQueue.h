#pragma once
#include <command/CommandBuilder.h>
#include <tools/ProcessJob.h>
#include <QPointer>
#include <QUuid>

namespace ffgui
{
class LogStore;
struct RenderTask {
    QString uid = QUuid::createUuid().toString(QUuid::Id128);
    QString input, output;
    QStringList ownedPaths;
    BuiltCommand command;
    QString status = QStringLiteral("queued"), error;
    double progress = 0, speed = 0, eta = 0;
};
class RenderJob : public QObject
{
    Q_OBJECT
public:
    RenderJob(const BuiltCommand &command, LogStore *log, QObject *parent = nullptr);
    ~RenderJob() override;
    void start();
    void stop();
signals:
    void progress(double fraction, double speed, double eta);
    void finished(bool ok, bool stopped, const QString &error);

private:
    BuiltCommand m_command;
    ProcessJob m_job;
};
class RenderQueue : public QObject
{
    Q_OBJECT
public:
    explicit RenderQueue(LogStore *log, QObject *parent = nullptr);
    ~RenderQueue() override;
    const QList<RenderTask> &tasks() const { return m_tasks; }
    void add(const QList<RenderTask> &tasks);
    void setPaused(bool paused);
    bool paused() const { return m_paused; }
    void setConcurrency(int count);
    void action(const QString &action, int row);
    void stopAll();
signals:
    void changed();
    void completed(const ffgui::RenderTask &task);

private:
    void pump();
    int indexOf(const QString &id) const;
    LogStore *m_log;
    QList<RenderTask> m_tasks;
    QHash<QString, QPointer<RenderJob>> m_active;
    bool m_paused = false;
    int m_concurrency = 1;
};
class BatchExpander
{
public:
    static QList<RenderTask> expand(const QStringList &files, const QVariantMap &track, double duration,
                                    const QList<QVariantMap> &nodes, const QList<QVariantMap> &outputs,
                                    const Catalog &catalog, const BuildInfo &caps);
};
} // namespace ffgui
