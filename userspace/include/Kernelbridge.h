#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include "Shared.h"

class KernelBridge {
public:
    KernelBridge();
    ~KernelBridge();
    // prevent copying
    KernelBridge(const KernelBridge&) = delete;
    KernelBridge& operator=(const KernelBridge&) = delete;

    bool Open();
    bool IsOpen() const;
    std::vector<PreemptionEvent> QueryEvents();
    void Close();
    static LONGLONG GetQpcFrequency();

private:
    HANDLE device_;
};