#pragma once
#pragma warning(push, 0)
#include <iostream>
#include <ctime>
#include <string>
#pragma warning(pop)
#include "debugtimers.h"

extern DebugTimers timers;

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

void printDebugTimerInfo();
