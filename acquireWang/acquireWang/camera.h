#pragma once
#pragma warning(push, 0)
#include <utility>
#pragma warning(pop)
#include "frame.h"

// Constants
const bool DEBUGGING = false;

class BaseCamera {
protected:
	size_t width, height, channels, bytesPerPixel;
	double fps;
public:
	// Default constructor to give default values to members
	BaseCamera() : width(0), height(0), channels(0), bytesPerPixel(0), fps(0) {}
	virtual ~BaseCamera() {
		endAcquisition();
		finalize();
		debugMessage("~BaseCamera", DEBUG_HIDDEN_INFO);
	}

	// These methods are called externally, and do nothing by default.
	// Override if you need them to do something.
	virtual void initialize() {};
	virtual void finalize() {};
	virtual void beginAcquisition() {};
	virtual void endAcquisition() {};

	// [frame] should already have the right dimensions, etc.
	// (getFrame only fills the data buffer of the frame)
	virtual BaseFrame getFrame() = 0;

	size_t getWidth() { return width; }
	size_t getHeight() { return height; }
	size_t getChannels() { return channels; }
	size_t getBytesPerPixel() { return bytesPerPixel; }
	size_t getFrameSize() { return width * height * channels; }
	size_t getBytes() { return width * height * channels * bytesPerPixel; }
	double getFPS() { return fps; }
};
