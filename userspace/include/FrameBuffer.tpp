#include <cstddef>
#include <cstdint>
#include <array>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "FrameBuffer.h"

// Inserts a new frame time (in nanoseconds) into the buffer.
template<size_t Capacity>
void FrameBuffer<Capacity>::push(int64_t frame_time_ns) {
    buffer_[head_ % Capacity] = frame_time_ns;
    ++head_;
    if (count_ < Capacity) {
        ++count_;
    }
}

// returns how many valid samples are currently in buffer
template<size_t Capacity>
size_t FrameBuffer<Capacity>::count() const {
    return count_;
}

// return true if buffer has no entries
template<size_t Capacity>
bool FrameBuffer<Capacity>::empty() const {
    return count_ == 0;
}

// returns most recently pushed frame
template<size_t Capacity>
int64_t FrameBuffer<Capacity>::back() const {
    if (empty()) {
        return 0;
    }
    return buffer_[(head_ - 1) % Capacity];
}

// Returns the mean of the last N frame times, in nanoseconds.
template<size_t Capacity>
double FrameBuffer<Capacity>::rolling_avg_ns(size_t window) const {
    if (empty()) {
        return 0.0;
    }
    size_t actual_window = std::min(window, count_);
    int64_t sum = 0;
    for (size_t i = 0; i < actual_window; i++) {
        sum += buffer_[(head_ - actual_window + i) % Capacity];
    }
    return static_cast<double>(sum) / static_cast<double>(actual_window);
}

// Returns the frame time at the Pth percentile of all stored samples.
template<size_t Capacity>
double FrameBuffer<Capacity>::percentile_ns(double p) const {
    if (empty()) {
        return 0.0;
    }
    std::vector<int64_t> sorted_frames(count_);
    for (size_t i = 0; i < count_; i++) {
        sorted_frames[i] = buffer_[(head_ - count_ + i) % Capacity];
    }
    std::sort(sorted_frames.begin(), sorted_frames.end());
    size_t index = static_cast<size_t>(p / 100.0 * (count_ - 1));
    return static_cast<double>(sorted_frames[index]);
}

// Returns true if most recent frame is a "spike" as determined by the threshold
template<size_t Capacity>
bool FrameBuffer<Capacity>::is_spike(double threshold, size_t window) const {
    if (count_ < 5) {
        return false; // not enough data to determine
    }
    return static_cast<double>(back()) > threshold * rolling_avg_ns(window);
}

template<size_t Capacity>
double FrameBuffer<Capacity>::avg_fps() const {
    if (empty()) {
        return 0.0;
    }
    double avg = rolling_avg_ns(count_);
    if (avg <= 0) {
        return 0.0;
    }
    return 1e9 / avg;
}