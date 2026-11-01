#include "MediaData.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace ffgui {

namespace {
constexpr float kClipLevel = 0.999f;          // −0.009 dBFS
constexpr double kClipMerge = 0.05;           // clips closer than this are one mark
constexpr int kMaxClipMarks = 20000;
}

MediaData::MediaData(int channels, int sampleRate, qint64 expectedFrames)
    : m_channels(std::max(1, channels))
    , m_rate(sampleRate > 0 ? sampleRate : 48000)
    , m_expected(std::max<qint64>(0, expectedFrames))
{
    m_peaks.reset(m_channels, m_expected);
    m_spectrogram.reset(m_channels, m_rate, m_expected);
}

void MediaData::append(const float *s, qint64 frames)
{
    std::unique_lock lock(m_lock);
    const qint64 start = m_peaks.frames();
    for (qint64 f = 0; f < frames; ++f) {
        const float *frame = s + f * m_channels;
        bool clipped = false;
        for (int c = 0; c < m_channels; ++c) {
            if (std::abs(frame[c]) >= kClipLevel) {
                ++m_clipSamples;
                clipped = true;
            }
        }
        if (clipped) {
            const double t = double(start + f) / m_rate;
            if (t - m_lastClip > kClipMerge && m_clipTimes.size() < kMaxClipMarks)
                m_clipTimes << t;
            m_lastClip = t;
        }
    }
    m_peaks.append(s, frames);
    m_spectrogram.append(s, frames);
}

void MediaData::finish(bool complete)
{
    std::unique_lock lock(m_lock);
    m_peaks.finish();
    m_finished = true;
    m_complete = complete;
}

void MediaData::setWindow(qint64 firstFrame, std::vector<float> interleaved)
{
    std::unique_lock lock(m_lock);
    m_windowFirst = firstFrame;
    m_window = std::move(interleaved);
}

qint64 MediaData::frames() const
{
    std::shared_lock lock(m_lock);
    return m_peaks.frames();
}

bool MediaData::finished() const
{
    std::shared_lock lock(m_lock);
    return m_finished;
}

bool MediaData::complete() const
{
    std::shared_lock lock(m_lock);
    return m_complete;
}

bool MediaData::windowCovers(double t0, double t1) const
{
    std::shared_lock lock(m_lock);
    const qint64 n = qint64(m_window.size()) / m_channels;
    return n > 0 && qint64(std::floor(t0 * m_rate)) >= m_windowFirst && qint64(std::ceil(t1 * m_rate)) <= m_windowFirst + n;
}

void MediaData::peakColumns(int channel, double t0, double t1, int count, PeakColumn *out) const
{
    std::shared_lock lock(m_lock);
    const double f0 = t0 * m_rate, f1 = t1 * m_rate;
    const qint64 n = qint64(m_window.size()) / m_channels;
    const double step = (f1 - f0) / std::max(1, count);
    // Zoomed in past the pyramid's base block: use the window's samples.
    if (step < PeakPyramid::kBaseBlock && n > 0 && f0 >= m_windowFirst && f1 <= m_windowFirst + n) {
        const int c0 = channel < 0 ? 0 : channel, c1 = channel < 0 ? m_channels : channel + 1;
        for (int i = 0; i < count; ++i) {
            const qint64 a = qint64(std::floor(f0 + i * step)) - m_windowFirst;
            const qint64 b = std::max(a + 1, qint64(std::floor(f0 + (i + 1) * step)) - m_windowFirst);
            PeakColumn col;
            float mn = 1e9f, mx = -1e9f;
            double sq = 0;
            qint64 used = 0;
            for (qint64 k = std::max<qint64>(0, a); k < std::min(b, n); ++k) {
                for (int c = c0; c < c1; ++c) {
                    const float v = m_window[size_t(k) * m_channels + c];
                    mn = std::min(mn, v);
                    mx = std::max(mx, v);
                    sq += v * v;
                    ++used;
                }
            }
            if (used > 0) {
                col.min = mn;
                col.max = mx;
                col.rms = float(std::sqrt(sq / used));
                col.valid = true;
            }
            out[i] = col;
        }
        return;
    }
    m_peaks.columns(channel, f0, f1, count, out);
}

PeakColumn MediaData::peakRange(int channel, double t0, double t1) const
{
    std::shared_lock lock(m_lock);
    return m_peaks.range(channel, qint64(std::floor(t0 * m_rate)), qint64(std::ceil(t1 * m_rate)));
}

bool MediaData::windowSamples(int channel, double t0, double t1, std::vector<float> *out, qint64 *firstFrame) const
{
    std::shared_lock lock(m_lock);
    const qint64 n = qint64(m_window.size()) / m_channels;
    const qint64 a = qint64(std::floor(t0 * m_rate)), b = qint64(std::ceil(t1 * m_rate));
    if (n == 0 || a < m_windowFirst || b > m_windowFirst + n || channel >= m_channels)
        return false;
    out->clear();
    out->reserve(size_t(b - a + 1));
    for (qint64 k = a - m_windowFirst; k <= std::min(b - m_windowFirst, n - 1); ++k) {
        if (channel >= 0) {
            out->push_back(m_window[size_t(k) * m_channels + channel]);
        } else {
            float sum = 0;
            for (int c = 0; c < m_channels; ++c)
                sum += m_window[size_t(k) * m_channels + c];
            out->push_back(sum / m_channels);
        }
    }
    *firstFrame = a;
    return true;
}

double MediaData::spectrumDb(int channel, double t, double hz) const
{
    std::shared_lock lock(m_lock);
    const int cols = m_spectrogram.columns();
    if (cols == 0)
        return NAN;
    const int col = std::clamp(int(std::lround((t * m_rate - Spectrogram::kSize / 2.0) / m_spectrogram.hop())), 0, cols - 1);
    const int bin = std::clamp(int(std::lround(hz / (m_rate / 2.0) * Spectrogram::kBins)), 0, Spectrogram::kBins - 1);
    uint8_t v = 0;
    const int c0 = channel < 0 ? 0 : channel, c1 = channel < 0 ? m_channels : channel + 1;
    for (int c = c0; c < c1; ++c)
        v = std::max(v, m_spectrogram.column(c, col)[bin]);
    return Spectrogram::toDb(v);
}

void MediaData::renderSpectrogram(int channel, double t0, double t1, double fMin, double fMax, bool logScale,
                                  const QRgb *lut, QImage *image) const
{
    const int w = image->width(), h = image->height();
    if (w <= 0 || h <= 0)
        return;
    std::shared_lock lock(m_lock);
    const int cols = m_spectrogram.columns();
    const double nyquist = m_rate / 2.0;
    fMax = std::clamp(fMax > 0 ? fMax : nyquist, 1.0, nyquist);
    fMin = std::clamp(fMin, 1.0, fMax * 0.999);
    const double binHz = nyquist / Spectrogram::kBins;

    // Bin range for each row (top row = fMax).
    std::vector<int> rowLo(h), rowHi(h);
    for (int y = 0; y < h; ++y) {
        auto freqAt = [&](double fy) {
            const double u = 1.0 - fy / h;
            return logScale ? fMin * std::pow(fMax / fMin, u) : fMin + (fMax - fMin) * u;
        };
        const double fHi = freqAt(y), fLo = freqAt(y + 1);
        int lo = int(std::floor(fLo / binHz)), hi = int(std::ceil(fHi / binHz));
        lo = std::clamp(lo, 0, Spectrogram::kBins - 1);
        hi = std::clamp(std::max(hi, lo + 1), 1, Spectrogram::kBins);
        rowLo[y] = lo;
        rowHi[y] = hi;
    }

    const int c0 = channel < 0 ? 0 : std::min(channel, m_channels - 1), c1 = channel < 0 ? m_channels : c0 + 1;
    const double hop = m_spectrogram.hop();
    for (int x = 0; x < w; ++x) {
        const double ta = t0 + (t1 - t0) * x / w, tb = t0 + (t1 - t0) * (x + 1) / w;
        int ca = int(std::floor((ta * m_rate - Spectrogram::kSize / 2.0) / hop + 0.5));
        int cb = int(std::floor((tb * m_rate - Spectrogram::kSize / 2.0) / hop + 0.5));
        cb = std::max(cb, ca + 1);
        const bool inside = cols > 0 && ca < cols && cb > 0 && ta * m_rate < m_peaks.frames();
        ca = std::clamp(ca, 0, std::max(0, cols - 1));
        cb = std::clamp(cb, ca + 1, std::max(1, cols));
        // Very zoomed out: sample a few columns per pixel rather than all.
        const int stride = std::max(1, (cb - ca) / 4);
        for (int y = 0; y < h; ++y) {
            uint8_t v = 0;
            if (inside) {
                for (int c = c0; c < c1; ++c)
                    for (int col = ca; col < cb; col += stride) {
                        const uint8_t *data = m_spectrogram.column(c, col);
                        for (int b = rowLo[y]; b < rowHi[y]; ++b)
                            v = std::max(v, data[b]);
                    }
            }
            reinterpret_cast<QRgb *>(image->scanLine(y))[x] = inside ? lut[v] : qRgba(0, 0, 0, 0);
        }
    }
}

qint64 MediaData::clipSamples() const
{
    std::shared_lock lock(m_lock);
    return m_clipSamples;
}

QList<double> MediaData::clipTimes() const
{
    std::shared_lock lock(m_lock);
    return m_clipTimes;
}

} // namespace ffgui
