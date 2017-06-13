#pragma once
#pragma warning(push, 0)
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#pragma warning(pop)
#include "camera.h"
#include "debug.h"

typedef uint8_t pointgrey_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Point Grey camera frame class, which derives
 * from the BaseFrame class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class PointGreyFrame : public BaseFrame {
public:
	// Constructor overloads
	PointGreyFrame(size_t _width, size_t _height) :
			BaseFrame(_width, _height, sizeof(pointgrey_t), 1) {}
	PointGreyFrame(size_t _width, size_t _height, pointgrey_t* _data, double _timestamp) :
			BaseFrame(_width, _height, sizeof(pointgrey_t), 1, _data, _timestamp) {}
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
	Spinnaker::Camera* pCam;

	int ensureReady(bool ensureAcquiring) {
		/* Returns negative values if not; returns 0 if yes */
		try {
			// Check pointer valid
			if (pCam == nullptr) { // invalid pointer
				debugMessage("pCam = nullptr", DEBUG_HIDDEN_INFO);
				return -1;
			}
			if (!pCam->IsValid()) { // invalid pointer
				debugMessage("Not valid", DEBUG_HIDDEN_INFO);
				return -1;
			}

			// Check initialized and acquiring; if not, try to fix it
			if (!pCam->IsInitialized()) {
				debugMessage("Was not initialized", DEBUG_HIDDEN_INFO);
				initialize();
			}
			if (ensureAcquiring && !pCam->IsStreaming()) {
				debugMessage("Was not acquiring", DEBUG_HIDDEN_INFO);
				pCam->BeginAcquisition();
			}

			// Check again and return
			if (!pCam->IsInitialized()) {
				debugMessage("Still not initialized", DEBUG_ERROR);
				return -2; // still not initialized
			}
			if (ensureAcquiring && !pCam->IsStreaming()) {
				debugMessage("Still not acquiring", DEBUG_ERROR);
				return -3; // still not streaming
			}
			return 0;
		}
		catch (...) {
			return -4; // some other error
		}
	}
public:
	PointGreyCamera(Spinnaker::Camera* _pCam) : pCam(_pCam) {
		debugMessage("PG Camera constructor", DEBUG_HIDDEN_INFO);
		channels = 1;
		bytesPerPixel = sizeof(pointgrey_t);
	}
	~PointGreyCamera() override {
		debugMessage("~PointGreyCamera", DEBUG_HIDDEN_INFO);
	}

	void initialize() override {
		debugMessage("Initializing PG camera", DEBUG_HIDDEN_INFO);
		pCam->Init();
		width = pCam->Width.GetValue();
		height = pCam->Height.GetValue();
		fps = pCam->AcquisitionFrameRate.GetValue();
	}

	void finalize() override {
		debugMessage("Finalizing PG camera", DEBUG_HIDDEN_INFO);
		if (pCam->IsStreaming()) {
			pCam->EndAcquisition();
		}
	}

	void beginAcquisition() override {
		debugMessage("Beginning acquisition PG camera", DEBUG_HIDDEN_INFO);
		try {
			ensureReady(false);
			if (!pCam->IsStreaming()) {
				pCam->BeginAcquisition();
			}
		}
		catch (...) {
			debugMessage("Error while beginning PG acquisition. Trying again...", DEBUG_ERROR);
			beginAcquisition();
		}
	}

	void endAcquisition() override {
		debugMessage("Ending acquisition PG camera", DEBUG_HIDDEN_INFO);
		if (pCam->IsStreaming()) {
			pCam->EndAcquisition();
		}
	}

	BaseFrame getFrame() override {
		debugMessage("pg getFrame", DEBUG_HIDDEN_INFO);
		try {
			int res = ensureReady(true);
			if (res < 0) {
				debugMessage("PG camera is not ready. Error message " + std::to_string(res), DEBUG_ERROR);
				return BaseFrame();
			}

			// Pull frame
			Spinnaker::ImagePtr pNewFrame = pCam->GetNextImage();
			if (pNewFrame->IsIncomplete()) {
				debugMessage("PG image incomplete with image status " + std::to_string(pNewFrame->GetImageStatus()), DEBUG_ERROR);
			}
			// Perform a deep copy and ensure each pixel is 1 byte
			Spinnaker::ImagePtr pgBuffer = pNewFrame->Convert(Spinnaker::PixelFormat_Mono8, Spinnaker::HQ_LINEAR);

			// Copy again in case pgBuffer deletes data when it goes out of scope
			PointGreyFrame frame(getWidth(), getHeight());
			frame.copyDataFromBuffer((pointgrey_t*) pgBuffer->GetData());

			// Release image
			pNewFrame->Release();

			return frame;
		}
		catch (...) {
			return BaseFrame();
		}
	}

	double getExposure() {
		ensureReady(false);
		return pCam->ExposureTime.GetValue();
	}
};
