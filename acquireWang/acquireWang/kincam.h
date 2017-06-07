#pragma once
#pragma warning(push, 0)
#include <string>
#include "Kinect.h"
#include "comdef.h"
#pragma warning(pop)
#include "camera.h"
#include "frame.h"
#include "debug.h"

using namespace std;

typedef uint16_t kinect_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Kinect camera frame class, which derives from the
 * BaseFrame class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class KinectFrame : public BaseFrame {
public:
	// Constructor overrides
	KinectFrame(size_t _width, size_t _height) :
			BaseFrame(_width, _height, sizeof(kinect_t)) {}
	KinectFrame(size_t _width, size_t _height, kinect_t* _data, double _timestamp) :
			BaseFrame(_width, _height, sizeof(kinect_t), _data, _timestamp) {}
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

	void handleHRESULT(HRESULT hr, string whileDoing) {
		if (hr != S_OK) {
			_com_error err(hr);
			debugMessage("Kinect camera error "s + whileDoing + ": "s + err.ErrorMessage(), LEVEL_ERROR);
		}
	}
public:
	KinectCamera() {
		HRESULT hr;
		hr = GetDefaultKinectSensor(&kinectSensor);
		handleHRESULT(hr, "detecting Kinect"s);

		// Open sensor
		hr = kinectSensor->Open();
		handleHRESULT(hr, "opening Kinect"s);

		// Subscribe to frame callback
		hr = kinectSensor->OpenMultiSourceFrameReader(FrameSourceTypes::FrameSourceTypes_Depth, &frameReader);
		handleHRESULT(hr, "opening Kinect source"s);
		hr = frameReader->SubscribeMultiSourceFrameArrived(&frameEvent);
		handleHRESULT(hr, "subscribing to frame event"s);

		// Get frame size
		IDepthFrameSource* depthFrameSource;
		hr = kinectSensor->get_DepthFrameSource(&depthFrameSource);
		handleHRESULT(hr, "getting depth frame source"s);
		IFrameDescription* frameDescription;
		hr = depthFrameSource->get_FrameDescription(&frameDescription);
		handleHRESULT(hr, "getting depth frame descriptor"s);
		int _height, _width;
		hr = frameDescription->get_Height(&_height);
		handleHRESULT(hr, "getting depth frame height"s);
		hr = frameDescription->get_Width(&_width);
		handleHRESULT(hr, "getting depth frame width"s);
		height = (size_t)_height;
		width = (size_t)_width;

		channels = 1;

		frameDescription->Release();

		fps = 30;
	}

	void finalize() {
		HRESULT hr = kinectSensor->Close();
		handleHRESULT(hr, "closing Kinect sensor"s);
	}

	virtual pair<bool, BaseFrame> getFrame() {
		try {
			HRESULT hr;

			UINT16* depthBuffer;
			UINT depthBufferSize;

			// Loop until successful frame
			while (true) {
				// Wait for frame
				DWORD wait_timeout = 100; // ms (DWORD = uint32)
				while (true) {
					unsigned long event_id = WaitForSingleObject(reinterpret_cast<HANDLE>(frameEvent), wait_timeout);
					if (event_id != WAIT_TIMEOUT)
						break;
				}

				// Get frame
				IMultiSourceFrameArrivedEventArgs* frameArgs;
				hr = frameReader->GetMultiSourceFrameArrivedEventData(frameEvent, &frameArgs);
				handleHRESULT(hr, "getting Kinect frame event data"s);
				IMultiSourceFrameReference* frameReference;
				hr = frameArgs->get_FrameReference(&frameReference);
				handleHRESULT(hr, "getting Kinect source frame reference"s);
				IMultiSourceFrame* frame;
				hr = frameReference->AcquireFrame(&frame);
				handleHRESULT(hr, "acquiring depth source frame"s);
				IDepthFrameReference* depthFrameReference;
				hr = frame->get_DepthFrameReference(&depthFrameReference);
				handleHRESULT(hr, "getting depth frame reference"s);
				IDepthFrame* depthFrame;
				hr = depthFrameReference->AcquireFrame(&depthFrame);
				handleHRESULT(hr, "getting depth frame"s);

				// Release handles
				frameReference->Release();
				depthFrameReference->Release();
				frameArgs->Release();

				if (hr == S_OK) {
					// Get frame
					hr = depthFrame->AccessUnderlyingBuffer(&depthBufferSize, &depthBuffer);
					handleHRESULT(hr, "getting depth frame data"s);

					depthFrame->Release();

					break;
				}
			}

			// Copy frame
			KinectFrame copiedFrame(getWidth(), getHeight());
			copiedFrame.copyDataFromBuffer(depthBuffer);

			return std::make_pair(true, copiedFrame);
		}
		catch (...) {
			return std::make_pair(false, BaseFrame());
		}
	}
};
