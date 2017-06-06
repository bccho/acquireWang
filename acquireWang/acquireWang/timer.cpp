#include "timer.h"

double getClockStamp() {
	FILETIME preciseTime; ULONGLONG t;
	GetSystemTimePreciseAsFileTime(&preciseTime);
	t = ((ULONGLONG)preciseTime.dwHighDateTime << 32) | (ULONGLONG)preciseTime.dwLowDateTime;
	return (double)t / 10000000.0; // converted to seconds
}
