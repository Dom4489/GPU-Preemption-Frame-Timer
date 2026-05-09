#pragma once
#include "Clock.h"
#include "FrameBuffer.h"
#include <cstdint>
#include <functional>

struct SpikeEvent {
    // When stutter began
    int64_t timestamp_ns;
    // Frame time of the spike
    double frame_time_ns;
    // Rolling average frame time at the time of the spike
    double rolling_avg_ns;
    // Multiplier for the spike
    double multiplier;
    // QPC ticks at the time of the spike
    int64_t qpc_ticks;
};

using SpikeCallback = std::function<void(const SpikeEvent&)>;
class FrameTimer {
    public:
       explicit FrameTimer(SpikeCallback on_spike = nullptr) : on_spike_(std::move(on_spike)) {}

        // Call this every frame with the latest frame time in nanoseconds
        void tick(int64_t frame_time_ns);

    private:
        SpikeCallback callback_;
        double spike_threshold_;
        size_t rolling_window_;
        FrameBuffer<> frame_buffer_;
};