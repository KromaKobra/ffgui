#include "Fft.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace ffgui {

Fft::Fft(int size)
    : m_size(size)
    , m_reverse(size)
    , m_twiddles(size / 2)
{
    int bits = 0;
    while ((1 << bits) < size)
        ++bits;
    for (int i = 0; i < size; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            r |= ((i >> b) & 1) << (bits - 1 - b);
        m_reverse[i] = r;
    }
    for (int k = 0; k < size / 2; ++k) {
        const double a = -2.0 * std::numbers::pi * k / size;
        m_twiddles[k] = { float(std::cos(a)), float(std::sin(a)) };
    }
}

void Fft::transform(std::complex<float> *data) const
{
    for (int i = 0; i < m_size; ++i)
        if (i < m_reverse[i])
            std::swap(data[i], data[m_reverse[i]]);
    for (int len = 2; len <= m_size; len <<= 1) {
        const int half = len / 2;
        const int stride = m_size / len;
        for (int i = 0; i < m_size; i += len) {
            for (int j = 0; j < half; ++j) {
                const std::complex<float> t = m_twiddles[j * stride] * data[i + j + half];
                data[i + j + half] = data[i + j] - t;
                data[i + j] += t;
            }
        }
    }
}

} // namespace ffgui
