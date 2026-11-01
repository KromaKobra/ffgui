#include "Scopes.h"
#include <algorithm>
#include <cmath>
#include <complex>
namespace ffgui
{
std::vector<double> SpectrumAnalyzer::bands(const float *x, int frames, int channels, int rate, bool musical)
{
    if (musical) {
        std::vector<double> out(96, -100);
        const double q = 1 / (std::pow(2., 1 / 12.) - 1);
        for (int band = 0; band < 96; ++band) {
            const double frequency = 32.7032 * std::pow(2., band / 12.);
            if (frequency >= rate / 2.)
                continue;
            const int n = std::clamp(int(std::ceil(q * rate / frequency)), 16, 4096);
            const double angle = -2 * M_PI * frequency / rate;
            const std::complex<double> rotation(std::cos(angle), std::sin(angle));
            std::complex<double> phase(1, 0), sum(0, 0);
            double weight = 0;
            for (int i = 0; i < n; ++i) {
                const double window = 0.5 - 0.5 * std::cos(2 * M_PI * i / (n - 1));
                const int frame = frames - n + i;
                if (frame >= 0) {
                    double mono = 0;
                    for (int c = 0; c < channels; ++c)
                        mono += x[frame * channels + c];
                    sum += mono / channels * window * phase;
                }
                weight += window;
                phase *= rotation;
            }
            out[band] = std::max(-100., 20 * std::log10(std::max(1e-10, std::abs(sum) * 2 / weight)));
        }
        return out;
    }
    constexpr int n = 2048;
    std::vector<std::complex<float>> fft(n);
    Fft transform(n);
    for (int i = 0; i < n; ++i) {
        const int f = frames - n + i;
        if (f >= 0 && f < frames) {
            double mono = 0;
            for (int c = 0; c < channels; ++c)
                mono += x[f * channels + c];
            fft[i] = float(mono / channels * (0.5 - 0.5 * std::cos(2 * M_PI * i / (n - 1))));
        }
    }
    transform.transform(fft.data());
    std::vector<double> out(musical ? 96 : 64, -100);
    for (int b = 0; b < int(out.size()); ++b) {
        const double lo = musical ? 32.7032 * std::pow(2., (b - 0.5) / 12.)
                                  : 20 * std::pow(std::max(1., rate / 2. / 20), double(b) / out.size());
        const double hi = musical ? 32.7032 * std::pow(2., (b + 0.5) / 12.)
                                  : 20 * std::pow(std::max(1., rate / 2. / 20), double(b + 1) / out.size());
        const int a = std::clamp(int(lo * n / rate), 1, n / 2),
                  z = std::clamp(int(std::ceil(hi * n / rate)), a + 1, n / 2 + 1);
        double peak = 0;
        for (int k = a; k < z; ++k)
            peak = std::max(peak, double(std::abs(fft[k])) * 4 / n);
        out[b] = std::max(-100., 20 * std::log10(std::max(peak, 1e-10)));
    }
    return out;
}
std::vector<double> Histogram::bins(const float *x, int count)
{
    std::vector<double> out(64);
    for (int i = 0; i < count; ++i) {
        const double db = 20 * std::log10(std::max(1e-6, double(std::abs(x[i]))));
        ++out[std::clamp(int((db + 96) / 96 * 63), 0, 63)];
    }
    const double high = *std::max_element(out.begin(), out.end());
    if (high > 0)
        for (auto &v : out)
            v /= high;
    return out;
}
std::vector<double> BitScope::activity(const float *x, int count)
{
    std::vector<double> out(24);
    for (int i = 0; i < count; ++i) {
        const qint32 v = qint32(std::llround(std::clamp(double(x[i]), -1., 1.) * 8388607));
        const quint32 bits = quint32(v < 0 ? -v : v);
        for (int bit = 0; bit < 24; ++bit)
            out[bit] += bool(bits & (1U << bit));
    }
    if (count)
        for (auto &v : out)
            v /= count;
    return out;
}
QVariantList Vectorscope::points(const float *x, int frames, int channels)
{
    QVariantList points;
    const int step = std::max(1, frames / 512);
    for (int i = 0; i < frames; i += step) {
        double l = x[i * channels], r = channels > 1 ? x[i * channels + 1] : l;
        points << QVariantMap{{"x", (l - r) * 0.70710678}, {"y", (l + r) * 0.70710678}};
    }
    return points;
}
QVariantMap ScopeAnalysis::analyze(const float *x, int frames, int channels, int rate)
{
    if (frames <= 0 || channels <= 0 || rate <= 0)
        return {};
    auto list = [](const std::vector<double> &v) {
        QVariantList out;
        for (double n : v)
            out << n;
        return out;
    };
    double ll = 0, rr = 0, lr = 0;
    for (int i = 0; i < frames; ++i) {
        const double l = x[i * channels], r = channels > 1 ? x[i * channels + 1] : l;
        ll += l * l;
        rr += r * r;
        lr += l * r;
    }
    return {{"spectrum", list(SpectrumAnalyzer::bands(x, frames, channels, rate))},
            {"cqt", list(SpectrumAnalyzer::bands(x, frames, channels, rate, true))},
            {"histogram", list(Histogram::bins(x, frames * channels))},
            {"bits", list(BitScope::activity(x, frames * channels))},
            {"vectorscope", Vectorscope::points(x, frames, channels)},
            {"phase", ll * rr > 0 ? lr / std::sqrt(ll * rr) : 0.0}};
}
} // namespace ffgui
