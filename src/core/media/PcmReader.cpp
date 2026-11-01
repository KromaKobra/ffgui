#include "PcmReader.h"

#include <log/LogStore.h>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>

#include <QElapsedTimer>
#include <QThread>

#include <cstring>

namespace ffgui {

QStringList PcmReader::decodeArgs(const Source &s, double start, double length)
{
    QStringList args { QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                       QStringLiteral("-loglevel"), QStringLiteral("level+error") };
    if (start > 0 && !s.filter.isEmpty()) args << QStringLiteral("-copyts");
    if (start > 0)
        args << QStringLiteral("-ss") << QString::number(start, 'f', 6);
    if (length > 0)
        args << QStringLiteral("-t") << QString::number(length, 'f', 6);
    args << QStringLiteral("-i") << s.input
         << QStringLiteral("-map") << QStringLiteral("0:a:%1").arg(s.audioIndex)
         << QStringLiteral("-vn") << QStringLiteral("-sn") << QStringLiteral("-dn")
         << QStringLiteral("-ac") << QString::number(s.channels)
         << QStringLiteral("-ar") << QString::number(s.sampleRate)
         << QStringLiteral("-c:a") << QStringLiteral("pcm_f32le")
         << QStringLiteral("-f") << QStringLiteral("f32le") << QStringLiteral("pipe:1");
    if (!s.filter.isEmpty()) {
        const int pos = args.indexOf(QStringLiteral("-ac"));
        args.insert(pos, QStringLiteral("-af")); args.insert(pos+1, s.filter);
    }
    return args;
}

PcmReader::PcmReader(const Source &source, LogStore *log, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_log(log)
    , m_data(std::make_shared<MediaData>(source.channels, source.sampleRate,
                                         qint64(source.duration * source.sampleRate)))
{
}

PcmReader::~PcmReader()
{
    cancel();
    if (m_thread) {
        m_thread->wait(5000);
        delete m_thread;
    }
}

void PcmReader::cancel()
{
    m_cancel = true;
}

void PcmReader::start()
{
    if (m_thread)
        return;
    m_running = true;
    const Source source = m_source;
    const std::shared_ptr<MediaData> data = m_data;
    LogStore *log = m_log;
    QPointer<PcmReader> self(this);
    m_thread = QThread::create([self, source, data, log, cancel = &m_cancel] {
        ProcessJob job(FfTools::paths().ffmpeg, decodeArgs(source));
        job.setLog(log, QStringLiteral("decode"), QStringLiteral("error"));
        job.setBackground(true);
        job.start();
        const int frameBytes = int(sizeof(float)) * source.channels;
        const qint64 expected = qint64(source.duration * source.sampleRate);
        QByteArray pending;
        QElapsedTimer sinceReport;
        sinceReport.start();
        auto report = [&](double fraction) {
            QMetaObject::invokeMethod(self, [self, fraction] {
                if (self)
                    emit self->progress(fraction);
            }, Qt::QueuedConnection);
        };
        while (!*cancel) {
            const bool more = job.waitForStdout(100);
            QByteArray chunk = job.readStdout();
            if (!chunk.isEmpty()) {
                pending += chunk;
                const qint64 frames = pending.size() / frameBytes;
                if (frames > 0) {
                    data->append(reinterpret_cast<const float *>(pending.constData()), frames);
                    pending.remove(0, frames * frameBytes);
                }
                if (sinceReport.elapsed() > 120) {
                    sinceReport.restart();
                    report(expected > 0 ? std::min(1.0, double(data->frames()) / expected) : 0.0);
                }
            }
            if (!more && !job.running() && job.stdoutAvailable() == 0)
                break;
        }
        if (*cancel)
            job.kill();
        job.waitForFinished(3000);
        const bool ok = !*cancel && job.succeeded();
        data->finish(ok);
        const QStringList problems = job.problems(QStringLiteral("error"));
        const QString error = ok ? QString() : (*cancel ? QStringLiteral("cancelled") : problems.isEmpty() ? QStringLiteral("decoding failed") : problems.last());
        QMetaObject::invokeMethod(self, [self, ok, error] {
            if (!self)
                return;
            self->m_running = false;
            emit self->progress(1.0);
            emit self->finished(ok, error);
        }, Qt::QueuedConnection);
    });
    m_thread->start();
}

WindowDecoder::WindowDecoder(const PcmReader::Source &source, std::shared_ptr<MediaData> data, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_data(data)
{
}

void WindowDecoder::request(double t0, double t1)
{
    if (m_job)
        static_cast<ProcessJob *>(m_job.data())->kill();
    t0 = std::max(0.0, t0);
    if (t1 <= t0)
        return;
    const int generation = ++m_generation;
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, PcmReader::decodeArgs(m_source, t0, t1 - t0), this);
    m_job = job;
    auto buffer = std::make_shared<QByteArray>();
    connect(job, &ProcessJob::stdoutReady, this, [job, buffer] { *buffer += job->readStdout(); });
    connect(job, &ProcessJob::finished, this, [this, job, buffer, t0, generation](bool ok) {
        *buffer += job->readStdout();
        job->deleteLater();
        if (!ok || generation != m_generation)
            return;
        auto data = m_data.lock();
        if (!data)
            return;
        std::vector<float> samples(size_t(buffer->size()) / sizeof(float));
        std::memcpy(samples.data(), buffer->constData(), samples.size() * sizeof(float));
        data->setWindow(qint64(std::llround(t0 * m_source.sampleRate)), std::move(samples));
        emit ready();
    });
    job->start();
}

} // namespace ffgui
