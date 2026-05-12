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
       explicit FrameTimer(SpikeCallback on_spike = nullptr);
       int64_t mark();
       const FrameBuffer<>& buffer() const;
       double current_fps() const;
       double low1_fps() const;
       double low01_fps() const;

    private:
       Clock clock_;
       FrameBuffer<> buffer_;
       int64_t last_ns_ = 0;
       int64_t last_ticks_ = 0;
       SpikeCallback on_spike_;
};