#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

// A single-producer, single-consumer ring of floats. Lock-free: one thread
// writes, another (the audio callback) reads.
namespace ffgui {

class RingBuffer
{
public:
    explicit RingBuffer(size_t capacity)
        : m_data(capacity + 1)
    {
    }

    size_t capacity() const { return m_data.size() - 1; }

    size_t readable() const
    {
        const size_t w = m_write.load(std::memory_order_acquire), r = m_read.load(std::memory_order_relaxed);
        return w >= r ? w - r : m_data.size() - r + w;
    }
    size_t writable() const { return capacity() - readable(); }

    // Producer side.
    size_t write(const float *src, size_t count)
    {
        const size_t w = m_write.load(std::memory_order_relaxed), r = m_read.load(std::memory_order_acquire);
        const size_t size = m_data.size();
        const size_t free = (r + size - w - 1) % size;
        count = std::min(count, free);
        const size_t first = std::min(count, size - w);
        std::memcpy(m_data.data() + w, src, first * sizeof(float));
        std::memcpy(m_data.data(), src + first, (count - first) * sizeof(float));
        m_write.store((w + count) % size, std::memory_order_release);
        return count;
    }

    // Consumer side.
    size_t read(float *dst, size_t count)
    {
        const size_t r = m_read.load(std::memory_order_relaxed), w = m_write.load(std::memory_order_acquire);
        const size_t size = m_data.size();
        const size_t available = w >= r ? w - r : size - r + w;
        count = std::min(count, available);
        const size_t first = std::min(count, size - r);
        std::memcpy(dst, m_data.data() + r, first * sizeof(float));
        std::memcpy(dst + first, m_data.data(), (count - first) * sizeof(float));
        m_read.store((r + count) % size, std::memory_order_release);
        return count;
    }

private:
    std::vector<float> m_data;
    std::atomic<size_t> m_write { 0 };
    std::atomic<size_t> m_read { 0 };
};

} // namespace ffgui
