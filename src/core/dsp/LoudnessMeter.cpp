#include "LoudnessMeter.h"

#include <algorithm>
#include <numbers>
#include <numeric>

namespace ffgui {

namespace {

constexpr int kOversample = 4;
constexpr int kTapsPerPhase = 12;

double channelWeight(const QString &name)
{
    if (name == u"LFE" || name == u"LFE2")
        return 0;
    // Surround channels count 1.41× (+1.5 dB); front and centre 1.
    if (name == u"SL" || name == u"SR" || name == u"BL" || name == u"BR")
        return 1.41;
    return 1;
}

} // namespace

LoudnessMeter::LoudnessMeter(int sampleRate, const QStringList &channelNames)
    : m_rate(sampleRate > 0 ? sampleRate : 48000)
    , m_channels(std::max<int>(1, int(channelNames.size())))
    , m_subBlockFrames(std::max(1, m_rate / 10))
{
    // K-weighting for any sample rate: a high-shelf "pre-filter" and the RLB
    // high-pass (coefficients as in libebur128).
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan(std::numbers::pi * f0 / m_rate);
        const double Vh = std::pow(10.0, G / 20.0);
        const double Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        m_stage[0] = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                       2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan(std::numbers::pi * f0 / m_rate);
        const double a0 = 1.0 + K / Q + K * K;
        m_stage[1] = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
    }

    // True peak: a windowed-sinc low-pass at the original Nyquist, split into
    // four phases of 12 taps.
    const int taps = kOversample * kTapsPerPhase;
    std::vector<double> h(taps);
    for (int n = 0; n < taps; ++n) {
        const double x = n - (taps - 1) / 2.0;
        const double sinc = x == 0 ? 1.0 : std::sin(std::numbers::pi * x / kOversample) / (std::numbers::pi * x / kOversample);
        const double window = 0.42 - 0.5 * std::cos(2 * std::numbers::pi * n / (taps - 1)) + 0.08 * std::cos(4 * std::numbers::pi * n / (taps - 1));
        h[n] = sinc * window;
    }
    m_phases.assign(kOversample, std::vector<float>(kTapsPerPhase));
    for (int p = 0; p < kOversample; ++p) {
        double sum = 0;
        for (int t = 0; t < kTapsPerPhase; ++t)
            sum += h[t * kOversample + p];
        for (int t = 0; t < kTapsPerPhase; ++t)
            m_phases[p][t] = float(h[t * kOversample + p] / sum);   // unity gain per phase
    }

    m_state.resize(m_channels);
    for (int c = 0; c < m_channels; ++c) {
        m_state[c].weight = c < channelNames.size() ? channelWeight(channelNames[c]) : 1.0;
        m_state[c].history.assign(kTapsPerPhase, 0.0f);
    }
}

void LoudnessMeter::reset()
{
    for (ChannelState &s : m_state) {
        s.z1[0] = s.z1[1] = s.z2[0] = s.z2[1] = 0;
        std::fill(s.history.begin(), s.history.end(), 0.0f);
        s.historyPos = 0;
    }
    m_subBlockCount = 0;
    m_subBlockEnergy = 0;
    m_subBlocks.clear();
    m_gatingBlocks.clear();
    m_truePeak = m_truePeakSince = m_samplePeak = 0;
}

void LoudnessMeter::process(const float *s, qint64 frames)
{
    for (qint64 f = 0; f < frames; ++f) {
        for (int c = 0; c < m_channels; ++c) {
            ChannelState &st = m_state[c];
            const float x = s[f * m_channels + c];

            // Sample and true peak.
            m_samplePeak = std::max(m_samplePeak, double(std::abs(x)));
            st.history[st.historyPos] = x;
            st.historyPos = (st.historyPos + 1) % kTapsPerPhase;
            for (int p = 0; p < kOversample; ++p) {
                const std::vector<float> &coef = m_phases[p];
                float y = 0;
                for (int t = 0; t < kTapsPerPhase; ++t)
                    y += coef[t] * st.history[(st.historyPos + kTapsPerPhase - 1 - t) % kTapsPerPhase];
                const double a = std::abs(y);
                m_truePeak = std::max(m_truePeak, a);
                m_truePeakSince = std::max(m_truePeakSince, a);
            }

            if (st.weight == 0)
                continue;
            // K-weighting: two transposed direct-form II biquads.
            double v = x;
            for (int k = 0; k < 2; ++k) {
                const Biquad &b = m_stage[k];
                const double y = b.b0 * v + st.z1[k];
                st.z1[k] = b.b1 * v - b.a1 * y + st.z2[k];
                st.z2[k] = b.b2 * v - b.a2 * y;
                v = y;
            }
            m_subBlockEnergy += st.weight * v * v;
        }
        if (++m_subBlockCount == m_subBlockFrames)
            closeSubBlock();
    }
}

void LoudnessMeter::closeSubBlock()
{
    m_subBlocks.push_back(m_subBlockEnergy / double(m_subBlockFrames));
    if (m_subBlocks.size() > 30)
        m_subBlocks.pop_front();
    m_subBlockEnergy = 0;
    m_subBlockCount = 0;
    if (m_subBlocks.size() >= 4)
        m_gatingBlocks.push_back(meanOfLast(4));
}

double LoudnessMeter::meanOfLast(size_t count) const
{
    if (m_subBlocks.size() < count || count == 0)
        return 0;
    return std::accumulate(m_subBlocks.end() - qsizetype(count), m_subBlocks.end(), 0.0) / double(count);
}

double LoudnessMeter::momentary() const
{
    return m_subBlocks.size() >= 4 ? toLufs(meanOfLast(4)) : -INFINITY;
}

double LoudnessMeter::shortTerm() const
{
    // Before 3 s have passed, the average of what there is.
    return m_subBlocks.empty() ? -INFINITY : toLufs(meanOfLast(m_subBlocks.size()));
}

double LoudnessMeter::integrated() const
{
    // Absolute gate at −70 LUFS, then a relative gate 10 LU below the
    // loudness of the blocks that passed it.
    const double absolute = std::pow(10.0, (-70.0 + 0.691) / 10.0);
    double sum = 0;
    size_t n = 0;
    for (double e : m_gatingBlocks)
        if (e > absolute) {
            sum += e;
            ++n;
        }
    if (n == 0)
        return -INFINITY;
    const double relative = sum / n * std::pow(10.0, -10.0 / 10.0);
    sum = 0;
    n = 0;
    for (double e : m_gatingBlocks)
        if (e > absolute && e > relative) {
            sum += e;
            ++n;
        }
    return n ? toLufs(sum / n) : -INFINITY;
}

double LoudnessMeter::truePeak() const
{
    return m_truePeak > 0 ? 20.0 * std::log10(m_truePeak) : -INFINITY;
}

double LoudnessMeter::samplePeak() const
{
    return m_samplePeak > 0 ? 20.0 * std::log10(m_samplePeak) : -INFINITY;
}

double LoudnessMeter::takeTruePeak()
{
    const double v = m_truePeakSince > 0 ? 20.0 * std::log10(m_truePeakSince) : -INFINITY;
    m_truePeakSince = 0;
    return v;
}

void Correlation::process(const float *s, qint64 frames, int channels, int left, int right)
{
    if (channels <= std::max(left, right))
        return;
    for (qint64 f = 0; f < frames; ++f) {
        const double l = s[f * channels + left], r = s[f * channels + right];
        m_lr += l * r;
        m_ll += l * l;
        m_rr += r * r;
    }
}

double Correlation::take(double fallback)
{
    const double d = std::sqrt(m_ll * m_rr);
    const double v = d > 1e-12 ? std::clamp(m_lr / d, -1.0, 1.0) : fallback;
    m_lr = m_ll = m_rr = 0;
    return v;
}

} // namespace ffgui
