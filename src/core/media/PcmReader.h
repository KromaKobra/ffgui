#pragma once

#include "MediaData.h"

#include <QObject>
#include <QPointer>

#include <atomic>
#include <memory>

class QThread;

// Decodes one audio track to 32-bit float PCM (an ffmpeg pipe) on its own
// thread and feeds a MediaData as the samples arrive, so the waveform and
// spectrogram fill in while the rest is still decoding.
namespace ffgui {

class LogStore;

class PcmReader : public QObject
{
    Q_OBJECT

public:
    struct Source
    {
        QString input;
        int audioIndex = 0;
        int channels = 2;
        int sampleRate = 48000;
        double duration = 0;    // seconds, for progress and preallocation; 0 = unknown
        QString filter;
    };

    PcmReader(const Source &source, LogStore *log = nullptr, QObject *parent = nullptr);
    ~PcmReader() override;

    std::shared_ptr<MediaData> data() const { return m_data; }
    const Source &source() const { return m_source; }
    void start();
    void cancel();
    bool running() const { return m_running; }

    static QStringList decodeArgs(const Source &source, double start = 0, double length = 0);

signals:
    void progress(double fraction);     // throttled, in the receiver's thread
    void finished(bool ok, const QString &error);

private:
    Source m_source;
    QPointer<LogStore> m_log;
    std::shared_ptr<MediaData> m_data;
    QThread *m_thread = nullptr;
    std::atomic_bool m_cancel { false };
    bool m_running = false;
};

// Decodes a short range of samples for sample-level zoom.
class WindowDecoder : public QObject
{
    Q_OBJECT

public:
    WindowDecoder(const PcmReader::Source &source, std::shared_ptr<MediaData> data, QObject *parent = nullptr);
    // Decodes [t0, t1); a newer request replaces one still running.
    void request(double t0, double t1);

signals:
    void ready();

private:
    PcmReader::Source m_source;
    std::weak_ptr<MediaData> m_data;
    QPointer<QObject> m_job;
    int m_generation = 0;
};

} // namespace ffgui
