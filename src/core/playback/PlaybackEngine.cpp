#include "PlaybackEngine.h"

#include <log/LogStore.h>
#include <media/PcmReader.h>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>
#include <tools/ShellQuote.h>

#include <QAudioDevice>
#include <QAudioSink>
#include <QFileInfo>
#include <QMediaDevices>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace ffgui {

namespace {
constexpr int kMaxChannels = 32;
constexpr qsizetype kChunkFrames = 2048;
}

PlaybackEngine::PlaybackEngine(LogStore *log, QObject *parent)
    : QObject(parent)
    , m_log(log)
{
    m_poll.setInterval(30);
    connect(&m_poll, &QTimer::timeout, this, &PlaybackEngine::poll);
}

PlaybackEngine::~PlaybackEngine()
{
    stop();
}

bool PlaybackEngine::hasOutputDevice()
{
    return !QMediaDevices::defaultAudioOutput().isNull();
}

void PlaybackEngine::setSource(const Source &source)
{
    stop();
    m_source = source;
    m_source.channels = std::clamp(source.channels, 1, kMaxChannels);
    std::vector<std::atomic<float>> gains(size_t(m_source.channels));
    for (auto &g : gains)
        g.store(1.0f);
    m_gains.swap(gains);
}

void PlaybackEngine::setChannelGains(const std::vector<float> &gains)
{
    for (size_t c = 0; c < m_gains.size(); ++c)
        m_gains[c].store(c < gains.size() ? gains[c] : 1.0f);
}

void PlaybackEngine::setSpeed(double speed)
{
    speed = std::clamp(speed, 0.25, 4.0);
    if (speed == m_speed)
        return;
    const bool wasPlaying = m_playing;
    const double at = position();
    m_speed = speed;
    if (wasPlaying)
        play(at);
}

double PlaybackEngine::position() const
{
    const int rate = m_format.sampleRate() > 0 ? m_format.sampleRate() : m_source.sampleRate;
    return m_startPosition + double(m_consumed.load()) / rate * m_speed;
}

void PlaybackEngine::buildMatrix(int outChannels)
{
    const int in = m_source.channels;
    m_matrix.assign(size_t(outChannels) * in, 0.0f);
    auto set = [&](int o, int i, float v) { m_matrix[size_t(o) * in + i] = v; };
    if (in == outChannels) {
        for (int c = 0; c < in; ++c)
            set(c, c, 1);
    } else if (in == 1) {
        for (int o = 0; o < std::min(outChannels, 2); ++o)
            set(o, 0, 1);
    } else if (outChannels == 1) {
        for (int i = 0; i < in; ++i)
            set(0, i, 1.0f / in);
    } else if (outChannels >= 2) {
        // Fold everything into left and right by channel name.
        static const QStringList left { "FL", "FLC", "BL", "SL", "TFL", "TBL", "WL", "BIL" };
        static const QStringList right { "FR", "FRC", "BR", "SR", "TFR", "TBR", "WR", "BIR" };
        float sumL = 0, sumR = 0;
        for (int i = 0; i < in; ++i) {
            const QString name = i < m_source.channelNames.size() ? m_source.channelNames[i] : QString();
            float l = 0.5f, r = 0.5f;
            if (left.contains(name)) { l = 1; r = 0; }
            else if (right.contains(name)) { l = 0; r = 1; }
            else if (name.startsWith(QStringLiteral("LFE"))) { l = 0; r = 0; }
            else if (name.endsWith(u'C') || name == u"FC") { l = 0.7071f; r = 0.7071f; }
            set(0, i, l);
            set(1, i, r);
            sumL += l;
            sumR += r;
        }
        const float norm = 1.0f / std::max({ 1.0f, sumL, sumR });
        for (float &v : m_matrix)
            v *= norm;
    }
}

bool PlaybackEngine::play(double position)
{
    stop();
    m_error.clear();
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull() || m_source.input.isEmpty()) {
        m_error = device.isNull() ? tr("No audio output device") : tr("Nothing to play");
        emit error(m_error);
        return false;
    }

    QAudioFormat format = device.preferredFormat();
    format.setSampleFormat(QAudioFormat::Float);
    QAudioFormat wanted = format;
    wanted.setSampleRate(m_source.sampleRate);
    if (device.isFormatSupported(wanted))
        format = wanted;
    wanted = format;
    wanted.setChannelCount(m_source.channels);
    wanted.setChannelConfig(QAudioFormat::defaultChannelConfigForChannelCount(m_source.channels));
    if (device.isFormatSupported(wanted))
        format = wanted;
    m_format = format;
    m_outChannels = std::max(1, format.channelCount());
    buildMatrix(m_outChannels);

    const int rate = format.sampleRate();
    m_ring = std::make_unique<RingBuffer>(size_t(rate) * m_source.channels / 2);
    m_tap = std::make_unique<RingBuffer>(size_t(rate) * m_source.channels / 2);
    m_frame.assign(size_t(kChunkFrames) * m_source.channels, 0.0f);
    m_consumed = 0;
    m_ended = false;
    m_startPosition = std::max(0.0, position);
    startDecoder(m_startPosition);

    m_sink = std::make_unique<QAudioSink>(device, format);
    m_sink->start([this](QSpan<float> out) { render(out.data(), out.size()); });
    if (m_sink->error() != QtAudio::NoError) {
        m_error = tr("The audio output couldn't be opened");
        stop();
        emit error(m_error);
        return false;
    }
    m_playing = true;
    m_poll.start();
    emit playingChanged(true);
    return true;
}

void PlaybackEngine::stop()
{
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    stopDecoder();
    m_poll.stop();
    if (m_playing) {
        m_startPosition = position();
        m_consumed = 0;
        m_playing = false;
        emit playingChanged(false);
    }
}

void PlaybackEngine::startDecoder(double position)
{
    m_stopDecoder = false;
    m_decoderDone = false;
    PcmReader::Source s { m_source.input, m_source.audioIndex, m_source.channels, m_format.sampleRate(), 0 };
    QStringList args = PcmReader::decodeArgs(s, position, 0);
    if (m_speed != 1.0) {
        // atempo takes 0.5..2 per instance.
        QStringList tempo;
        double left = m_speed;
        while (left > 2.0) { tempo << QStringLiteral("atempo=2"); left /= 2.0; }
        while (left < 0.5) { tempo << QStringLiteral("atempo=0.5"); left /= 0.5; }
        tempo << QStringLiteral("atempo=%1").arg(left, 0, 'f', 4);
        args.insert(args.indexOf(QStringLiteral("-c:a")), QStringLiteral("-af"));
        args.insert(args.indexOf(QStringLiteral("-c:a")), tempo.join(u','));
    }
    RingBuffer *ring = m_ring.get();
    const int channels = m_source.channels;
    LogStore *log = m_log;
    m_decoder = QThread::create([args, ring, channels, log, stopFlag = &m_stopDecoder, done = &m_decoderDone] {
        ProcessJob job(FfTools::paths().ffmpeg, args);
        job.setLog(log, QStringLiteral("playback"), QStringLiteral("error"));
        job.start();
        const qint64 frameBytes = qint64(sizeof(float)) * channels;
        QByteArray pending;
        while (!*stopFlag) {
            if (job.stdoutAvailable() == 0 && pending.size() < frameBytes) {
                if (!job.waitForStdout(50) && !job.running() && job.stdoutAvailable() == 0)
                    break;
            }
            const qint64 room = qint64(ring->writable()) * qint64(sizeof(float)) - pending.size();
            if (room < frameBytes) {
                QThread::msleep(5);
                continue;
            }
            pending += job.readStdout(std::min(room, job.stdoutAvailable()));
            const qint64 frames = pending.size() / frameBytes;
            if (frames > 0) {
                const size_t written = ring->write(reinterpret_cast<const float *>(pending.constData()), size_t(frames) * channels);
                pending.remove(0, qsizetype(written * sizeof(float)));
            }
        }
        if (*stopFlag)
            job.kill();
        job.waitForFinished(2000);
        *done = true;
    });
    m_decoder->start();
}

void PlaybackEngine::stopDecoder()
{
    if (!m_decoder)
        return;
    m_stopDecoder = true;
    m_decoder->wait(5000);
    delete m_decoder;
    m_decoder = nullptr;
}

void PlaybackEngine::render(float *out, qsizetype samples)
{
    // The audio thread: no allocation, no locks.
    const int in = m_source.channels, outCh = m_outChannels;
    const qsizetype frames = samples / outCh;
    float gains[kMaxChannels];
    for (int c = 0; c < in; ++c)
        gains[c] = m_gains[size_t(c)].load(std::memory_order_relaxed);
    const float volume = m_volume.load(std::memory_order_relaxed);
    float *scratch = m_frame.data();

    qsizetype done = 0;
    while (done < frames) {
        const qsizetype n = std::min(frames - done, kChunkFrames);
        const qsizetype got = qsizetype(m_ring->read(scratch, size_t(n) * in)) / in;
        for (qsizetype f = 0; f < got; ++f)
            for (int c = 0; c < in; ++c)
                scratch[f * in + c] *= gains[c];
        m_tap->write(scratch, size_t(got) * in);
        float *o = out + done * outCh;
        for (qsizetype f = 0; f < got; ++f) {
            const float *x = scratch + f * in;
            for (int oc = 0; oc < outCh; ++oc) {
                const float *row = m_matrix.data() + size_t(oc) * in;
                float acc = 0;
                for (int c = 0; c < in; ++c)
                    acc += row[c] * x[c];
                o[f * outCh + oc] = acc * volume;
            }
        }
        std::fill(o + got * outCh, o + n * outCh, 0.0f);
        m_consumed.fetch_add(got, std::memory_order_relaxed);
        if (got < n && m_decoderDone.load() && m_ring->readable() == 0)
            m_ended = true;
        done += n;
    }
}

qint64 PlaybackEngine::takeTap(float *out, qint64 maxFrames)
{
    if (!m_tap)
        return 0;
    const int in = m_source.channels;
    return qint64(m_tap->read(out, size_t(maxFrames) * in)) / in;
}

void PlaybackEngine::poll()
{
    if (m_sink && m_sink->error() != QtAudio::NoError) {
        m_error = tr("Audio output error");
        stop();
        emit error(m_error);
        return;
    }
    if (m_ended.exchange(false)) {
        stop();
        emit ended();
    }
}

bool launchFfplay(const QString &input, double position, const QString &title, LogStore *log, QString *error)
{
    const QString ffplay = FfTools::paths().ffplay;
    if (ffplay.isEmpty()) {
        *error = QObject::tr("ffplay was not found");
        return false;
    }
    const QStringList args { QStringLiteral("-hide_banner"), QStringLiteral("-autoexit"),
                             QStringLiteral("-loglevel"), QStringLiteral("error"),
                             QStringLiteral("-ss"), QString::number(std::max(0.0, position), 'f', 3),
                             QStringLiteral("-window_title"), title, QStringLiteral("-i"), input };
    if (log)
        log->newJob(QStringLiteral("ffplay"), shellJoin(QStringList { QStringLiteral("ffplay") } + args));
    if (!QProcess::startDetached(ffplay, args)) {
        *error = QObject::tr("ffplay didn't start");
        return false;
    }
    return true;
}

} // namespace ffgui
