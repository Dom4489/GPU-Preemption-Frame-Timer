#include "FrameTimer.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

// Sleeps for ~ ns nanoseconds
// Use QPC spin wait for precise sleeping on Windows
#ifdef _WIN32
static void precise_sleep_ns(int64_t ns) {
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    int64_t ticks_needed = (ns * freq.QuadPart) / 1'000'000'000LL;
    do { QueryPerformanceCounter(&now); }
    while ((now.QuadPart - start.QuadPart) < ticks_needed);
}
#else
static void precise_sleep_ns(int64_t ns) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}
#endif

// Erases the current terminal line and moves the cursor to the start.
static void clear_line() {
    std::cout << "\r\033[K";
}

// Prints a simple ASCII progress bar representing current FPS vs target FPS.
static void print_bar(double fps, double target_fps) {
    const int width = 30;
    int filled = static_cast<int>((fps / target_fps) * width);
    filled = std::max(0, std::min(width, filled));
    std::cout << "[";
    for (int i = 0; i < width; ++i)
        std::cout << (i < filled ? '=' : ' ');
    std::cout << "]";
}

// Simulates a game loop running at ~60fps with occasional frame spikes.
// compute and record frame times of previous frame at the top of each frame (includes OS scheduling latency between frames)
// simulate work by sleeping for ~ one frame's worth of time
// every ~80 frames, sleep longer to simulate a GPU preemption stall
// write live stats to the terminal every 5 frames
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const double  target_fps    = 60.0;
    const int64_t target_ns     = static_cast<int64_t>(1e9 / target_fps); // ~16.6ms
    const int     total_frames  = 300;

    std::cout << "GPU Preemption Frame Timer\n";
    std::cout << "Simulating " << target_fps << " fps with injected preemption stalls\n\n";

    int spike_count = 0;

    // Construct the FrameTimer with a lambda spike callback.
    FrameTimer timer([&](const SpikeEvent& evt) {
        ++spike_count;
        double ms     = evt.frame_time_ns  / 1e6;
        double avg_ms = evt.rolling_avg_ns / 1e6;

        // Print on a new line so it doesn't get overwritten by the HUD.
        std::cout << "\n"
                  << "  *** SPIKE #" << spike_count
                  << "  frame=" << std::fixed << std::setprecision(2) << ms << "ms"
                  << "  baseline=" << avg_ms << "ms"
                  << "  (" << std::setprecision(1) << evt.multiplier << "x)"
                  << "  qpc_ticks=" << evt.qpc_ticks
                  // call DeviceIoControl here, passing evt.qpc_ticks
                  // as the search key into the kernel driver's event ring buffer.
                  << " ***\n";
    });

    for (int frame = 0; frame < total_frames; ++frame) {

        // Mark the start of this frame (records end of previous frame).
        timer.mark();
        int64_t work_ns = target_ns;

        if (frame > 10 && frame % 80 == 0) {
            // Inject a preemption-stall spike: 3-7x the normal frame time.
            // frame > 10 guard prevents triggering before the baseline settles.
            work_ns = target_ns * (3 + std::rand() % 5);
        } else {
            // Normal frame with ±10% jitter
            work_ns += (std::rand() % (target_ns / 5)) - (target_ns / 10);
        }

        precise_sleep_ns(work_ns);

        // Update every 5 frames.
        if (frame % 5 == 0) {
            clear_line();
            std::cout << "  Frame " << std::setw(4) << frame << "  ";
            print_bar(timer.current_fps(), target_fps);
            std::cout << "  " << std::fixed << std::setprecision(1)
                      << timer.current_fps() << " fps"
                      << "  1%low=" << std::setprecision(1) << timer.low1_fps()
                      << "  spikes=" << spike_count;
            std::cout.flush();
        }
    }

    // Final mark to capture the last frame's time.
    timer.mark();

    // Print the session summary.
    std::cout << "\n\nSession summary\n";
    std::cout << "---------------\n";
    std::cout << "  Avg FPS  : " << std::fixed << std::setprecision(1) << timer.current_fps() << "\n";
    std::cout << "  1% Low   : " << timer.low1_fps()  << "\n";
    std::cout << "  0.1% Low : " << timer.low01_fps() << "\n";
    std::cout << "  Spikes   : " << spike_count << "\n";

    return 0;
}