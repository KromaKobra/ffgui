#include "Spectrogram.h"
#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ffgui {

Spectrogram::Spectrogram() = default;
Spectrogram::~Spectrogram() = default;

uint8_t Spectrogram::toLevel(double db)
{
    const double v = (db - kFloorDb) / -kFloorDb * 255.0;
    return uint8_t(std::clamp(std::lround(v), 0L, 255L));
}

void Spectrogram::reset(int channels, int sampleRate, qint64 expectedFrames)
{
    m_channels = std::max(1, channels);
    m_rate = sampleRate > 0 ? sampleRate : 48000;
    m_hop = expectedFrames > 0 ? int(std::max<qint64>(256, (expectedFrames + kTargetColumns - 1) / kTargetColumns)) : 1024;
    m_columns = 0;
    m_frames = 0;
    m_nextEnd = kSize;
    if (!m_fft)
        m_fft = std::make_unique<Fft>(kSize);
    m_window.resize(kSize);
    for (int i = 0; i < kSize; ++i)
        m_window[i] = float(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / (kSize - 1)));
    m_history.assign(m_channels, std::vector<float>(kSize, 0.0f));
    m_historyPos = 0;
    m_data.assign(m_channels, {});
    const size_t expectedColumns = expectedFrames > 0 ? size_t(expectedFrames / m_hop + 2) : 0;
    for (auto &d : m_data)
        d.reserve(expectedColumns * kBins);
    m_scratch.resize(kSize);
}

void Spectrogram::append(const float *s, qint64 frames)
{
    qint64 done = 0;
    while (done < frames) {
        // Up to the next column boundary, or the end of the ring.
        const qint64 n = std::min({ frames - done, m_nextEnd - m_frames, qint64(kSize - m_historyPos) });
        for (int c = 0; c < m_channels; ++c) {
            float *h = m_history[c].data() + m_historyPos;
            const float *src = s + done * m_channels + c;
            for (qint64 f = 0; f < n; ++f)
                h[f] = src[f * m_channels];
        }
        m_historyPos = int((m_historyPos + n) % kSize);
        m_frames += n;
        done += n;
        if (m_frames == m_nextEnd) {
            computeColumn();
            m_nextEnd += m_hop;
        }
    }
}

void Spectrogram::computeColumn()
{
    // A full-scale sine peaks at |X| = N/4 with a Hann window; work in power
    // to skip the square roots.
    const float scale = 4.0f / kSize;
    const float powerScale = scale * scale;
    const float floorPower = float(std::pow(10.0, kFloorDb / 10.0));
    for (int c = 0; c < m_channels; ++c) {
        const float *h = m_history[c].data();
        const int tail = kSize - m_historyPos;
        for (int i = 0; i < tail; ++i)
            m_scratch[i] = { h[m_historyPos + i] * m_window[i], 0.0f };
        for (int i = tail; i < kSize; ++i)
            m_scratch[i] = { h[i - tail] * m_window[i], 0.0f };
        m_fft->transform(m_scratch.data());
        std::vector<uint8_t> &out = m_data[c];
        const size_t base = out.size();
        out.resize(base + kBins);
        for (int k = 0; k < kBins; ++k) {
            const float power = std::norm(m_scratch[k]) * powerScale;
            out[base + k] = power > floorPower ? toLevel(10.0 * std::log10(power)) : 0;
        }
    }
    ++m_columns;
}

} // namespace ffgui
