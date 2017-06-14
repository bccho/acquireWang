#pragma once
#pragma warning(push, 0)
#include <string>

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/packet_pipeline.h>
#pragma warning(pop)
#include "camera.h"
#include "frame.h"
#include "debug.h"

typedef uint16_t kinect_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Kinect camera frame class, which derives from the
 * BaseFrame class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class KinectFrame : public BaseFrame {
public:
	// Constructor overrides
	KinectFrame(size_t _width, size_t _height) :
			BaseFrame(_width, _height, sizeof(kinect_t), 1) {}
	KinectFrame(size_t _width, size_t _height, kinect_t* _data, double _timestamp) :
			BaseFrame(_width, _height, sizeof(kinect_t), 1, _data, _timestamp) {}
	// Method overrides
	void copyDataFromBuffer(kinect_t* buffer) {
		BaseFrame::copyDataFromBuffer(buffer);
	}
	void copyDataToBuffer(kinect_t* buffer) {
		BaseFrame::copyDataToBuffer(buffer);
	}
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Kinect camera class, which derives from the
 * BaseCamera class. The current implementation (using the default Windows
 * driver) does not permit multiple Kinect cameras, and currently only returns
 * the depth stream.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class KinectCamera : public BaseCamera {
private:
	libfreenect2::Freenect2Device* kinectSensor; // externally managed
	libfreenect2::SyncMultiFrameListener* listener;

	// Disable assignment operator and copy constructor
	KinectCamera& operator=(const KinectCamera& other) = delete;
	KinectCamera(const KinectCamera& other) = delete;
public:
	// Sensor should be managed externally (the constructor does not open the device,
	// and the sensor does not close it).
	KinectCamera(libfreenect2::Freenect2Device* sensor) :
			kinectSensor(sensor) {
		// Device should already be opened
		if (sensor == nullptr) {
			debugMessage("Device not opened!", DEBUG_ERROR);
			return; // TODO: throw exception
		}

		// Set up frame listener
		try {
			int frameTypes = libfreenect2::Frame::Depth
				| libfreenect2::Frame::Ir; // TODO: is IR necessary?
			listener = new libfreenect2::SyncMultiFrameListener(frameTypes);
			kinectSensor->setIrAndDepthFrameListener(listener);
		}
		catch (...) {
			debugMessage("Error setting up frame event listener", DEBUG_ERROR);
			return; // TODO: throw exception
		}

		// Important camera variables
		bytesPerPixel = sizeof(kinect_t);
		height = 424;
		width = 512;
		channels = 1;
		fps = 30;
	}
	~KinectCamera() override {
		debugMessage("~KinectCamera", DEBUG_HIDDEN_INFO);
		delete listener; // never free listener before kinect stop
	}

	void beginAcquisition() override {
		debugMessage("kinect initialize()", DEBUG_HIDDEN_INFO);
		bool res = kinectSensor->start();
		if (!res) debugMessage("Error starting Kinect sensor", DEBUG_ERROR);
	}

	// Must call endAcquisition() before KinectCamera is destroyed!
	void endAcquisition() override {
		kinectSensor->stop();
	}

	BaseFrame getFrame() override {
		try {
			libfreenect2::FrameMap frameMap;

			// Wait for frame
			debugMessage("Waiting...", DEBUG_HIDDEN_INFO);
			const int wait_timeout = 100; // ms (DWORD = uint32)
			const int max_tries = 10; // total 1 second
			//while (true) {
			for (int i = 0; i < max_tries; i++) {
				// If frame received, then stop waiting
				if (listener->waitForNewFrame(frameMap, wait_timeout)) break;
			}

			// Get frame
			libfreenect2::Frame* depthFrame = frameMap[libfreenect2::Frame::Depth];

			// Check frame dimensions
			if (depthFrame->width != width) {
				debugMessage("Kinect width is incorrect: actually "
					+ std::to_string(depthFrame->width), DEBUG_ERROR);
				width = depthFrame->width;
			}
			if (depthFrame->height != height) {
				debugMessage("Kinect height is incorrect: actually "
					+ std::to_string(depthFrame->height), DEBUG_ERROR);
				height = depthFrame->height;
			}

			// Copy frame
			KinectFrame frame(getWidth(), getHeight());
			frame.copyDataFromBuffer((kinect_t*)(depthFrame->data));
			listener->release(frameMap);

			debugMessage("Returning successful...", DEBUG_HIDDEN_INFO);
			return frame;
		}
		catch (...) {
			debugMessage("Returning failed...", DEBUG_HIDDEN_INFO);
			return BaseFrame();
		}
	}
};
