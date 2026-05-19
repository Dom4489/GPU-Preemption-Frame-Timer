#pragma once 
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <chrono>
#endif

#include <cstdint>

class Clock {
    public:
        Clock();

        int64_t now_ns() const;

        int64_t now_ticks() const;

        int64_t ticks_to_ns(int64_t ticks) const;
    private:
#ifdef _WIN32
        LARGE_INTEGER frequency_{};
#endif
};