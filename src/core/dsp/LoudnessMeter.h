#pragma once

#include <QStringList>

#include <cmath>
#include <deque>
#include <vector>

// ITU-R BS.1770 / EBU R128 loudness: K-weighting, momentary (400 ms),
// short-term (3 s) and gated integrated loudness, and true peak from 4×
// oversampling. Feed it interleaved float samples.
namespace ffgui {

class LoudnessMeter
{
public:
    // Channel names (FL, FR, FC, LFE, SL, ...) set the BS.1770 weights.
    LoudnessMeter(int sampleRate, const QStringList &channelNames);

    void process(const float *interleaved, qint64 frames);
    void reset();

    double momentary() const;           // LUFS, -inf before 400 ms
    double shortTerm() const;           // LUFS, -inf before the first 100 ms block
    double integrated() const;          // LUFS, -inf until a block passes the gate
    double truePeak() const;            // dBTP, highest since reset
    double samplePeak() const;          // dBFS, highest since reset
    double takeTruePeak();              // dBTP since the last call (for live meters)

    static double toLufs(double energy) { return energy > 0 ? -0.691 + 10.0 * std::log10(energy) : -INFINITY; }

private:
    struct Biquad
    {
        double b0, b1, b2, a1, a2;
    };
    struct ChannelState
    {
        double z1[2] = { 0, 0 };
        double z2[2] = { 0, 0 };
        double weight = 1;
        std::vector<float> history;      // last taps for the true-peak filter
        int historyPos = 0;
    };

    void closeSubBlock();
    double meanOfLast(size_t count) const;

    int m_rate;
    int m_channels;
    Biquad m_stage[2];
    std::vector<ChannelState> m_state;
    std::vector<std::vector<float>> m_phases;    // true-peak polyphase FIR, [phase][tap]
    qint64 m_subBlockFrames;
    qint64 m_subBlockCount = 0;
    double m_subBlockEnergy = 0;
    std::deque<double> m_subBlocks;               // last 30 sub-block energies
    std::vector<double> m_gatingBlocks;           // every 400 ms block energy (100 ms hop)
    double m_truePeak = 0;
    double m_truePeakSince = 0;
    double m_samplePeak = 0;
};

// Pearson correlation of two channels over what was fed since the last take().
class Correlation
{
public:
    void process(const float *interleaved, qint64 frames, int channels, int left = 0, int right = 1);
    // −1..+1; `fallback` when there was nothing to measure.
    double take(double fallback = 0);

private:
    double m_lr = 0, m_ll = 0, m_rr = 0;
};

} // namespace ffgui
