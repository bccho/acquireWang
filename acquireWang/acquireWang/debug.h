#pragma once
#pragma warning(push, 0)
#include <iostream>
#include <ctime>
#include <string>
#pragma warning(pop)
#include "debugtimers.h"

/* Debug messages */
enum DEBUG_LEVELS {
	DEBUG_TRIVIAL_INFO = 20,
	DEBUG_HIDDEN_INFO = 15,
	DEBUG_INFO = 10,
	DEBUG_IMPORTANT_INFO = 8,
	DEBUG_WARNING = 6,
	DEBUG_MINOR_ERROR = 4,
	DEBUG_ERROR = 3,
	DEBUG_MUST_SHOW = 0
};

#define MAX_VERBOSITY DEBUG_INFO
#define DEBUG_SHOW_TIMESTAMPS true

inline void debugMessage(const std::string message, int verbosity) {
	if (verbosity <= MAX_VERBOSITY) {
		if (DEBUG_SHOW_TIMESTAMPS) {
			// Get current wall clock time
			std::chrono::time_point<std::chrono::system_clock> now_time = std::chrono::system_clock::now();
			std::time_t now_time_t = std::chrono::system_clock::to_time_t(now_time);
			// Format string
			std::tm * now_time_data = std::localtime(&now_time_t);
			char now_time_str[256];
			std::strftime(now_time_str, 256, "%F %T", now_time_data);
			// Output as timestamp
			std::cout << "[" << now_time_str << "] ";
		}
		std::cout << message << std::endl;
	}
}

/* Debug timers */
extern DebugTimers timers;

enum dtimer {
	DTIMER_OVERALL = 0,				// overall (start to finish of recording command)
	DTIMER_PREP = 1,				// initialization
	DTIMER_CLEANUP = 2,				// finalization
	DTIMER_ACQUISITION = 3,			// from start to stop of acquisition
	DTIMER_WRITE_FRAME = 4,			// writing frames to file
	DTIMER_COPY_TO = 5,				// copying frames to (any) buffers
	DTIMER_COPY_FROM = 6,			// copying frames from (any) buffers
	DTIMER_GET_FRAME = 7,			// getting frames from camera
	DTIMER_MOVE_WRITE = 8,			// moving frames to write buffers (includes dequeueing frames)
	DTIMER_DEQUEUE = 9,				// dequeueing frames
	DTIMER_FRAME_COPY_CONST = 10,	// frame copy constructor
	DTIMER_FRAME_ASSIGN = 11		// frame assignment operator
};

inline void printDebugTimerInfo() {
	debugMessage("Overall:                          " + std::to_string(timers.getTotalTime(DTIMER_OVERALL)), DEBUG_INFO);
	debugMessage("Main thread:", DEBUG_INFO);
	debugMessage("  Initialization:                 " + std::to_string(timers.getTotalTime(DTIMER_PREP)), DEBUG_INFO);
	debugMessage("  Finalization:                   " + std::to_string(timers.getTotalTime(DTIMER_CLEANUP)), DEBUG_INFO);
	debugMessage("  Acquisition:                    " + std::to_string(timers.getTotalTime(DTIMER_ACQUISITION)), DEBUG_INFO);
	debugMessage("Acquisition threads (total):", DEBUG_INFO);
	debugMessage("  Getting frames:                 " + std::to_string(timers.getTotalTime(DTIMER_GET_FRAME)), DEBUG_INFO);
	debugMessage("Saving thread:", DEBUG_INFO);
	debugMessage("  Writing frames:                 " + std::to_string(timers.getTotalTime(DTIMER_WRITE_FRAME)), DEBUG_INFO);
	debugMessage("  Moving frames to write buffers: " + std::to_string(timers.getTotalTime(DTIMER_MOVE_WRITE)), DEBUG_INFO);
	debugMessage("    Dequeueing frames:            " + std::to_string(timers.getTotalTime(DTIMER_DEQUEUE)), DEBUG_INFO);
	debugMessage("General:", DEBUG_INFO);
	debugMessage("  Copying frames to buffers:      " + std::to_string(timers.getTotalTime(DTIMER_COPY_TO)), DEBUG_INFO);
	debugMessage("  Copying frames from buffers:    " + std::to_string(timers.getTotalTime(DTIMER_COPY_FROM)), DEBUG_INFO);
	debugMessage("  Frame copy constructor:         " + std::to_string(timers.getTotalTime(DTIMER_FRAME_COPY_CONST)), DEBUG_INFO);
	debugMessage("  Frame assignment operator:      " + std::to_string(timers.getTotalTime(DTIMER_FRAME_ASSIGN)), DEBUG_INFO);
}
