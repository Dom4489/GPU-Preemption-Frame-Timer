#include <iostream>
#include <thread>
#include <chrono>
#include "Clock.h"

int main() {
    Clock clock;

    // Test 1: Get current time in nanoseconds
    int64_t start_ns = clock.now_ns();
    std::cout << "Start time (ns): " << start_ns << std::endl;

    // Test 2: Get current time in ticks
    int64_t start_ticks = clock.now_ticks();
    std::cout << "Start time (ticks): " << start_ticks << std::endl;

    // Sleep for 100ms to show time progression
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Test 3: Get time again
    int64_t end_ns = clock.now_ns();
    std::cout << "End time (ns): " << end_ns << std::endl;

    int64_t end_ticks = clock.now_ticks();
    std::cout << "End time (ticks): " << end_ticks << std::endl;

    // Test 4: Calculate elapsed time
    int64_t elapsed_ns = end_ns - start_ns;
    int64_t elapsed_ticks = end_ticks - start_ticks;

    std::cout << "\nElapsed time (ns): " << elapsed_ns << std::endl;
    std::cout << "Elapsed time (ticks): " << elapsed_ticks << std::endl;

    return 0;
}
