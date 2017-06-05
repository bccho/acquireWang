#pragma once
#include <utility>

// Constants
const bool DEBUGGING = false;

template <typename T> class BaseCamera {
protected:
	size_t width, height;
	double fps;
public:
	// Default constructor to give default values to members
	BaseCamera() : width(0), height(0), fps(0) {}

	// These methods are called externally, and do nothing by default.
	// Override if you need them to do something.
	void initialize() {};
	void finalize() {};
	void beginAcquisition() {};
	void endAcquisition() {};

	// Must return a valid frame, so must be overridden
	virtual std::pair<bool, void*> getFrame() = 0;

	size_t getWidth() { return width; }
	size_t getHeight() { return height; }
	size_t getFrameSize() { return width * height; }
	double getFPS() { return fps; }
};
