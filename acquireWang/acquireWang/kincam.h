#pragma once
#pragma warning(push, 0)
#include <string>
#include "Kinect.h"
#include "comdef.h"
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
	IKinectSensor* kinectSensor;
	IMultiSourceFrameReader* frameReader;

	WAITABLE_HANDLE frameEvent;

	bool valid; // true if camera is valid
	bool silent; // to silence error messages at beginning of kinect acquisition

	void handleHRESULT(HRESULT hr, std::string whileDoing) {
		if (hr != S_OK) {
			_com_error err(hr);
			if (!silent) {
				debugMessage("Kinect camera error while " + whileDoing + ": " + err.ErrorMessage(), DEBUG_ERROR);
			}
			throw "Kinect camera error while " + whileDoing + ": " + err.ErrorMessage();
		}
	}
public:
	KinectCamera() {
		valid = true;
		silent = false;
		try {
			HRESULT hr;

			hr = GetDefaultKinectSensor(&kinectSensor);
			handleHRESULT(hr, "detecting Kinect");

			// See if sensor available
			if (!kinectSensor) {
				debugMessage("No Kinect camera available", DEBUG_ERROR);
				throw "No Kinect camera available";
			}
			BOOLEAN isAvailable = true;
			hr = kinectSensor->get_IsAvailable(&isAvailable);
			if (FAILED(hr) || !isAvailable) {
				debugMessage("No Kinect camera available", DEBUG_ERROR);
				throw "No Kinect camera available";
			}

			// Open sensor
			hr = kinectSensor->Open();
			handleHRESULT(hr, "opening Kinect sensor");

			// Subscribe to frame callback
			hr = kinectSensor->OpenMultiSourceFrameReader(FrameSourceTypes::FrameSourceTypes_Depth, &frameReader);
			handleHRESULT(hr, "opening Kinect source");
			hr = frameReader->SubscribeMultiSourceFrameArrived(&frameEvent);
			handleHRESULT(hr, "subscribing to frame event");

			// Get frame size
			IDepthFrameSource* depthFrameSource;
			hr = kinectSensor->get_DepthFrameSource(&depthFrameSource);
			handleHRESULT(hr, "getting depth frame source");
			IFrameDescription* frameDescription;
			hr = depthFrameSource->get_FrameDescription(&frameDescription);
			handleHRESULT(hr, "getting depth frame descriptor");
			int _height, _width;
			hr = frameDescription->get_Height(&_height);
			handleHRESULT(hr, "getting depth frame height");
			hr = frameDescription->get_Width(&_width);
			handleHRESULT(hr, "getting depth frame width");
			height = (size_t)_height;
			width = (size_t)_width;

			frameDescription->Release();
			hr = kinectSensor->Close();
			handleHRESULT(hr, "closing Kinect sensor");

			bytesPerPixel = sizeof(kinect_t);
			channels = 1;
			fps = 30;
			camType = CAMERA_KINECT;
		}
		catch (...) {
			valid = false;
		}
	}
	~KinectCamera() override {
		debugMessage("~KinectCamera", DEBUG_HIDDEN_INFO);
	}

	bool isValid() { return valid; }

	void initialize() override {
		debugMessage("kinect initialize()", DEBUG_HIDDEN_INFO);
		HRESULT hr = kinectSensor->Open();
		handleHRESULT(hr, "opening Kinect sensor");
		silent = true;
	}

	void finalize() override {
		HRESULT hr = kinectSensor->Close();
		handleHRESULT(hr, "closing Kinect sensor");
	}

	BaseFrame getFrame() override {
		try {
			HRESULT hr;

			UINT16* depthBuffer;
			UINT depthBufferSize;

			// Wait for frame
			debugMessage("Waiting...", DEBUG_HIDDEN_INFO);
			DWORD wait_timeout = 100; // ms (DWORD = uint32)
			//while (true) {
			for (int i = 0; i < 10; i++) {
				unsigned long event_id = WaitForSingleObject(reinterpret_cast<HANDLE>(frameEvent), wait_timeout);
				if (event_id != WAIT_TIMEOUT)
					break;
			}

			// Get frame
			IMultiSourceFrameArrivedEventArgs* frameArgs;
			hr = frameReader->GetMultiSourceFrameArrivedEventData(frameEvent, &frameArgs);
			handleHRESULT(hr, "getting Kinect frame event data");
			IMultiSourceFrameReference* frameReference;
			hr = frameArgs->get_FrameReference(&frameReference);
			handleHRESULT(hr, "getting Kinect source frame reference");
			IMultiSourceFrame* frameRef;
			hr = frameReference->AcquireFrame(&frameRef);
			handleHRESULT(hr, "acquiring depth source frame");
			IDepthFrameReference* depthFrameReference;
			hr = frameRef->get_DepthFrameReference(&depthFrameReference);
			handleHRESULT(hr, "getting depth frame reference");
			IDepthFrame* depthFrame;
			hr = depthFrameReference->AcquireFrame(&depthFrame);
			handleHRESULT(hr, "getting depth frame");

			// Release handles
			frameReference->Release();
			depthFrameReference->Release();
			frameArgs->Release();

			// Get frame
			hr = depthFrame->AccessUnderlyingBuffer(&depthBufferSize, &depthBuffer);
			handleHRESULT(hr, "getting depth frame data");

			// Copy frame
			KinectFrame frame(getWidth(), getHeight());
			frame.copyDataFromBuffer((kinect_t*) depthBuffer);

			// Set timestamp
			double newTimestamp = getClockStamp(); // get timestamp when received
			frame.setTimestamp(newTimestamp);

			// Release depth frame
			depthFrame->Release();

			debugMessage("Returning successful...", DEBUG_HIDDEN_INFO);
			silent = false;
			return frame;
		}
		catch (...) {
			debugMessage("Returning failed...", DEBUG_HIDDEN_INFO);
			return BaseFrame();
		}
	}
};
