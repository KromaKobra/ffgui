#pragma once

#include <QtGlobal>

#include <vector>

// Min, max and RMS of decoded audio at several resolutions: 256-sample base
// blocks, each level above 4× coarser. Answers "what's in this pixel column"
// for any zoom down to the base block; below that, use the samples.
namespace ffgui {

struct PeakColumn
{
    float min = 0;
    float max = 0;
    float rms = 0;
    bool valid = false;
};

class PeakPyramid
{
public:
    static constexpr int kBaseBlock = 256;
    static constexpr int kFactor = 4;

    void reset(int channels, qint64 expectedFrames = 0);
    void append(const float *interleaved, qint64 frames);
    // Closes the last, partial blocks so the tail is queryable.
    void finish();

    int channels() const { return m_channels; }
    qint64 frames() const { return m_frames; }

    // [first, last) frames of one channel, or of all channels with -1.
    PeakColumn range(int channel, qint64 first, qint64 last) const;
    // `count` equal columns over [first, last) (fractional frames).
    void columns(int channel, double first, double last, int count, PeakColumn *out) const;

private:
    struct Level
    {
        qint64 blockFrames = 0;
        qint64 blocks = 0;
        std::vector<float> min, max, sumsq;    // [block * channels + channel]
    };
    void pushBlock(size_t level, const float *mins, const float *maxs, const float *sumsqs);
    PeakColumn aggregate(const Level &level, int channel, qint64 firstBlock, qint64 lastBlock, qint64 frames) const;

    int m_channels = 0;
    qint64 m_frames = 0;
    std::vector<Level> m_levels;
    // The block being filled at each level.
    std::vector<std::vector<float>> m_pendMin, m_pendMax, m_pendSq;
    std::vector<qint64> m_pendCount;
};

} // namespace ffgui
