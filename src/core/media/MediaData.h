#pragma once

#include "PeakPyramid.h"
#include "Spectrogram.h"

#include <QImage>
#include <QList>

#include <shared_mutex>
#include <vector>

// Everything ffgui knows about one input's decoded audio: the peak pyramid,
// the spectrogram, where it clips, and (for deep zoom) the raw samples of
// the visible window. Filled from a decoder thread while the UI reads it, so
// every access takes the lock.
namespace ffgui {

class MediaData
{
public:
    MediaData(int channels, int sampleRate, qint64 expectedFrames);

    // ----- Writer (the decoder thread)
    void append(const float *interleaved, qint64 frames);
    void finish(bool complete);
    void setWindow(qint64 firstFrame, std::vector<float> interleaved);

    // ----- Readers
    int channels() const { return m_channels; }
    int sampleRate() const { return m_rate; }
    qint64 expectedFrames() const { return m_expected; }
    qint64 frames() const;
    bool finished() const;
    bool complete() const;

    // `count` pixel columns over [t0, t1) seconds. channel -1 = all channels.
    // Uses the window samples when they cover the range.
    void peakColumns(int channel, double t0, double t1, int count, PeakColumn *out) const;
    PeakColumn peakRange(int channel, double t0, double t1) const;
    // Samples of one channel over [t0, t1) when the window covers it.
    bool windowSamples(int channel, double t0, double t1, std::vector<float> *out, qint64 *firstFrame) const;
    bool windowCovers(double t0, double t1) const;

    // Level in dB of the spectrogram at (t, frequency); NaN outside it.
    double spectrumDb(int channel, double t, double hz) const;
    // Paints [t0, t1) × [fMin, fMax] into `image` (its size sets the
    // resolution), mapping 8-bit levels through `lut`.
    void renderSpectrogram(int channel, double t0, double t1, double fMin, double fMax, bool logScale,
                           const QRgb *lut, QImage *image) const;

    qint64 clipSamples() const;
    QList<double> clipTimes() const;

private:
    mutable std::shared_mutex m_lock;
    const int m_channels;
    const int m_rate;
    const qint64 m_expected;
    PeakPyramid m_peaks;
    Spectrogram m_spectrogram;
    bool m_finished = false;
    bool m_complete = false;
    qint64 m_clipSamples = 0;
    QList<double> m_clipTimes;
    double m_lastClip = -1;
    qint64 m_windowFirst = 0;
    std::vector<float> m_window;
};

} // namespace ffgui
