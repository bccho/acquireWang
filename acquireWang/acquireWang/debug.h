#pragma once
#pragma warning(push, 0)
#include <iostream>
#include <string>
#pragma warning(pop)

enum Levels {
	LEVEL_INFO = 10,
	LEVEL_IMPORTANT_INFO = 8,
	LEVEL_WARNING = 6,
	LEVEL_MINOR_ERROR = 4,
	LEVEL_ERROR = 3,
	LEVEL_MUST_SHOW = 0
};

#define VERBOSE_LEVEL 10

inline void debugMessage(const std::string message, int verbosity) {
	if (VERBOSE_LEVEL >= verbosity) std::cout << message << std::endl;
}
