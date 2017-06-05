#pragma once
#include <string>
#include "Kinect.h"
#include "comdef.h"
#include "basecamera.h"

using namespace std;

typedef uint16_t kinect_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Kinect camera class, which derives from the
 * BaseCamera class. The current implementation (using the default Windows
 * driver) does not permit multiple Kinect cameras, and currently only returns
 * the depth stream.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class KinectCamera : public BaseCamera<uint16_t> {
private:
	IKinectSensor* kinectSensor;
	IMultiSourceFrameReader* frameReader;

	WAITABLE_HANDLE frameEvent;

	void handleHRESULT(HRESULT hr, string& whileDoing) {
		if (hr != S_OK) {
			_com_error err(hr);
			if (DEBUGGING) {
				printf("[!] Error %s: %s\n", whileDoing.c_str(), err.ErrorMessage());
			}
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

		frameDescription->Release();

		fps = 30;
	}

	void finalize() {
		HRESULT hr = kinectSensor->Close();
		handleHRESULT(hr, "closing Kinect sensor"s);
	}

	virtual pair<bool, void*> getFrame() {
		try {
			HRESULT hr;

			UINT16* depthBuffer;
			UINT depthBufferSize;

			// Loop until successful frame
			while (true) {
				// Wait for frame
				DWORD wait_timeout = 100; // ms (DWORD = uint32)
				while (true) {
					int event_id = WaitForSingleObject(reinterpret_cast<HANDLE>(frameEvent), wait_timeout);
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
			void* copiedFrame = new uint16_t[getFrameSize()];
			memcpy(copiedFrame, depthBuffer, depthBufferSize * sizeof(uint16_t));

			return make_pair(true, copiedFrame);
		}
		catch (...) {
			return make_pair(false, nullptr);
		}
	}
};
