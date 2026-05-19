#include "Clock.h"

Clock::Clock() {
    #ifdef _WIN32
    QueryPerformanceFrequency(&frequency_);
    #endif
};

int64_t Clock::now_ns() const {
#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    // have to divide by freq to get seconds
    return (ticks.QuadPart * 1'000'000'000) / frequency_.QuadPart;
#else
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
#endif
}

int64_t Clock::now_ticks() const {
#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
#else
    return now_ns();
#endif
}

int64_t Clock::ticks_to_ns(int64_t ticks) const {
#ifdef _WIN32
    return (ticks * 1'000'000'000) / frequency_.QuadPart;
#else
    return ticks;
#endif
}