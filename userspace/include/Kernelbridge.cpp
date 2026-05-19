#include "Kernelbridge.h"
#include <iostream>

static constexpr wchar_t DEVICE_PATH[] = L"\\\\.\\PreemptionTracker";

KernelBridge::KernelBridge()
    : device_(INVALID_HANDLE_VALUE)
{}

KernelBridge::~KernelBridge() {
    Close();
}

bool KernelBridge::Open() {
    device_ = CreateFileW(
        DEVICE_PATH,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (device_ == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            std::cerr << "[KernelBridge] Driver not loaded. "
                      << "Run: sc start PreemptionTracker\n"
                      << "  Frame timer will run without kernel correlation.\n";
        } else {
            std::cerr << "[KernelBridge] CreateFile failed. Error: " << err
                      << "\n  Try running as administrator.\n";
        }
        return false;
    }

    std::cout << "[KernelBridge] Connected to PreemptionTracker driver.\n";
    return true;
}

bool KernelBridge::IsOpen() const {
    return device_ != INVALID_HANDLE_VALUE && device_ != nullptr;
}

std::vector<PreemptionEvent> KernelBridge::QueryEvents() {
    std::vector<PreemptionEvent> result;
    if (!IsOpen()) return result;

    PreemptionEvent outBuffer[MAX_EVENTS_PER_QUERY] = {};
    DWORD bytesReturned = 0;

    BOOL success = DeviceIoControl(
        device_,
        IOCTL_GET_PREEMPTION_EVENTS,
        nullptr,
        0,
        outBuffer,
        sizeof(outBuffer),
        &bytesReturned,
        nullptr); // synchronous call

    if (!success) {
        DWORD err = GetLastError();
        std::cerr << "[KernelBridge] DeviceIoControl failed. Error: " << err << "\n";
        return result;
    }

    // Compute how many complete events were returned.
    DWORD eventCount = bytesReturned / sizeof(PreemptionEvent);
    for (DWORD i = 0; i < eventCount; ++i) {
        result.push_back(outBuffer[i]);
    }

    return result;
}

void KernelBridge::Close() {
    if (IsOpen()) {
        CloseHandle(device_);
        device_ = INVALID_HANDLE_VALUE;
    }
}

LONGLONG KernelBridge::GetQpcFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}