#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h>
#include "Shared.h"

extern "C" DRIVER_INITIALIZE DriverEntry;
#define RING_BUFFER_SIZE 256

static PreemptionEvent g_EventBuffer[RING_BUFFER_SIZE];
static ULONG g_head = 0;
static ULONG g_count = 0;
static KSPIN_LOCK g_BufferLock; // protects access to the ring buffer

static KTIMER g_Timer;
static KDPC g_Dpc;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
KDEFERRED_ROUTINE SimulatedPreemptionDpc;

static void PushEvent(_In_ const PreemptionEvent* evt) {
	KIRQL oldIrql;
	KeAcquireSpinLock(&g_BufferLock, &oldIrql);
	g_EventBuffer[g_head % RING_BUFFER_SIZE] = *evt;
	g_head++;
	if (g_count < RING_BUFFER_SIZE) g_count++;
	KeReleaseSpinLock(&g_BufferLock, oldIrql);
}

static ULONG PopEvents(
	_Out_writes_(maxEvents) PreemptionEvent* outBuffer,
	_In_ ULONG maxEvents) {
	KIRQL oldIrql;
	KeAcquireSpinLock(&g_BufferLock, &oldIrql);

	ULONG count = min(g_count, maxEvents);
	ULONG start = g_head - g_count;
	for (ULONG i = 0; i < count; i++) {
		outBuffer[i] = g_EventBuffer[(start + i) % RING_BUFFER_SIZE];
	}
	// clear buffer after reading
	g_count = 0;

	KeReleaseSpinLock(&g_BufferLock, oldIrql);
	return count;
}

VOID simulatedPreemptionDpc(
	_In_ PKDPC Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2) {
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	LARGE_INTEGER freq;
	PreemptionEvent evt = {};
	evt.qpc_ticks = KeQueryPerformanceCounter(&freq).QuadPart;
	// simulate a preemption duration of 10ms
	evt.duration_ticks = freq.QuadPart / 100;
	evt.context_id = 1;
	evt.padding = 0;
	PushEvent(&evt);

	// re-arm timer
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -800000LL;
	KeSetTimer(&g_Timer, dueTime, &g_Dpc);

}

VOID EvtIoDeviceControl(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t OutputBufferLength,
	_In_ size_t InputBufferLength,
	_In_ ULONG IoControlCode
) {
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(InputBufferLength);

	// Only accept our custom IOCTL for retrieving preemption events
	if (IoControlCode != IOCTL_GET_PREEMPTION_EVENTS) {
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}

	// reject if no space for at least one event
	if (OutputBufferLength < sizeof(PreemptionEvent)) {
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	PreemptionEvent* outBuffer = nullptr;
	size_t bufferSize = 0;
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(
		Request, 
		sizeof(PreemptionEvent), 
		reinterpret_cast<PVOID*>(&outBuffer),
		&bufferSize);

	if (!NT_SUCCESS(status)) {
		WdfRequestComplete(Request, status);
		return;
	}

	// cap events
	ULONG maxEvents = static_cast<ULONG>(bufferSize / sizeof(PreemptionEvent));
	if (maxEvents > MAX_EVENTS_PER_QUERY) {
		maxEvents = MAX_EVENTS_PER_QUERY;
	}

	ULONG count = PopEvents(outBuffer, maxEvents);
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, static_cast<ULONG_PTR>(count * sizeof(PreemptionEvent)));
}

VOID EvtDriverUnload(_In_ WDFDRIVER Driver) {
	UNREFERENCED_PARAMETER(Driver);
	KeCancelTimer(&g_Timer);
}

extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath) {
	// Initialize ring buffer spin lock
	KeInitializeSpinLock(&g_BufferLock);

	// initialize dpc and timer
	KeInitializeTimer(&g_Timer);
	KeInitializeDpc(&g_Dpc, simulatedPreemptionDpc, nullptr);

	// create driver object 
	WDF_DRIVER_CONFIG config;
	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
	config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
	config.EvtDriverUnload = EvtDriverUnload;

	WDFDRIVER driver;
	NTSTATUS status = WdfDriverCreate(
		DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		&driver
	);

	if (!NT_SUCCESS(status)) {
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
			"PreemptionTracker: WdfDriverCreate failed 0x%x\n", status));
		return status;
	}

	// allocate control device init structure
	DECLARE_CONST_UNICODE_STRING(sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;WD)");
	PWDFDEVICE_INIT deviceInit = WdfControlDeviceInitAllocate(driver, &sddl);
	if (!deviceInit) return STATUS_INSUFFICIENT_RESOURCES;

	// set device name
	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, L"\\Device\\PreemptionTracker");
	status = WdfDeviceInitAssignName(deviceInit, &deviceName);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(deviceInit);
		return status;
	}
	WdfDeviceInitSetIoType(deviceInit, WdfDeviceIoBuffered);

	// create device object
	WDFDEVICE device;
	status = WdfDeviceCreate(&deviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
	if (!NT_SUCCESS(status)) return status;

	// create sym link
	UNICODE_STRING symLink;
	RtlInitUnicodeString(&symLink, L"\\DosDevices\\PreemptionTracker");
	status = WdfDeviceCreateSymbolicLink(device, &symLink);
	if (!NT_SUCCESS(status)) return status;

	// create IO queue that handles our custom IOCTL
	WDF_IO_QUEUE_CONFIG queueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
	queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;
	status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) return status;

	// finialize control device
	WdfControlFinishInitializing(device);

	// arm timer
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -800000LL; // 80ms relative time
	KeSetTimer(&g_Timer, dueTime, &g_Dpc);
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,"PreemptionTracker: driver loaded successfully\n"));

	return STATUS_SUCCESS;
	
}