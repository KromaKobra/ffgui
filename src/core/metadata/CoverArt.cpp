#include "CoverArt.h"

#include <log/LogStore.h>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>

#include <memory>

namespace ffgui {

CoverArt::CoverArt(const QString &input, int streamIndex, LogStore *log, QObject *parent)
    : QObject(parent)
    , m_input(input)
    , m_stream(streamIndex)
    , m_log(log)
{
}

void CoverArt::start()
{
    // Re-encoding to PNG gives QImage something it always reads.
    const QStringList args { QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-loglevel"),
                             QStringLiteral("level+error"), QStringLiteral("-i"), m_input, QStringLiteral("-map"),
                             QStringLiteral("0:%1").arg(m_stream), QStringLiteral("-frames:v"), QStringLiteral("1"),
                             QStringLiteral("-c:v"), QStringLiteral("png"), QStringLiteral("-f"), QStringLiteral("image2pipe"),
                             QStringLiteral("pipe:1") };
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, args, this);
    job->setLog(m_log, QStringLiteral("cover"), QStringLiteral("error"));
    auto bytes = std::make_shared<QByteArray>();
    connect(job, &ProcessJob::stdoutReady, this, [job, bytes] { *bytes += job->readStdout(); });
    connect(job, &ProcessJob::finished, this, [this, job, bytes](bool ok) {
        *bytes += job->readStdout();
        job->deleteLater();
        QImage image;
        ok = ok && image.loadFromData(*bytes);
        emit finished(ok, image);
    });
    job->start();
}

} // namespace ffgui
