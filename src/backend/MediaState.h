#pragma once

#include <media/PcmReader.h>

#include <QElapsedTimer>
#include <QQmlEngine>

#include <memory>

namespace ffgui { class LogStore; }

// The decoded audio of the input the Media View shows: progress while it
// decodes, readouts for the hover crosshair, and the data the waveform and
// spectrogram items draw. Recently shown inputs stay decoded.
class MediaState : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Media)
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(MediaState *processed READ processed CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int channels READ channels NOTIFY changed)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    // Bumped whenever more audio has been decoded or a zoom window arrived.
    Q_PROPERTY(int revision READ revision NOTIFY updated)

public:
    explicit MediaState(ffgui::LogStore *log, QObject *parent = nullptr, bool withProcessed = true);
    ~MediaState() override;

    MediaState *processed() const { return m_processed; }
    bool ready() const { return m_data != nullptr; }
    bool loading() const;
    bool live() const { return m_live; }
    QString error() const { return m_error; }
    int channels() const { return m_source.channels; }
    int sampleRate() const { return m_source.sampleRate; }
    double progress() const { return m_progress; }
    int revision() const { return m_revision; }

    // Peak level in dBFS around `t` (±halfWindow s); -inf for silence, NaN without data.
    Q_INVOKABLE double levelAt(int channel, double t, double halfWindow) const;
    // RMS level in dBFS around `t`.
    Q_INVOKABLE double rmsAt(int channel, double t, double halfWindow) const;
    // Spectrogram level in dB at (t, frequency).
    Q_INVOKABLE double spectrumAt(int channel, double t, double hz) const;
    // Decodes [t0, t1) at full resolution for sample-level zoom.
    Q_INVOKABLE void requestWindow(double t0, double t1);

    std::shared_ptr<ffgui::MediaData> data() const { return m_data; }
    QString key() const { return m_key; }

    // Shows (decoding if needed) one track. `key` identifies input + track.
    void show(const QString &key, const ffgui::PcmReader::Source &source);
    // Nothing to show; `live` for streams without a length.
    void clear(bool live = false);
    void forget(const QString &keyPrefix);

signals:
    void changed();
    void progressChanged();
    void updated();
    void decoded(const QString &key, bool ok);

private:
    struct Entry
    {
        std::unique_ptr<ffgui::PcmReader> reader;
        std::shared_ptr<ffgui::MediaData> data;
        bool done = false;
        bool ok = false;
        QString error;
    };
    void bump();

    MediaState *m_processed = nullptr;
    ffgui::LogStore *m_log;
    std::map<QString, Entry> m_cache;
    QStringList m_order;                // most recent last
    QString m_key;
    ffgui::PcmReader::Source m_source;
    std::shared_ptr<ffgui::MediaData> m_data;
    std::unique_ptr<ffgui::WindowDecoder> m_window;
    bool m_live = false;
    QString m_error;
    double m_progress = 0;
    int m_revision = 0;
    QElapsedTimer m_redraw;
};
