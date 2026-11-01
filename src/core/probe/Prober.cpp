#include "Prober.h"
#include "ProbeParser.h"

#include <log/LogStore.h>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>

namespace ffgui {

Prober::Prober(const QString &input, LogStore *log, QObject *parent)
    : QObject(parent)
    , m_input(input)
    , m_log(log)
{
}

void Prober::start()
{
    QStringList args = probeArgs(m_input);
    // Network inputs: give up after 15 s without data.
    if (m_input.contains(QStringLiteral("://"))) {
        args.insert(args.size() - 2, QStringLiteral("-rw_timeout"));
        args.insert(args.size() - 2, QStringLiteral("15000000"));
    }
    m_job = new ProcessJob(FfTools::paths().ffprobe, args, this);
    m_job->setLog(m_log, QStringLiteral("probe"));
    connect(m_job, &ProcessJob::finished, this, [this](bool ok) {
        ProbeResult result;
        result.path = m_input;
        QString error;
        // ffprobe says it guessed a layout for every WAV without a channel
        // mask; that's information, not a problem with the file.
        QStringList problems;
        for (const QString &p : m_job->problems(QStringLiteral("warning")))
            if (!LogLine::benign(p))
                problems << p;
        const QStringList errors = m_job->problems(QStringLiteral("error"));
        bool parsed = parseProbe(m_job->readStdout(), &result, &error);
        if (!ok || !parsed) {
            error = errors.isEmpty() ? (error.isEmpty() ? QStringLiteral("ffprobe couldn't read it") : error) : errors.last();
            // "junk.wav: Invalid data found when processing input" -> drop the path.
            const qsizetype colon = error.indexOf(QStringLiteral(": "));
            if (colon > 0 && m_input.endsWith(error.left(colon)))
                error = error.mid(colon + 2);
            emit finished(false, result, error, problems);
        } else if (result.tracks.isEmpty()) {
            emit finished(false, result, tr("No audio tracks"), problems);
        } else {
            emit finished(true, result, {}, problems);
        }
        m_job->deleteLater();
        m_job = nullptr;
    });
    m_job->start();
}

} // namespace ffgui
