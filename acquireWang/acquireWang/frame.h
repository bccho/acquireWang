#pragma once
#pragma warning(push, 0)
#include <cstdlib>
#include <cstring>
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
	const size_t width, height, channels;
	const size_t bytesPerPixel;
	double timestamp;
	void* data;

protected:
	// Constructor and destructor
	BaseFrame(size_t _width, size_t _height, size_t _bytesPerPixel, size_t _channels = 1) :
			width(_width), height(_height), bytesPerPixel(_bytesPerPixel), channels(_channels),
			timestamp(0) {
		data = std::calloc(width * height * channels, bytesPerPixel);
	}
	BaseFrame(size_t _width, size_t _height, size_t _bytesPerPixel, void* _data, double _timestamp) :
			BaseFrame(_width, _height, _bytesPerPixel) {
		copyDataFromBuffer(_data);
		setTimestamp(_timestamp);
	}

public:
	// Default constructor and destructor
	BaseFrame() : width(0), height(0), channels(0), bytesPerPixel(0), timestamp(0), data(nullptr) {}
	~BaseFrame() { if (data != nullptr) std::free(data); }
	// Copy constructor (shallow copy)
	BaseFrame(const BaseFrame& other) :
			width(other.width), height(other.height), channels(other.channels), bytesPerPixel(other.bytesPerPixel),
			timestamp(other.timestamp), data(other.data) {}
	
	// Getters and setters
	size_t getWidth() { return width; }
	size_t getHeight() { return height; }
	size_t getChannels() { return channels; }
	size_t getBytesPerPixel() { return bytesPerPixel; }
	size_t getNumPixels() { return width * height * channels; }
	size_t getBytes() { return getNumPixels() * bytesPerPixel; }

	double getTimestamp() { return timestamp; }
	void setTimestamp(double _timestamp) { timestamp = _timestamp; }

	// Buffer access methods (protect data from abuse)
	// Derived classes should override these for type safety
	void copyDataFromBuffer(void* buffer) {
		std::memcpy(data, buffer, getBytes());
	}
	void copyDataToBuffer(void* buffer) {
		std::memcpy(buffer, data, getBytes());
	}

	// Assignment operator override
	BaseFrame operator=(const BaseFrame& other) { return BaseFrame(other); } // note: shallow copy!
};