#pragma once
#pragma warning(push, 0)
#include <iostream>
#include <string>
#pragma warning(pop)

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

inline void debugMessage(const std::string message, int verbosity) {
	if (verbosity <= MAX_VERBOSITY) std::cout << message << std::endl;
}
