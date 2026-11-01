#pragma once

#include "RingBuffer.h"

#include <QAudioFormat>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

class QAudioSink;
class QThread;

// Plays an input through the default audio output: ffmpeg decodes from the
// playhead into a lock-free ring, and Qt's real-time audio callback pulls
// from it. Per-channel gains (solo) and the preview volume are applied in the
// callback, so neither restarts the decoder; the frames actually played are
// copied to a tap for the meters.
namespace ffgui {

class LogStore;

class PlaybackEngine : public QObject
{
    Q_OBJECT

public:
    struct Source
    {
        QString input;
        int audioIndex = 0;
        int channels = 2;
        int sampleRate = 48000;
        QStringList channelNames;
        double duration = 0;
    };

    explicit PlaybackEngine(LogStore *log = nullptr, QObject *parent = nullptr);
    ~PlaybackEngine() override;

    void setSource(const Source &source);
    const Source &source() const { return m_source; }

    // Starts playing from `position` seconds.
    bool play(double position);
    void stop();
    bool playing() const { return m_playing; }
    double position() const;

    void setVolume(double volume) { m_volume.store(float(volume)); }
    void setSpeed(double speed);
    // 1 per input channel to play, 0 to mute (solo).
    void setChannelGains(const std::vector<float> &gains);

    // Frames the device has taken (input channel layout, after solo gains),
    // for the meters. Returns the number of frames copied.
    qint64 takeTap(float *out, qint64 maxFrames);
    int tapChannels() const { return m_source.channels; }
    qint64 framesPlayed() const { return m_consumed.load(); }
    // The format the device was opened with.
    QAudioFormat outputFormat() const { return m_format; }
    QString lastError() const { return m_error; }

    static bool hasOutputDevice();

signals:
    void playingChanged(bool playing);
    void ended();
    void error(const QString &message);

private:
    void startDecoder(double position);
    void stopDecoder();
    void render(float *out, qsizetype samples);
    void buildMatrix(int outChannels);
    void poll();

    QPointer<LogStore> m_log;
    Source m_source;
    QAudioFormat m_format;
    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<RingBuffer> m_ring;
    std::unique_ptr<RingBuffer> m_tap;
    QThread *m_decoder = nullptr;
    std::atomic_bool m_stopDecoder { false };
    std::atomic_bool m_decoderDone { false };
    std::atomic_bool m_ended { false };
    std::atomic<qint64> m_consumed { 0 };
    std::atomic<float> m_volume { 0.8f };
    std::vector<std::atomic<float>> m_gains;
    std::vector<float> m_matrix;          // [out * inChannels + in]
    std::vector<float> m_frame;           // scratch, audio thread only
    int m_outChannels = 2;
    double m_startPosition = 0;
    double m_speed = 1;
    bool m_playing = false;
    QString m_error;
    QTimer m_poll;
};

// Opens an input in real ffplay, for reference listening.
bool launchFfplay(const QString &input, double position, const QString &title, LogStore *log, QString *error);

} // namespace ffgui
