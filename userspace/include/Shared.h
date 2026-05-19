#pragma once

#ifndef NTDEF_H  // Avoid conflicts with kernel driver includes
#include <windows.h>
#include <winioctl.h>
#endif

// Maximum number of events that can be tracked per query
#define MAX_EVENTS_PER_QUERY 16

typedef struct _PreemptionEvent {
	LONGLONG qpc_ticks; // tick count at the moment of preemption interrupt
	LONGLONG duration_ticks; // duration of the preemption in ticks
	ULONG context_id; // identifier for the GPU context involved in the preemption interrupt
	ULONG padding; // padding to align struct to 8 bytes
} PreemptionEvent;

// IOCTL code to retrieve preemption events from the driver for user-space
#define IOCTL_GET_PREEMPTION_EVENTS \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
