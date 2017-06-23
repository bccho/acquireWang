#pragma once
#pragma warning(push, 0)
#include <thread>
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
	Spinnaker::System* sys;
	//Spinnaker::CameraList* camlist;
	std::string serial;
	Spinnaker::Camera* pCam;

	void getCamFromSerial() {
		//pCam = camlist->GetBySerial(serial);
		try {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			debugMessage("Getting camera list...", DEBUG_INFO);
			Spinnaker::CameraList camlist = sys->GetCameras();
			debugMessage(std::to_string(camlist.GetSize()) + " cameras.", DEBUG_INFO);
			debugMessage("Getting camera by serial...", DEBUG_INFO);
			Spinnaker::Camera* newCam = camlist.GetBySerial(serial);
			if (newCam != nullptr) {
				if (newCam->IsValid()) {
					debugMessage("Setting pCam...", DEBUG_INFO);
					pCam = newCam;
				}
			}
			debugMessage("Clearing camlist...", DEBUG_INFO);
			camlist.Clear();
		}
		catch (...) {
			debugMessage("Getting camera from device serial failed!", DEBUG_ERROR);
		}
		debugMessage("Returning from getCamFromSerial()...", DEBUG_INFO);
	}

	int ensureReady(bool ensureAcquiring) {
		/* Returns negative values if not; returns 0 if yes */
		int result = -1; // somehow pCam variable access fails... should never return -1
		try {
			// Check pointer valid
			if (pCam == nullptr) { // invalid pointer
				debugMessage("pCam == nullptr", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				getCamFromSerial();
			}
			result = -3; // pCam pointer dereference failed
			if (!pCam->IsValid()) { // invalid pointer
				debugMessage("pCam is not valid", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				getCamFromSerial();
			}

			result = -3; // pCam pointer dereference failed
			// Check initialized and acquiring; if not, try to fix it
			if (!pCam->IsInitialized()) {
				debugMessage("pCam is not initialized", DEBUG_ERROR);
				result = -4; // initialize function call failed
				initialize();
			}
			result = -3; // pCam pointer dereference failed
			if (ensureAcquiring && !pCam->IsStreaming()) {
				debugMessage("pCam is not acquiring", DEBUG_ERROR);
				result = -5; // begin acquisition function call failed
				pCam->BeginAcquisition();
			}

			// Check again and return
			result = -3; // pCam pointer dereference failed
			if (pCam == nullptr) {
				debugMessage("pCam still nullptr", DEBUG_ERROR);
				result = -6; // still invalid pointer
			}
			else if (!pCam->IsValid()) {
				debugMessage("pCam still not valid", DEBUG_ERROR);
				result = -7; // still not valid
			}
			else if (!pCam->IsInitialized()) {
				debugMessage("pCam still not initialized", DEBUG_ERROR);
				result = -8; // still not initialized
			}
			else if (ensureAcquiring && !pCam->IsStreaming()) {
				debugMessage("Still not acquiring", DEBUG_ERROR);
				result -9; // still not streaming
			}
			result = 0; // all OK
		}
		catch (...) {
			return result;
		}
		return result;
	}
public:
	//PointGreyCamera(Spinnaker::CameraList* _camlist, std::string _serial) :
			//camlist(_camlist), serial(_serial) {
	PointGreyCamera(Spinnaker::System* _sys, std::string _serial) :
			sys(_sys), serial(_serial) {
		debugMessage("PG Camera constructor", DEBUG_HIDDEN_INFO);
		getCamFromSerial();
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
		if (pCam->IsInitialized()) {
			pCam->DeInit();
		}
	}

	void beginAcquisition() override {
		debugMessage("Beginning acquisition PG camera", DEBUG_HIDDEN_INFO);
		try {
			while (ensureReady(false) < 0) {};
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
