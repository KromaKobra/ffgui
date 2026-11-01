#pragma once

#include <QtGlobal>

#include <complex>
#include <cstdint>
#include <memory>
#include <vector>

// A spectrogram built as samples stream in: 2048-point Hann-windowed FFT
// columns, levels as 8-bit dB (0 = −120 dB, 255 = 0 dBFS for a full-scale
// sine). The hop grows with the input's length so a whole file stays around
// 16k columns per channel.
namespace ffgui {

class Fft;

class Spectrogram
{
public:
    static constexpr int kSize = 2048;
    static constexpr int kBins = kSize / 2;
    static constexpr double kFloorDb = -120.0;
    static constexpr int kTargetColumns = 16384;

    Spectrogram();
    ~Spectrogram();

    void reset(int channels, int sampleRate, qint64 expectedFrames);
    void append(const float *interleaved, qint64 frames);

    int channels() const { return m_channels; }
    int sampleRate() const { return m_rate; }
    int hop() const { return m_hop; }
    int columns() const { return m_columns; }
    // Frame at the centre of a column.
    double columnFrame(int column) const { return double(column) * m_hop + kSize / 2.0; }
    const uint8_t *column(int channel, int column) const
    {
        return m_data[channel].data() + size_t(column) * kBins;
    }

    static uint8_t toLevel(double db);
    static double toDb(uint8_t level) { return kFloorDb + level * (-kFloorDb / 255.0); }

private:
    void computeColumn();

    int m_channels = 0;
    int m_rate = 48000;
    int m_hop = 1024;
    int m_columns = 0;
    qint64 m_frames = 0;
    qint64 m_nextEnd = kSize;            // frame count at which the next column is due
    std::unique_ptr<Fft> m_fft;
    std::vector<float> m_window;
    std::vector<std::vector<float>> m_history;   // last kSize samples per channel (ring)
    int m_historyPos = 0;
    std::vector<std::vector<uint8_t>> m_data;    // [channel][column * kBins + bin]
    std::vector<std::complex<float>> m_scratch;
};

} // namespace ffgui
