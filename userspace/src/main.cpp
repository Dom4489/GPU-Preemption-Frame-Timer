// Runs a simulated game loop, detects frame time spikes using the
// FrameTimer and queries the kernel driver for GPU preemption events that
// coincide with each spike.
//
// Two modes:
// 1. With driver loaded: spike fires → IOCTL query → preemption events printed
// 2. Without driver loaded: spike fires → "(kernel driver not loaded)" printed
//
// The frame timer runs identically in both modes and KernelBridge degrades
// gracefully if the driver is absent.
//
// How to run:
// 1. Build and load PreemptionTracker.sys in your VM (target PC)
// 2. Build this tool on your host PC
// 3. Copy the compiled frametimer.exe into the VM and run it there (with driver loaded)

#define NOMINMAX
#include "FrameTimer.h"
#include "KernelBridge.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

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

static void clear_line() { std::cout << "\r\033[K"; }

static void print_bar(double fps, double target) {
    const int w = 30;
    int filled = static_cast<int>((fps / target) * w);
    filled = std::max(0, std::min(w, filled));
    std::cout << "[";
    for (int i = 0; i < w; ++i) std::cout << (i < filled ? '=' : ' ');
    std::cout << "]";
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const double  TARGET_FPS   = 60.0;
    const int64_t TARGET_NS    = static_cast<int64_t>(1e9 / TARGET_FPS);
    const int     TOTAL_FRAMES = 600; // ~10 seconds at 60fps

    // open the kernel bridge
    // if the driver is not loaded, prints a warning and returns false.
    KernelBridge bridge;
    bool driverAvailable = bridge.Open();
    LONGLONG qpcFreq = KernelBridge::GetQpcFrequency();

    std::cout << "\nGPU Preemption Frame Timer\n";
    std::cout << "Kernel driver: " << (driverAvailable ? "CONNECTED" : "NOT LOADED") << "\n";
    std::cout << "Simulating " << TARGET_FPS << " fps with injected stalls every ~80 frames\n\n";

    int spikeCount = 0;

    // when a spike fires:
    // 1. print the spike details
    // 2. if driver is available, query for preemption events
    // 3. print any events found, with ms offset from the spike timestamp
    FrameTimer timer([&](const SpikeEvent& evt) {
        ++spikeCount;
        double frameMs = evt.frame_time_ns / 1e6;
        double avgMs = evt.rolling_avg_ns / 1e6;

        std::cout << "\n";
        std::cout << "  SPIKE #" << spikeCount << "\n";
        std::cout << "    Frame time : " << std::fixed << std::setprecision(2)
                  << frameMs << " ms\n";
        std::cout << "    Baseline   : " << avgMs << " ms\n";
        std::cout << "    Multiplier : " << std::setprecision(1)
                  << evt.multiplier << "x\n";
        std::cout << "    QPC ticks  : " << evt.qpc_ticks << "\n";

        if (driverAvailable) {
            // Query the kernel driver for any buffered preemption events.
            auto events = bridge.QueryEvents();

            if (events.empty()) {
                std::cout << "    Preemption : none recorded in this window\n";
            } else {
                std::cout << "    Preemption events: " << events.size() << " found\n";
                for (size_t i = 0; i < events.size(); ++i) {
                    // Compute the time offset between the preemption event
                    // and the start of the spike in milliseconds.
                    LONGLONG tickDelta  = events[i].qpc_ticks - evt.qpc_ticks;
                    double offsetMs = (static_cast<double>(tickDelta) * 1000.0) / static_cast<double>(qpcFreq);
                    double durationMs = (static_cast<double>(events[i].duration_ticks) * 1000.0) / static_cast<double>(qpcFreq);

                    std::cout << "      [" << i << "] context_id=" << events[i].context_id
                              << "  offset=" << std::setprecision(2) << offsetMs << "ms"
                              << "  duration=" << durationMs << "ms\n";
                }
            }
        } else {
            std::cout << "    Preemption : (driver not loaded)\n";
        }
        std::cout << "\n";
    });

    // Game loop
    for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
        timer.mark();

        int64_t workNs = TARGET_NS;

        if (frame > 10 && frame % 80 == 0) {
            // Inject a spike: 3-7x normal frame time, simulating a preemption stall.
            workNs = TARGET_NS * (3 + std::rand() % 5);
        } else {
            // Normal jitter
            workNs += (std::rand() % (TARGET_NS / 5)) - (TARGET_NS / 10);
        }

        precise_sleep_ns(workNs);

        if (frame % 5 == 0) {
            clear_line();
            std::cout << "  Frame " << std::setw(4) << frame << "  ";
            print_bar(timer.current_fps(), TARGET_FPS);
            std::cout << "  " << std::fixed << std::setprecision(1)
                      << timer.current_fps() << " fps"
                      << "  1%%low=" << timer.low1_fps()
                      << "  spikes=" << spikeCount;
            std::cout.flush();
        }
    }

    timer.mark();

    // Session summary
    std::cout << "\n\nSession Summary\n";
    std::cout << "---------------\n";
    std::cout << "  Avg FPS  : " << std::fixed << std::setprecision(1)
              << timer.current_fps() << "\n";
    std::cout << "  1%% Low  : " << timer.low1_fps()  << "\n";
    std::cout << "  0.1%% Low: " << timer.low01_fps() << "\n";
    std::cout << "  Spikes   : " << spikeCount << "\n";
    std::cout << "  Driver   : " << (driverAvailable ? "connected" : "not loaded") << "\n";

    return 0;
}