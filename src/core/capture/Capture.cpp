#include "Capture.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <memory>
#include <tools/FfTools.h>
namespace ffgui
{
DeviceList::DeviceList(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}
QList<QVariantMap> DeviceList::parse(const QString &text, const QString &backend)
{
    QList<QVariantMap> out;
    static const QRegularExpression row(
        QStringLiteral(R"(^\s*([* ])?\s*(\S+)\s+\[([^\]]+)\](?:\s*\(.*\))?\s*$)"));
    for (const auto &line : text.split(u'\n')) {
        const auto m = row.match(line);
        if (!m.hasMatch())
            continue;
        const QString id = m.captured(2);
        if (id.startsWith(u'[') || id == u"Auto-detected")
            continue;
        out << QVariantMap{{"id", id},
                           {"name", m.captured(3)},
                           {"backend", backend},
                           {"isDefault", m.captured(1) == u"*"},
                           {"isMonitor", id.contains("monitor", Qt::CaseInsensitive)}};
    }
    return out;
}
DeviceList::~DeviceList()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->kill();
        delete m_job;
    }
}
void DeviceList::refresh()
{
    if (m_job)
        return;
    m_rows.clear();
    next(0);
}
void DeviceList::next(int backend)
{
    const QString kind = backend == 0 ? QStringLiteral("pulse") : QStringLiteral("alsa");
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, {"-hide_banner", "-sources", kind}, this);
    m_job = job;
    job->setLog(m_log, QStringLiteral("devices"), QStringLiteral("error"));
    auto text = std::make_shared<QByteArray>();
    connect(job, &ProcessJob::stdoutReady, this, [job, text] { *text += job->readStdout(); });
    connect(job, &ProcessJob::stderrLine, this,
            [text](const LogLine &l) { *text += l.text.toUtf8() + '\n'; });
    connect(job, &ProcessJob::finished, this, [this, job, text, kind, backend](bool) {
        *text += job->readStdout();
        m_rows << parse(QString::fromUtf8(*text), kind);
        m_job = nullptr;
        job->deleteLater();
        if (backend == 0)
            next(1);
        else
            emit finished(m_rows, {});
    });
    job->start();
}
Recorder::Recorder(LogStore *log, QObject *parent) : QObject(parent), m_log(log)
{
    m_limit.setInterval(100);
    connect(&m_limit, &QTimer::timeout, this, [this] {
        if (m_spec.maxBytes <= 0)
            return;
        qint64 bytes = 0;
        const QFileInfo file(m_spec.output);
        QString pattern = file.fileName();
        pattern.replace(QRegularExpression("%0?\\d*d"), "*");
        const QDir dir(file.absolutePath());
        for (const auto &name : dir.entryList({pattern}, QDir::Files))
            if (!m_before.contains(name))
                bytes += QFileInfo(dir.filePath(name)).size();
        if (bytes >= m_spec.maxBytes)
            stop();
    });
}
Recorder::~Recorder()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->stop();
        m_job->waitForFinished(1500);
        delete m_job;
    }
}
QStringList Recorder::args(const CaptureSpec &s)
{
    QStringList a{"-hide_banner", "-loglevel", "level+info", "-n"};
    if (s.backend == u"lavfi")
        a << "-re";
    a << "-f" << s.backend << "-i" << s.device << "-map" << "0:a:0" << "-c:a" << s.codec << "-ar"
      << QString::number(s.rate) << "-ac" << QString::number(s.channels);
    if (s.codec == u"flac")
        a << "-sample_fmt" << "s32" << "-bits_per_raw_sample" << QString::number(s.depth);
    if (s.seconds > 0)
        a << "-t" << QString::number(s.seconds);
    if (s.maxBytes > 0)
        a << "-fs" << QString::number(s.maxBytes);
    if (s.splitSeconds > 0)
        a << "-f" << "segment" << "-segment_time" << QString::number(s.splitSeconds) << "-reset_timestamps"
          << "1";
    a << "-flush_packets" << "1" << s.output << "-map" << "0:a:0" << "-ar" << QString::number(s.rate) << "-ac"
      << QString::number(s.channels) << "-c:a" << "pcm_f32le";
    if (s.seconds > 0)
        a << "-t" << QString::number(s.seconds);
    a << "-f" << "f32le" << "pipe:1";
    return a;
}
void Recorder::start(const CaptureSpec &s)
{
    if (m_job)
        return;
    if (s.output.isEmpty() || s.rate < 8000 || s.channels < 1 || s.channels > 32 || s.seconds < 0 ||
        s.splitSeconds < 0) {
        emit finished(false, {}, QStringLiteral("Invalid recording settings"));
        return;
    }
    if (s.splitSeconds > 0) {
        const auto pattern = QRegularExpression("%0?\\d*d").match(s.output);
        if (!pattern.hasMatch()) {
            emit finished(false, {}, QStringLiteral("Split recordings need a %03d filename pattern"));
            return;
        }
        const QFileInfo file(s.output);
        QString glob = file.fileName();
        glob.replace(QRegularExpression("%0?\\d*d"), "*");
        if (!QDir(file.absolutePath()).entryList({glob}, QDir::Files).isEmpty()) {
            emit finished(false, {}, QStringLiteral("Choose a new recording segment pattern"));
            return;
        }
    } else if (QFileInfo::exists(s.output)) {
        emit finished(false, {}, QStringLiteral("Choose a new recording filename"));
        return;
    }
    m_spec = s;
    m_frames = 0;
    m_pending.clear();
    m_before = QDir(QFileInfo(s.output).absolutePath()).entryList(QDir::Files);
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, args(s), this);
    m_job = job;
    job->setLog(m_log, QStringLiteral("record"), QStringLiteral("info"));
    job->setKeepStderr(false);
    connect(job, &ProcessJob::stdoutReady, this, &Recorder::read);
    auto errors = std::make_shared<QStringList>();
    connect(job, &ProcessJob::stderrLine, this, [errors](const LogLine &l) {
        if (l.level == u"error" && errors->size() < 20)
            *errors << l.text;
    });
    connect(job, &ProcessJob::finished, this, [this, job, errors](bool ok) {
        read();
        m_limit.stop();
        m_job = nullptr;
        QStringList files;
        if (m_spec.splitSeconds <= 0) {
            if (QFileInfo::exists(m_spec.output))
                files << m_spec.output;
        } else {
            const QFileInfo pattern(m_spec.output);
            QString match = pattern.fileName();
            match.replace(QRegularExpression("%0?\\d*d"), "*");
            const QDir dir(pattern.absolutePath());
            for (const auto &name : dir.entryList({match}, QDir::Files, QDir::Name))
                if (!m_before.contains(name))
                    files << dir.absoluteFilePath(name);
        }
        job->deleteLater();
        emit finished(ok, files, errors->join(u'\n'));
    });
    job->start();
    m_limit.start();
}
void Recorder::read()
{
    if (!m_job)
        return;
    m_pending += m_job->readStdout();
    const qint64 samples = m_pending.size() / 4, frames = samples / m_spec.channels;
    double peak = 0;
    if (frames == 0)
        return;
    const auto *x = reinterpret_cast<const float *>(m_pending.constData());
    if (m_spec.monitor)
        for (qint64 i = 0; i < frames * m_spec.channels; ++i)
            peak = std::max(peak, double(std::abs(x[i])));
    m_frames += frames;
    m_pending.remove(0, frames * m_spec.channels * 4);
    emit reading(double(m_frames) / m_spec.rate, peak > 0 ? 20 * std::log10(peak) : -100);
}
void Recorder::stop()
{
    if (m_job)
        m_job->stop();
}
StreamOut::StreamOut(const StreamSpec &s, LogStore *log, QObject *parent)
    : QObject(parent), m_spec(s), m_log(log)
{
}
StreamOut::~StreamOut()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->stop();
        m_job->waitForFinished(1500);
        delete m_job;
    }
}
QStringList StreamOut::args(const StreamSpec &s, QString *error)
{
    auto fail = [&](const QString &text) {
        if (error)
            *error = text;
        return QStringList();
    };
    if (s.input.isEmpty() || s.url.isEmpty() || s.bitrate < 8 || s.bitrate > 1024)
        return fail(QStringLiteral("Choose a source, destination and valid bitrate"));
    const QUrl url(s.url);
    QString codec = "aac", format;
    if (s.protocol == u"rtmp") {
        if (url.scheme() != u"rtmp" && url.scheme() != u"rtmps")
            return fail(QStringLiteral("Use an RTMP URL"));
        format = "flv";
    } else if (s.protocol == u"icecast") {
        if (url.scheme() != u"icecast")
            return fail(QStringLiteral("Use an icecast:// URL"));
        format = "mp3";
        codec = "libmp3lame";
    } else if (s.protocol == u"srt") {
        if (url.scheme() != u"srt")
            return fail(QStringLiteral("Use an srt:// URL"));
        format = "mpegts";
    } else if (s.protocol == u"hls") {
        if (!url.scheme().isEmpty() && !url.isLocalFile())
            return fail(QStringLiteral("Choose a local HLS playlist"));
        format = "hls";
    } else
        return fail(QStringLiteral("Choose RTMP, Icecast, SRT or HLS"));
    QString target = s.protocol == u"hls" && url.isLocalFile() ? url.toLocalFile() : s.url;
    if (s.protocol == u"hls" &&
        (QFileInfo(target).absoluteFilePath() == QFileInfo(s.input).absoluteFilePath() ||
         QFileInfo::exists(target)))
        return fail(QStringLiteral("Choose a new HLS playlist path"));
    QStringList a{"-hide_banner", "-loglevel", "level+info", "-n"};
    if (!s.graph.isEmpty()) {
        // A graph plan compiled for one output: every source is read in real time.
        if (!s.graph.value("error").toString().isEmpty())
            return fail(s.graph.value("error").toString());
        const auto pads = s.graph.value("outputs").toMap();
        if (pads.isEmpty())
            return fail(QStringLiteral("Connect a graph output first"));
        for (const auto &arg : s.graph.value("inputs").toStringList()) {
            if (arg == u"-i")
                a << "-re";
            a << arg;
        }
        a << "-filter_complex" << s.graph.value("filter").toString() << "-map"
          << u'[' + pads.first().toString() + u']';
    } else {
        a << "-re" << "-i" << s.input << "-map" << QStringLiteral("0:a:%1").arg(s.audioIndex);
        if (!s.filter.isEmpty())
            a << "-af" << s.filter;
    }
    a << "-c:a" << codec << "-b:a" << QString::number(s.bitrate) + u'k' << "-progress" << "pipe:1"
      << "-nostats";
    if (s.protocol == u"icecast")
        a << "-content_type" << "audio/mpeg";
    if (s.protocol == u"hls")
        a << "-hls_time" << "2" << "-hls_list_size" << "6" << "-hls_segment_filename"
          << QFileInfo(target).absolutePath() + u'/' + QFileInfo(target).completeBaseName() + u'_' +
                 QUuid::createUuid().toString(QUuid::Id128).left(8) + "_%06d.ts";
    a << "-f" << format << target;
    return a;
}
void StreamOut::start()
{
    if (m_job)
        return;
    QString error;
    const auto a = args(m_spec, &error);
    if (!error.isEmpty()) {
        emit finished(false, false, error);
        return;
    }
    m_bytes = 0;
    m_time = 0;
    const int segment = a.indexOf("-hls_segment_filename");
    m_segmentPattern = segment >= 0 ? a[segment + 1] : QString();
    m_job = new ProcessJob(FfTools::paths().ffmpeg, a, this);
    m_job->setProgress(true);
    m_job->setLog(m_log, QStringLiteral("stream"), QStringLiteral("info"));
    m_job->setKeepStderr(false);
    auto errors = std::make_shared<QStringList>();
    connect(m_job, &ProcessJob::stderrLine, this, [errors](const LogLine &l) {
        if (l.level == u"error" && errors->size() < 20)
            *errors << l.text;
    });
    connect(m_job, &ProcessJob::progress, this, [this](const ProgressParser::Snapshot &s) {
        qint64 bytes = s.totalSize;
        if (!m_segmentPattern.isEmpty()) {
            const QFileInfo file(m_segmentPattern);
            QString glob = file.fileName();
            glob.replace(QRegularExpression("%0?\\d*d"), "*");
            bytes = 0;
            const QDir dir(file.absolutePath());
            for (const auto &name : dir.entryList({glob}, QDir::Files))
                bytes += QFileInfo(dir.filePath(name)).size();
        }
        const double dt = s.outTime - m_time;
        const double bitrate = dt > 0 && bytes >= m_bytes ? double(bytes - m_bytes) * 8 / dt : 0;
        m_bytes = bytes;
        m_time = s.outTime;
        emit reading(bitrate);
    });
    connect(m_job, &ProcessJob::finished, this, [this, errors](bool ok) {
        const bool stopped = m_job->stopped();
        m_job->deleteLater();
        m_job = nullptr;
        emit finished(ok, stopped, errors->join(u'\n'));
    });
    m_job->start();
}
} // namespace ffgui

namespace ffgui
{
void StreamOut::stop()
{
    if (m_job)
        m_job->stop();
}
} // namespace ffgui
