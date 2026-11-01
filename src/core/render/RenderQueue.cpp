#include "RenderQueue.h"
#include <tools/FfTools.h>
#include <QFileInfo>
#include <QTimer>

namespace ffgui
{
RenderJob::RenderJob(const BuiltCommand &command, LogStore *log, QObject *parent)
    : QObject(parent), m_command(command), m_job(FfTools::paths().ffmpeg, command.args, this)
{
    m_job.setProgress(true);
    m_job.setLog(log, QStringLiteral("render"), QStringLiteral("info"));
    connect(&m_job, &ProcessJob::progress, this, [this](const ProgressParser::Snapshot &s) {
        const double fraction = m_command.duration > 0 ? std::clamp(s.outTime / m_command.duration, 0.0, 1.0) : 0;
        emit progress(fraction, s.speed, s.speed > 0 ? std::max(0.0, m_command.duration - s.outTime) / s.speed : 0);
    });
    connect(&m_job, &ProcessJob::finished, this, [this](bool ok) {
        emit finished(ok, m_job.stopped(), ok ? QString() : m_job.problems(QStringLiteral("error")).join(u'\n'));
    });
}
RenderJob::~RenderJob()
{
    disconnect(&m_job, nullptr, this, nullptr);
    m_job.stop();
    if (m_job.running()) m_job.waitForFinished(1500);
}

void RenderJob::start()
{
    if (!m_command.error.isEmpty()) {
        emit finished(false, false, m_command.error);
        return;
    }
    m_job.start();
}
void RenderJob::stop() { m_job.stop(); }
RenderQueue::RenderQueue(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}
RenderQueue::~RenderQueue()
{
    // Disconnect before QObject deletes children: the task list must stay
    // alive for normal callbacks, and no callback may outlive this queue.
    for (const auto &job : std::as_const(m_active))
        if (job) { disconnect(job, nullptr, this, nullptr); job->stop(); }
    for (const auto &job : std::as_const(m_active)) delete job.data();
    m_active.clear();
}

void RenderQueue::add(const QList<RenderTask> &tasks)
{
    m_tasks << tasks;
    emit changed();
    pump();
}
int RenderQueue::indexOf(const QString &id) const
{
    for (int i = 0; i < m_tasks.size(); ++i)
        if (m_tasks[i].uid == id)
            return i;
    return -1;
}
void RenderQueue::setPaused(bool paused)
{
    m_paused = paused;
    emit changed();
    pump();
}
void RenderQueue::setConcurrency(int count)
{
    m_concurrency = std::clamp(count, 1, 4);
    pump();
}
void RenderQueue::stopAll()
{
    for (auto &task : m_tasks)
        if (task.status == u"queued")
            task.status = QStringLiteral("cancelled");
    for (const auto &job : std::as_const(m_active))
        if (job)
            job->stop();
    emit changed();
}
void RenderQueue::action(const QString &action, int row)
{
    if (action == u"pause") {
        setPaused(true);
        return;
    }
    if (action == u"resume") {
        setPaused(false);
        return;
    }
    if (action == u"clearFinished") {
        m_tasks.removeIf([](const RenderTask &t) { return t.status != u"queued" && t.status != u"running"; });
    } else if (row >= 0 && row < m_tasks.size()) {
        auto &task = m_tasks[row];
        if (action == u"cancel") {
            if (auto *job = m_active.value(task.uid).data())
                job->stop();
            else
                task.status = QStringLiteral("cancelled");
        } else if (action == u"retry" && task.status != u"running" && task.status != u"queued") {
            bool ownsOutputs = !task.command.paths.isEmpty();
            for (const auto &path : task.command.paths)
                ownsOutputs &= task.ownedPaths.contains(path);
            if (ownsOutputs) {
                const int flag = task.command.args.indexOf(QStringLiteral("-n"));
                if (flag >= 0)
                    task.command.args[flag] = QStringLiteral("-y");
                if (task.command.groups.size() > 1)
                    task.command.groups[1].replace(QStringLiteral(" -n "), QStringLiteral(" -y "));
            }
            task.status = QStringLiteral("queued");
            task.error.clear();
            task.progress = 0;
        } else if (task.status == u"queued" && (action == u"up" || action == u"down")) {
            const int to = row + (action == u"up" ? -1 : 1);
            if (to >= 0 && to < m_tasks.size() && m_tasks[to].status == u"queued")
                m_tasks.move(row, to);
        }
    }
    emit changed();
    pump();
}
void RenderQueue::pump()
{
    if (m_paused)
        return;
    for (int i = 0; i < m_tasks.size() && m_active.size() < m_concurrency; ++i) {
        auto &task = m_tasks[i];
        if (task.status != u"queued")
            continue;
        task.status = QStringLiteral("running");
        for (const auto &path : task.command.paths)
            if (!QFileInfo::exists(path) && !task.ownedPaths.contains(path))
                task.ownedPaths << path;
        auto *job = new RenderJob(task.command, m_log, this);
        const QString id = task.uid;
        m_active.insert(id, job);
        connect(job, &RenderJob::progress, this, [this, id](double fraction, double speed, double eta) {
            const int i = indexOf(id);
            if (i < 0)
                return;
            m_tasks[i].progress = fraction;
            m_tasks[i].speed = speed;
            m_tasks[i].eta = eta;
            emit changed();
        });
        connect(job, &RenderJob::finished, this, [this, id, job](bool ok, bool stopped, const QString &error) {
            const int i = indexOf(id);
            m_active.remove(id);
            if (i >= 0) {
                auto &task = m_tasks[i];
                task.status = stopped ? QStringLiteral("stopped")
                              : ok    ? QStringLiteral("done")
                                      : QStringLiteral("failed");
                task.error = error;
                if (ok && !stopped)
                    task.progress = 1;
                emit completed(task);
            }
            job->deleteLater();
            emit changed();
            QTimer::singleShot(0, this, &RenderQueue::pump);
        });
        emit changed();
        job->start();
    }
}
QList<RenderTask> BatchExpander::expand(const QStringList &files, const QVariantMap &track, double duration,
                                        const QList<QVariantMap> &nodes, const QList<QVariantMap> &outputs,
                                        const Catalog &catalog, const BuildInfo &caps)
{
    QList<RenderTask> tasks;
    for (const auto &file : files)
        for (const auto &output : outputs) {
            RenderTask task;
            task.input = file;
            task.command = CommandBuilder::build(file, track.value(QStringLiteral("audioIndex")).toInt(), track,
                                                 duration, nodes, {output}, catalog, caps);
            task.output = task.command.paths.value(0);
            tasks << task;
        }
    return tasks;
}
} // namespace ffgui
