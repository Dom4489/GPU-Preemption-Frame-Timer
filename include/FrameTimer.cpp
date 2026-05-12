#include "FrameTimer.h"

FrameTimer::FrameTimer(SpikeCallback on_spike) : on_spike_(std::move(on_spike)) {}

int64_t FrameTimer::mark() {
    int64_t ticks = clock_.now_ticks();
    int64_t now = clock_.ticks_to_ns(ticks);

    // First call, arm timer and return 0
    if (last_ns_ == 0) {
        last_ns_ = now;
        last_ticks_ = ticks;
        return 0;
    }
    // calculate frame time and push to buffer
    int64_t frame_time = now - last_ns_;
    buffer_.push(frame_time);
    // check for spike and fire callback if registered
    if (on_spike_ && buffer_.is_spike()) {
        SpikeEvent evt{};
        evt.timestamp_ns = last_ns_;
        evt.frame_time_ns = frame_time;
        evt.rolling_avg_ns = buffer_.rolling_avg_ns();
        evt.multiplier = static_cast<double>(frame_time) /evt.rolling_avg_ns;
        evt.qpc_ticks = last_ticks_;
        on_spike_(evt);
    }

    // update time stamps for next call
    last_ns_ = now;
    last_ticks_ = ticks;
    return frame_time;
}

const FrameBuffer<>& FrameTimer::buffer() const {
    return buffer_;
}

double FrameTimer::current_fps() const {
    return buffer_.avg_fps();
}

double FrameTimer::low1_fps() const {
    return 1e9 / buffer_.percentile_ns(99.0);
}

double FrameTimer::low01_fps() const {
    return 1e9 / buffer_.percentile_ns(99.9);
}