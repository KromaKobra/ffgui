#include "PeakPyramid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ffgui {

void PeakPyramid::reset(int channels, qint64 expectedFrames)
{
    m_channels = std::max(1, channels);
    m_frames = 0;
    m_levels.clear();
    m_pendMin.clear();
    m_pendMax.clear();
    m_pendSq.clear();
    m_pendCount.clear();
    // Enough levels that the top one has a handful of blocks.
    int levels = 1;
    for (qint64 block = kBaseBlock; block * kFactor * 8 < std::max<qint64>(expectedFrames, kBaseBlock * 64); block *= kFactor)
        ++levels;
    levels = std::clamp(levels, 1, 12);
    qint64 blockFrames = kBaseBlock;
    for (int l = 0; l < levels; ++l, blockFrames *= kFactor) {
        Level level;
        level.blockFrames = blockFrames;
        const size_t expectedBlocks = expectedFrames > 0 ? size_t(expectedFrames / blockFrames + 2) : 0;
        level.min.reserve(expectedBlocks * m_channels);
        level.max.reserve(expectedBlocks * m_channels);
        level.sumsq.reserve(expectedBlocks * m_channels);
        m_levels.push_back(std::move(level));
        m_pendMin.emplace_back(m_channels, std::numeric_limits<float>::max());
        m_pendMax.emplace_back(m_channels, std::numeric_limits<float>::lowest());
        m_pendSq.emplace_back(m_channels, 0.0f);
        m_pendCount.push_back(0);
    }
}

void PeakPyramid::pushBlock(size_t l, const float *mins, const float *maxs, const float *sumsqs)
{
    Level &level = m_levels[l];
    level.min.insert(level.min.end(), mins, mins + m_channels);
    level.max.insert(level.max.end(), maxs, maxs + m_channels);
    level.sumsq.insert(level.sumsq.end(), sumsqs, sumsqs + m_channels);
    ++level.blocks;
    if (l + 1 >= m_levels.size())
        return;
    // Fold into the next level's pending block.
    auto &pmin = m_pendMin[l + 1], &pmax = m_pendMax[l + 1], &psq = m_pendSq[l + 1];
    for (int c = 0; c < m_channels; ++c) {
        pmin[c] = std::min(pmin[c], mins[c]);
        pmax[c] = std::max(pmax[c], maxs[c]);
        psq[c] += sumsqs[c];
    }
    if (++m_pendCount[l + 1] == kFactor) {
        pushBlock(l + 1, pmin.data(), pmax.data(), psq.data());
        std::fill(pmin.begin(), pmin.end(), std::numeric_limits<float>::max());
        std::fill(pmax.begin(), pmax.end(), std::numeric_limits<float>::lowest());
        std::fill(psq.begin(), psq.end(), 0.0f);
        m_pendCount[l + 1] = 0;
    }
}

void PeakPyramid::append(const float *s, qint64 frames)
{
    if (m_levels.empty())
        return;
    auto &pmin = m_pendMin[0], &pmax = m_pendMax[0], &psq = m_pendSq[0];
    qint64 &count = m_pendCount[0];
    for (qint64 f = 0; f < frames; ++f) {
        const float *frame = s + f * m_channels;
        for (int c = 0; c < m_channels; ++c) {
            const float v = frame[c];
            pmin[c] = std::min(pmin[c], v);
            pmax[c] = std::max(pmax[c], v);
            psq[c] += v * v;
        }
        if (++count == kBaseBlock) {
            pushBlock(0, pmin.data(), pmax.data(), psq.data());
            std::fill(pmin.begin(), pmin.end(), std::numeric_limits<float>::max());
            std::fill(pmax.begin(), pmax.end(), std::numeric_limits<float>::lowest());
            std::fill(psq.begin(), psq.end(), 0.0f);
            count = 0;
        }
    }
    m_frames += frames;
}

void PeakPyramid::finish()
{
    for (size_t l = 0; l < m_levels.size(); ++l) {
        if (m_pendCount[l] == 0)
            continue;
        pushBlock(l, m_pendMin[l].data(), m_pendMax[l].data(), m_pendSq[l].data());
        m_pendCount[l] = 0;
        std::fill(m_pendMin[l].begin(), m_pendMin[l].end(), std::numeric_limits<float>::max());
        std::fill(m_pendMax[l].begin(), m_pendMax[l].end(), std::numeric_limits<float>::lowest());
        std::fill(m_pendSq[l].begin(), m_pendSq[l].end(), 0.0f);
    }
}

PeakColumn PeakPyramid::aggregate(const Level &level, int channel, qint64 firstBlock, qint64 lastBlock, qint64 frames) const
{
    PeakColumn col;
    firstBlock = std::max<qint64>(0, firstBlock);
    lastBlock = std::min(lastBlock, level.blocks);
    if (firstBlock >= lastBlock || frames <= 0)
        return col;
    float mn = std::numeric_limits<float>::max(), mx = std::numeric_limits<float>::lowest();
    double sq = 0;
    const int c0 = channel < 0 ? 0 : channel, c1 = channel < 0 ? m_channels : channel + 1;
    for (qint64 b = firstBlock; b < lastBlock; ++b) {
        const size_t base = size_t(b) * m_channels;
        for (int c = c0; c < c1; ++c) {
            mn = std::min(mn, level.min[base + c]);
            mx = std::max(mx, level.max[base + c]);
            sq += level.sumsq[base + c];
        }
    }
    col.min = mn;
    col.max = mx;
    col.rms = float(std::sqrt(sq / double(frames * (c1 - c0))));
    col.valid = true;
    return col;
}

PeakColumn PeakPyramid::range(int channel, qint64 first, qint64 last) const
{
    if (m_levels.empty() || last <= first || channel >= m_channels)
        return {};
    const qint64 span = last - first;
    size_t l = 0;
    while (l + 1 < m_levels.size() && m_levels[l + 1].blockFrames * 2 <= span)
        ++l;
    const Level &level = m_levels[l];
    const qint64 fb = first / level.blockFrames;
    const qint64 lb = (last + level.blockFrames - 1) / level.blockFrames;
    const qint64 covered = std::min(lb, level.blocks) - std::max<qint64>(0, fb);
    return aggregate(level, channel, fb, lb, std::min(covered * level.blockFrames, std::max<qint64>(1, m_frames)));
}

void PeakPyramid::columns(int channel, double first, double last, int count, PeakColumn *out) const
{
    if (count <= 0)
        return;
    const double step = (last - first) / count;
    if (m_levels.empty() || step <= 0 || channel >= m_channels) {
        std::fill(out, out + count, PeakColumn {});
        return;
    }
    size_t l = 0;
    while (l + 1 < m_levels.size() && double(m_levels[l + 1].blockFrames) <= step)
        ++l;
    const Level &level = m_levels[l];
    for (int i = 0; i < count; ++i) {
        const double a = first + i * step, b = a + step;
        qint64 fb = qint64(std::floor(a / level.blockFrames));
        qint64 lb = qint64(std::ceil(b / level.blockFrames));
        if (lb <= fb)
            lb = fb + 1;
        out[i] = aggregate(level, channel, fb, lb, (std::min(lb, level.blocks) - std::max<qint64>(0, fb)) * level.blockFrames);
    }
}

} // namespace ffgui
