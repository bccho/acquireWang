#pragma once
#pragma warning(push, 0)
#include <cstdlib>
#include <cstring>
#include "debug.h"
#pragma warning(pop)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class provides an interface for a generic frame object. The datatype
 * is abstracted away into the class, and the internal data buffer is
 * protected (though mutable) and internally managed. Different camera types
 * should inherit from this class and replace the void pointers (void*) with
 * concrete data type pointers, such as uint8_t* or uint16_t*, for the sake
 * of type safety.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class BaseFrame {
private:
	size_t width, height, channels;
	size_t bytesPerPixel;
	bool valid;

	double timestamp;
	void* data;

	// Allocates memory for data buffer
	void* allocate() { return std::calloc(getNumPixels(), bytesPerPixel); }

public:
	// Constructor and destructor
	BaseFrame(size_t _width, size_t _height, size_t _bytesPerPixel, size_t _channels) :
			width(_width), height(_height), bytesPerPixel(_bytesPerPixel), channels(_channels),
			timestamp(0), valid(true) {
		data = allocate();
	}
	BaseFrame(size_t _width, size_t _height, size_t channels, size_t _bytesPerPixel, void* _data, double _timestamp) :
			BaseFrame(_width, _height, _bytesPerPixel, channels) {
		copyDataFromBuffer(_data);
		setTimestamp(_timestamp);
	}
	// Default constructor and destructor
	BaseFrame() : width(0), height(0), channels(0), bytesPerPixel(0), timestamp(0), data(nullptr), valid(false) {}
	virtual ~BaseFrame() {
		//debugMessage("~BaseFrame " + std::to_string(width) + " " + std::to_string(height), DEBUG_INFO);
		if (data != nullptr) std::free(data);
	}

	// Copy constructor (deep copy; calls assignment operator overload)
	BaseFrame(const BaseFrame& other) : width(other.width), height(other.height), channels(other.channels),
			bytesPerPixel(other.bytesPerPixel), timestamp(other.timestamp), valid(other.valid) {
		timers.start(DTIMER_FRAME_COPY_CONST);
		data = allocate();
		copyDataFromBuffer(other.data);
		timers.pause(DTIMER_FRAME_COPY_CONST);
	}
	
	// Getters and setters
	bool isValid() const { return valid; }
	size_t getWidth() const { return width; }
	size_t getHeight() const { return height; }
	size_t getChannels() const { return channels; }
	size_t getBytesPerPixel() const { return bytesPerPixel; }
	size_t getNumPixels() const { return width * height * channels; }
	size_t getBytes() const { return getNumPixels() * bytesPerPixel; }

	double getTimestamp() { return timestamp; }
	void setTimestamp(double _timestamp) { timestamp = _timestamp; }

	// Buffer access methods (protect data from abuse)
	// Derived classes should override these for type safety
	void copyDataFromBuffer(void* buffer, bool verbose = false, std::string context = "") {
		try {
			if (verbose) {
				debugMessage("copyDataFromBuffer: context " + context, DEBUG_INFO);
			}
			timers.start(DTIMER_COPY_FROM);
			std::memcpy(data, buffer, getBytes());
			timers.pause(DTIMER_COPY_FROM);
		}
		catch (...) {
			valid = false;
		}
	}
	void copyDataToBuffer(void* buffer) {
		try {
			timers.start(DTIMER_COPY_TO);
			std::memcpy(buffer, data, getBytes());
			timers.pause(DTIMER_COPY_TO);
		}
		catch (...) {
			valid = false;
		}
	}

	// Assignment operator override
	BaseFrame& operator=(const BaseFrame& other) { // deep copy
		timers.start(DTIMER_FRAME_ASSIGN);
		if (this != &other) {
			width = other.width;
			height = other.height;
			channels = other.channels;
			bytesPerPixel = other.bytesPerPixel;
			valid = other.valid;

			timestamp = other.timestamp;
			data = allocate(); // assignment operator is called on an uncontructed instance?
			copyDataFromBuffer(other.data);
		}
		timers.pause(DTIMER_FRAME_ASSIGN);

		return *this;
	}
};