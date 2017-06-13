#pragma once
#include <chrono>
#include <vector>

class DebugTimers {
private:
	size_t numTimers;
	std::vector<double> times;
	std::vector<bool> enabled;
	std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> starts;
public:
	// Constructor
	DebugTimers(size_t _numTimers) : numTimers(_numTimers), times(numTimers, 0), enabled(numTimers, false),
			starts(numTimers, std::chrono::time_point<std::chrono::high_resolution_clock>()) {}
	
	// Start a timer
	void start(size_t ind) {
		// Validate input
		if (ind < 0 || ind >= numTimers) return; // out of bounds
		if (enabled[ind]) return; // already started

		// Start timer
		enabled[ind] = true;
		starts[ind] = std::chrono::high_resolution_clock::now();
	}

	// Pause a timer and add to total elapsed time
	void pause(size_t ind) {
		// Validate input
		if (ind < 0 || ind >= numTimers) return; // out of bounds
		if (!enabled[ind]) return; // not started

		// Pause timer
		enabled[ind] = false;
		std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - starts[ind];
		times[ind] += elapsed.count();
	}

	// Returns if a timer is currently running
	bool isRunning(size_t ind) {
		return enabled[ind];
	}

	// Returns total elapsed time of a timer (does not include current time if timer is running)
	double getTotalTime(size_t ind) {
		// Validate input
		if (ind < 0 || ind >= numTimers) return -1; // out of bounds

		return times[ind];
	}

	// Resets all timers
	void resetAll() {
		for (size_t i = 0; i < numTimers; i++) {
			enabled[i] = false;
			times[i] = 0;
			starts[i] = std::chrono::time_point<std::chrono::high_resolution_clock>();
		}
	}
}; 
