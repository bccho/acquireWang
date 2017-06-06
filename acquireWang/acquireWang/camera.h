#pragma once
#pragma warning(push, 0)
#include <utility>
#pragma warning(pop)
#include "frame.h"

// Constants
const bool DEBUGGING = false;

class BaseCamera {
protected:
	size_t width, height, channels;
	double fps;
public:
	// Default constructor to give default values to members
	BaseCamera() : width(0), height(0), channels(0), fps(0) {}

	// These methods are called externally, and do nothing by default.
	// Override if you need them to do something.
	void initialize() {};
	void finalize() {};
	void beginAcquisition() {};
	void endAcquisition() {};

	// Must return a valid frame, so must be overridden
	virtual std::pair<bool, BaseFrame> getFrame() = 0;

	size_t getWidth() { return width; }
	size_t getHeight() { return height; }
	size_t getChannels() { return channels; }
	size_t getFrameSize() { return width * height * channels; }
	double getFPS() { return fps; }
};
