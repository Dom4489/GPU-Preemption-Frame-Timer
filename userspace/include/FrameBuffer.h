#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <numeric>
#include <vector>

template<size_t Capacity = 512>
class FrameBuffer {
    public:
        void push(int64_t frame_time_ns);
        size_t count() const;
        bool empty() const;
        int64_t back() const;
        double rolling_avg_ns(size_t window = 60) const;
        double percentile_ns(double p) const;
        bool is_spike(double threshold = 2.0, size_t window = 60) const;
        double avg_fps() const;
    private:
        std::array<int64_t, Capacity> buffer_{};
        size_t head_ = 0;
        size_t count_ = 0;
};

#include "FrameBuffer.tpp"