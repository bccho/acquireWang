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

	void handleHRESULT(HRESULT hr, std::string whileDoing) {
		if (hr != S_OK) {
			_com_error err(hr);
			//throw "Kinect camera error while "s + whileDoing + ": "s + err.ErrorMessage();
			debugMessage("Kinect camera error " + whileDoing + ": " + err.ErrorMessage(), LEVEL_ERROR);
		}
	}
public:
	KinectCamera() {
		HRESULT hr;
		hr = GetDefaultKinectSensor(&kinectSensor);
		handleHRESULT(hr, "detecting Kinect");

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

		channels = 1;

		frameDescription->Release();
		hr = kinectSensor->Close();
		handleHRESULT(hr, "closing Kinect sensor");

		fps = 30;
	}
	~KinectCamera() {
		debugMessage("~KinectCamera", LEVEL_INFO);
	}

	virtual void initialize() {
		HRESULT hr = kinectSensor->Open();
		handleHRESULT(hr, "opening Kinect sensor");
	}

	virtual void finalize() {
		HRESULT hr = kinectSensor->Close();
		handleHRESULT(hr, "closing Kinect sensor");
	}

	virtual std::pair<bool, BaseFrame> getFrame() {
		try {
			HRESULT hr;

			UINT16* depthBuffer;
			UINT depthBufferSize;

			// Loop until successful frame
			while (true) {
				// Wait for frame
				debugMessage("Waiting...", LEVEL_INFO);
				DWORD wait_timeout = 100; // ms (DWORD = uint32)
				while (true) {
					unsigned long event_id = WaitForSingleObject(reinterpret_cast<HANDLE>(frameEvent), wait_timeout);
					if (event_id != WAIT_TIMEOUT)
						break;
				}

				// Get frame
				IMultiSourceFrameArrivedEventArgs* frameArgs;
				hr = frameReader->GetMultiSourceFrameArrivedEventData(frameEvent, &frameArgs);
				handleHRESULT(hr, "getting Kinect frame event data");
				if (hr != S_OK) continue;
				IMultiSourceFrameReference* frameReference;
				hr = frameArgs->get_FrameReference(&frameReference);
				handleHRESULT(hr, "getting Kinect source frame reference");
				if (hr != S_OK) continue;
				IMultiSourceFrame* frame;
				hr = frameReference->AcquireFrame(&frame);
				handleHRESULT(hr, "acquiring depth source frame");
				if (hr != S_OK) continue;
				IDepthFrameReference* depthFrameReference;
				hr = frame->get_DepthFrameReference(&depthFrameReference);
				handleHRESULT(hr, "getting depth frame reference");
				if (hr != S_OK) continue;
				IDepthFrame* depthFrame;
				hr = depthFrameReference->AcquireFrame(&depthFrame);
				handleHRESULT(hr, "getting depth frame");
				if (hr != S_OK) continue;

				// Release handles
				frameReference->Release();
				depthFrameReference->Release();
				frameArgs->Release();

				if (hr == S_OK) {
					// Get frame
					hr = depthFrame->AccessUnderlyingBuffer(&depthBufferSize, &depthBuffer);
					handleHRESULT(hr, "getting depth frame data");
					if (hr != S_OK) continue;

					depthFrame->Release();

					break;
				}
			}

			// Copy frame
			KinectFrame copiedFrame(getWidth(), getHeight());
			copiedFrame.copyDataFromBuffer(depthBuffer);

			debugMessage("Returning successful...", LEVEL_INFO);
			return std::make_pair(true, copiedFrame);
		}
		catch (...) {
			debugMessage("Returning failed...", LEVEL_INFO);
			return std::make_pair(false, BaseFrame());
		}
	}
};
