# GPU Preemption Frame Timer

A Windows kernel-mode driver and user-space C++ tool that exposes GPU preemption events and correlates them with frame time spikes.

---

## Motivation 💡

I have always been into PC gaming and a few months ago built my own PC. One thing that always frustrated me was frame time spikes, those moments where the game visibly stutters even though your average FPS looks fine. Tools like MSI Afterburner and CapFrameX can tell you when a spike happened, but not why.

After taking my operating systems course at UBC and getting interested in how drivers work, I realized the answer lives in the kernel. Frame spikes are frequently caused by GPU preemption: when the Windows GPU scheduler forcibly interrupts your game's workload to service another process. This event happens inside `dxgkrnl.sys`, below the level that any user-space profiling tool can observe. So I decided to attempt to build one that could.

---

## What It Does 🔍

✅ Measures frame times with nanosecond precision using `QueryPerformanceCounter`  
✅ Maintains a ring buffer of the last 512 frame times for rolling statistics  
✅ Detects frame spikes using a configurable multiplier threshold (default 2x baseline)  
✅ Reports **1% low** and **0.1% low FPS**
✅ Runs a kernel-mode Windows driver (`.sys`) that captures simulated GPU preemption events via a `KTIMER`/`KDPC` pipeline (same interrupt deferral mechanism used in real GPU drivers)  
✅ Exposes kernel events to user space over a custom IOCTL interface  
✅ Correlates each frame spike with any preemption events buffered by the driver, printing the time offset in milliseconds  

---

## Demo 🎬

### Driver loaded and running in VM
<!-- Add screenshot of sc query PreemptionTracker showing RUNNING -->

### frametimer.exe output
<!-- Add screenshot of terminal output showing CONNECTED and live FPS bar -->

### Spike detection with kernel correlation
<!-- Add screenshot of a SPIKE block showing preemption event offset -->

### Live demo
<!-- Add your GIF here: ![demo](demo.gif) -->

### Session summary
<!-- Add screenshot of session summary -->

---

## Architecture

```
+--------------------------------------------------+
|                  User Space                      |
|                                                  |
|  main.cpp                                        |
|    game loop → FrameTimer::mark()                |
|                     |                            |
|              spike detected                      |
|                     |                            |
|         KernelBridge::QueryEvents()              |
|           DeviceIoControl (IOCTL)                |
+--------------------------------------------------+
                       |
           kernel / user space boundary
                       |
+--------------------------------------------------+
|                 Kernel Space                     |
|                                                  |
|  PreemptionTracker.sys                           |
|                                                  |
|  KTIMER → KDPC → PushEvent()                      |
|                     |                            |
|            Ring buffer                           |
|            protected by KSPIN_LOCK               |
|                     |                            |
|        EvtIoDeviceControl → PopEvents()          |
+--------------------------------------------------+
```

**User-space components** (C++17, cross-platform):

- `Clock` — wraps `QueryPerformanceCounter` on Windows. Uses raw QPC ticks so timestamps align directly with kernel events
- `FrameBuffer` — fixed-capacity ring buffer storing frame times.
- `FrameTimer` — delta timing engine with spike detection callback. Fires `SpikeEvent` with `qpc_ticks` as the kernel search key
- `KernelBridge` — opens a handle to the driver via `CreateFile`, sends `IOCTL_GET_PREEMPTION_EVENTS` via `DeviceIoControl`, returns buffered `PreemptionEvent` structs

**Kernel-mode driver** (C++, WDK, KMDF):

- Non-PnP control device
- `KSPIN_LOCK` protecting the ring buffer
- `KTIMER` + `KDPC` simulating preemption interrupt events every ~80ms
- `METHOD_BUFFERED` IOCTL
---

## How It Was Built 🔧

* User space (cross-platform C++17):**
- Built and tested `Clock`, `FrameBuffer`, `FrameTimer`
- Wrote a 15-test hand-rolled test harness verifying monotonicity, delta accuracy, spike detection, percentile math, and ring buffer wrapping
- Designed `SpikeEvent` with a `qpc_ticks` field from the start

**Kernel driver (Windows, WDK, KMDF):**
- Set up a Windows 11 VM with test signing enabled, kernel debugging over network via `kdnet`
- Connected WinDbg on host to VM kernel for live crash analysis
- Implemented a non-PnP KMDF control device with symbolic link, IO queue, and IOCTL handler
- Implemented ring buffer with `KSPIN_LOCK` and `KTIMER`/`KDPC` event source
- Wired `KernelBridge` into the spike callback

---

## Challenges 🧠

1. **IRQL constraints** — kernel ring buffer access had to use `KSPIN_LOCK` instead of a mutex because the DPC fires at `DISPATCH_LEVEL`. Getting this wrong causes an immediate bugcheck
2. **Driver unload safety** — `KeCancelTimer` must be called in `EvtDriverUnload` before returning. A timer that fires after the driver is unloaded executes code that no longer exists in memory
3. **C++ linkage in kernel mode** — `DriverEntry` required `extern "C"` to prevent C++ name mangling from hiding it from the WDF linker
4. **QPC alignment** — user space and kernel both use `QueryPerformanceCounter` / `KeQueryPerformanceCounter` which read the same hardware TSC, ensuring tick values are directly comparable across the boundary without conversion
5. **Static linking** — the user-space binary required `-static` to bundle MinGW runtime DLLs, since the VM doesn't have MSYS2 installed

---

## Production Implementation Path 🚀

The current event source is a simulated `KTIMER`/`KDPC` firing every 80ms. The scaling path to real GPU preemption events would be:

**Replace `SimulatedPreemptionDpc` with `DxgkDdiInterruptRoutine`:**

In a full WDDM driver, the GPU signals a preemption-complete interrupt when the scheduler's preemption request is acknowledged. The driver's ISR would:

1. Acknowledge the hardware interrupt by writing to the GPU's interrupt clear register via MMIO (mapped from the GPU's PCI BAR space using `MmMapIoSpace`)
2. Read the preemption reason and context ID from GPU status registers
3. Queue a DPC via `KeInsertQueueDpc`

The DPC would then call `KeQueryPerformanceCounter` for the timestamp and `PushEvent`. The ring buffer, spin lock, IOCTL interface, and user-space correlation code require no changes.

**Additional improvements:**
- Pass the spike's `qpc_ticks` into the IOCTL so the driver filters events server-side within a ±5ms window, rather than returning all buffered events
- Add preemption reason codes (timeout, priority preemption, TDR) from GPU registers to `PreemptionEvent`
- Extend the user-space tool to log sessions to a CSV for post-analysis
---

## Build Instructions

### Driver (Windows, Visual Studio + WDK)

```
1. Open an empty KMDF project template in Visual Studio
2. Copy \driverFiles\Preemptiontracker.cpp to a new file in the "Driver Files" folder
3. Replace the .inf file with \driverFiles\Preemptiontracker.inf
4. Create a new folder called "Include" and copy \driverFiles\Shared.h into the folder
5. Set target to x64 / Debug
6. Build → produces PreemptionTracker.sys
7. Copy file over to VM
8. In VM (admin command prompt):
   sc create PreemptionTracker type= kernel binPath= "C:\path\to\PreemptionTracker.sys"
   sc start PreemptionTracker
```

> **Important:** Always load the driver in a VM with test signing enabled (`bcdedit /set testsigning on`). A bug in a kernel-mode driver will bluescreen the host immediately.

### User-space tool (Windows, MSYS2 UCRT64)

```bash (once in correct userspace project directory)
mkdir build
cd build
cmake -G "Unix Makefiles" ..
make
```

Copy `frametimer.exe` into the VM and run it in command prompt as adminstrator. Running on the host without the driver loaded works too — the tool degrades gracefully and prints frame stats only.

---

## References

- [Windows Display Driver Model documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/windows-vista-display-driver-model-design-guide)
- [GPU preemption in WDDM](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/gpu-preemption)
- [QueryPerformanceCounter internals](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
- [Kernel-mode driver development with WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/)
