// =============================================================================
// FrameTimer.cpp
// =============================================================================
// Implementations for FrameTimer methods declared in FrameTimer.h.
//
// WHY IS THIS IN A .cpp AND NOT THE HEADER?
//   FrameTimer is a regular (non-template) class. The compiler only needs to
//   see the declarations (in .h) at call sites — it links the definitions
//   (here in .cpp) later at link time. This keeps compile times fast and is
//   standard practice for non-template classes in C++.
//
//   Contrast with FrameBuffer.h — that IS a template, so its full
//   implementation must stay in the header where the compiler can see it
//   whenever it instantiates the template for a specific Capacity value.
// =============================================================================

#include "FrameTimer.h"

// =============================================================================
// FrameTimer::FrameTimer()
// =============================================================================
// Constructor. Takes an optional spike callback and moves it into the member.
//
// Why std::move?
//   std::function can internally heap-allocate storage for large closures
//   (lambdas that capture many variables). Moving transfers ownership of that
//   heap allocation instead of copying it. For small lambdas the compiler
//   will likely optimise this away, but moving is the correct habit.
//
// Why the member initialiser list `: on_spike_(std::move(on_spike))`?
//   Members are constructed before the constructor body runs. Initialising
//   in the list constructs them directly with the right value. Assigning
//   inside the body would default-construct first, then assign — one extra
//   operation for no benefit.
//
// clock_, buffer_, last_ns_, last_ticks_ are all zero/default initialised
// via their in-class initialisers declared in FrameTimer.h.
// =============================================================================
FrameTimer::FrameTimer(SpikeCallback on_spike)
    : on_spike_(std::move(on_spike))
{}

// =============================================================================
// FrameTimer::mark()
// =============================================================================
// Records the current timestamp, computes the frame time since the last call,
// pushes it into the ring buffer, and fires the spike callback if needed.
//
// FIRST CALL BEHAVIOUR:
//   last_ns_ starts at 0, which is used as a sentinel meaning "not yet called".
//   now_ns() will never return 0 in practice — it would require the hardware
//   QPC counter to be at exactly 0 at this precise instant, which cannot happen
//   on a running system. So 0 is a safe sentinel value.
//
//   On the first call we just arm the timer (store last_ns_ and last_ticks_)
//   and return 0 — there is nothing to measure yet.
//
// SUBSEQUENT CALLS:
//   frame_time = now_ns - last_ns_
//   This is the duration of the frame that just completed. It includes
//   everything that happened since the previous mark() — rendering work,
//   sleep, and any OS scheduling latency between frames.
//
// SPIKE DETECTION:
//   After pushing the frame time, we ask the buffer if this frame is a spike.
//   If yes, we populate a SpikeEvent. Note that qpc_ticks is set to
//   last_ticks_ — the tick value at the START of the spiking frame, not the
//   end. This is the correct search key for the kernel driver: we want to
//   find preemption events that occurred DURING the spike, not after it.
//
// TICK vs NS STORAGE:
//   We read both ticks and nanoseconds from the clock at the same instant
//   (two QPC calls very close together). ticks_to_ns() converts last_ticks_
//   for the delta calculation. last_ticks_ itself is preserved unconverted
//   for the SpikeEvent, because the kernel driver speaks ticks, not ns.
// =============================================================================
int64_t FrameTimer::mark() {
    int64_t ticks = clock_.now_ticks();
    int64_t now   = clock_.ticks_to_ns(ticks);

    // First call — arm the timer and return 0.
    if (last_ns_ == 0) {
        last_ns_    = now;
        last_ticks_ = ticks;
        return 0;
    }

    // Compute and record the frame time.
    int64_t frame_time = now - last_ns_;
    buffer_.push(frame_time);

    // Check for a spike and fire the callback if one is registered.
    if (on_spike_ && buffer_.is_spike()) {
        SpikeEvent evt{};
        evt.timestamp_ns   = last_ns_;
        evt.frame_time_ns  = frame_time;
        evt.rolling_avg_ns = buffer_.rolling_avg_ns();
        evt.multiplier     = static_cast<double>(frame_time) / evt.rolling_avg_ns;
        evt.qpc_ticks      = last_ticks_; // START of spike — the kernel search key
        on_spike_(evt);
    }

    // Advance the stored timestamps for the next call.
    last_ns_    = now;
    last_ticks_ = ticks;
    return frame_time;
}

// =============================================================================
// FrameTimer::buffer()
// =============================================================================
// Returns a const reference to the internal FrameBuffer.
//
// const& means: no copy (reference), and the caller cannot modify the buffer
// (const). This is the standard way to expose an internal container for
// read-only inspection without paying for a copy.
// =============================================================================
const FrameBuffer<>& FrameTimer::buffer() const {
    return buffer_;
}

// =============================================================================
// FrameTimer::current_fps()
// =============================================================================
// Returns average FPS over the buffer's rolling window.
//
// FPS = 1 second / average frame time
//     = 1,000,000,000 ns / avg_frame_time_ns
//
// FrameBuffer::avg_fps() handles the division and the zero-guard.
// =============================================================================
double FrameTimer::current_fps() const {
    return buffer_.avg_fps();
}

// =============================================================================
// FrameTimer::low1_fps()
// =============================================================================
// Returns the 1% low FPS.
//
// percentile_ns(1.0) gives the frame time that 99% of frames were faster than.
// Inverting it gives the FPS equivalent of that worst-1% frame time.
//
// This is the metric GPU review sites use to measure stutter. A game with
// 60 avg FPS but 5 FPS 1% low feels terrible; one with 55 avg and 50 FPS
// 1% low feels smooth. Average FPS alone hides this difference entirely.
// =============================================================================
double FrameTimer::low1_fps() const {
    return 1e9 / buffer_.percentile_ns(1.0);
}

// =============================================================================
// FrameTimer::low01_fps()
// =============================================================================
// Returns the 0.1% low FPS — the absolute worst frames in the sample set.
//
// At 512 samples, 0.1% is ~0.5 frames, so this effectively returns the
// single worst frame time recorded. More statistically meaningful once you
// accumulate thousands of samples across a longer session log.
// =============================================================================
double FrameTimer::low01_fps() const {
    return 1e9 / buffer_.percentile_ns(0.1);
}