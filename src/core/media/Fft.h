#pragma once

#include <complex>
#include <vector>

// An in-place iterative radix-2 FFT of a fixed power-of-two size.
namespace ffgui {

class Fft
{
public:
    explicit Fft(int size);
    int size() const { return m_size; }
    void transform(std::complex<float> *data) const;

private:
    int m_size;
    std::vector<int> m_reverse;
    std::vector<std::complex<float>> m_twiddles;
};

} // namespace ffgui
