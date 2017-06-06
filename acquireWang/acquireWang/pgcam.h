#pragma once
#pragma warning(push, 0)
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#pragma warning(pop)
#include "camera.h"
#include "debug.h"

using namespace std;
typedef uint8_t pointgrey_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Point Grey camera frame class, which derives
 * from the BaseFrame class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class PointGreyFrame : public BaseFrame {
public:
	// Constructor overloads
	PointGreyFrame(size_t _width, size_t _height) :
			BaseFrame(_width, _height, sizeof(pointgrey_t)) {}
	PointGreyFrame(size_t _width, size_t _height, pointgrey_t* _data, double _timestamp) :
			BaseFrame(_width, _height, sizeof(pointgrey_t), _data, _timestamp) {}
	// Method overloads
	void copyDataFromBuffer(pointgrey_t* buffer) {
		BaseFrame::copyDataFromBuffer(buffer);
	}
	void copyDataToBuffer(pointgrey_t* buffer) {
		BaseFrame::copyDataToBuffer(buffer);
	}
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Point Grey camera class, which derives from the
 * BaseCamera class. This requires the Point Grey context to be managed
 * outside the class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class PointGreyCamera : public BaseCamera {
private:
	Spinnaker::CameraPtr pCam;

	int ensureOK(bool ensureAcquiring) {
		/* Returns negative values if not; returns 0 if yes */
		try {
			// Check pointer valid
			if (!pCam->IsValid())
				return -1; // invalid pointer

						   // Check initialized and acquiring; if not, try to fix it
			if (!pCam->IsInitialized()) {
				initialize();
			}
			if (ensureAcquiring && !pCam->IsStreaming()) {
				pCam->BeginAcquisition();
			}

			// Check again and return
			if (!pCam->IsInitialized())
				return -2; // still not initialized
			if (ensureAcquiring && !pCam->IsStreaming())
				return -3; // still not streaming
			return 0;
		}
		catch (...) {
			return -4; // some other error
		}
	}
public:
	PointGreyCamera(Spinnaker::CameraPtr& _pCam) : pCam(_pCam) { channels = 1; }

	void initialize() {
		pCam->Init();
		debugMessage("PG camera initialized"s, LEVEL_INFO);
		width = pCam->Width.GetValue();
		height = pCam->Height.GetValue();
		fps = pCam->AcquisitionFrameRate.GetValue();
	}

	void finalize() {
		ensureOK(false);
		if (pCam->IsStreaming()) {
			pCam->EndAcquisition();
		}
		pCam = NULL;
	}

	void beginAcquisition() {
		ensureOK(false);
		pCam->BeginAcquisition();
	}

	void endAcquisition() {
		pCam->EndAcquisition();
	}

	virtual pair<bool, BaseFrame> getFrame() {
		try {
			int res = ensureOK(true);
			if (res < 0) {
				debugMessage("PG camera is not cooperating. Error message "s + std::to_string(res), LEVEL_ERROR);
				return std::make_pair(false, BaseFrame());
			}

			// Pull frame
			Spinnaker::ImagePtr pNewFrame = pCam->GetNextImage();
			if (pNewFrame->IsIncomplete()) {
				debugMessage("PG image incomplete with image status "s + std::to_string(pNewFrame->GetImageStatus()), LEVEL_ERROR);
			}
			// Perform a deep copy and ensure each pixel is 1 byte
			Spinnaker::ImagePtr pgBuffer = pNewFrame->Convert(Spinnaker::PixelFormat_Mono8, Spinnaker::HQ_LINEAR);

			// Copy again in case copiedFrame deletes data when it goes out of scope
			PointGreyFrame copiedFrame(getWidth(), getHeight());
			copiedFrame.copyDataFromBuffer((pointgrey_t*) pgBuffer->GetData());

			// Release image
			pNewFrame->Release();

			return std::make_pair(true, copiedFrame);
		}
		catch (...) {
			return std::make_pair(false, BaseFrame());
		}
	}

	double getExposure() {
		ensureOK(false);
		return pCam->ExposureTime.GetValue();
	}
};
