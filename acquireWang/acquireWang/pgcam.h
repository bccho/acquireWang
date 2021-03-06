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
	std::string serial;
	Spinnaker::Camera* pCam;
	bool triggeredAcquisition;

	uint64_t init_timestamp_ns;
	double init_timestamp_win;

	void getCamFromSerial() {
		//debugMessage("Getting camera list...", DEBUG_INFO);
		Spinnaker::CameraList camlist = sys->GetCameras();
		try {
			debugMessage("Getting PG camera by serial " + serial, DEBUG_INFO);
			debugMessage("  " + std::to_string(camlist.GetSize()) + " PG cameras detected", DEBUG_INFO);
			Spinnaker::Camera* newCam = camlist.GetBySerial(serial);
			if (newCam != nullptr) {
				if (newCam->IsValid()) {
					debugMessage("  Found. Setting pCam...", DEBUG_INFO);
					pCam = newCam;
				}
			}
		}
		catch (...) {
			debugMessage("  Error while getting PG camera from device serial!", DEBUG_ERROR);
		}
		camlist.Clear();
		//debugMessage("Clearing camlist...", DEBUG_INFO);
		debugMessage("  Done.", DEBUG_INFO);
	}

	int isReady(bool checkAcquiring) {
		/* Returns negative values if not; returns 0 if yes */
		int result = -1; // somehow pCam variable access fails... should never return -1
		try {
			// Check pointer valid
			while (pCam == nullptr) { // invalid pointer
				debugMessage("pCam == nullptr", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				return result;
			}
			result = -3; // pCam pointer dereference failed
			while (!pCam->IsValid()) { // invalid pointer
				debugMessage("pCam is not valid", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				return result;
			}

			result = -3; // pCam pointer dereference failed
			// Check initialized and acquiring; if not, try to fix it
			if (!pCam->IsInitialized()) {
				debugMessage("pCam is not initialized", DEBUG_ERROR);
				result = -4; // initialize function call failed
				return result;
			}
			result = -3; // pCam pointer dereference failed
			if (checkAcquiring && !pCam->IsStreaming()) {
				debugMessage("pCam is not acquiring", DEBUG_ERROR);
				result = -5; // begin acquisition function call failed
				return result;
			}
			result = 0; // all OK
		}
		catch (...) {
			return result;
		}
		return result;
	}

	int ensureReady(bool ensureAcquiring) {
		/* Returns negative values if not; returns 0 if yes */
		int result = -1; // somehow pCam variable access fails... should never return -1
		try {
			// Check pointer valid
			while (pCam == nullptr) { // invalid pointer
				debugMessage("pCam == nullptr", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				getCamFromSerial();
			}
			result = -3; // pCam pointer dereference failed
			while (!pCam->IsValid()) { // invalid pointer
				debugMessage("pCam is not valid", DEBUG_ERROR);
				result = -2; // getting camera from camlist failed
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				getCamFromSerial();
			}

			result = -3; // pCam pointer dereference failed
			// Check initialized and acquiring; if not, try to fix it
			if (!pCam->IsInitialized()) {
				debugMessage("pCam is not initialized", DEBUG_ERROR);
				result = -4; // initialize function call failed
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				initialize();
			}
			result = -3; // pCam pointer dereference failed
			if (ensureAcquiring && !pCam->IsStreaming()) {
				debugMessage("pCam is not acquiring", DEBUG_ERROR);
				result = -5; // begin acquisition function call failed
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
	PointGreyCamera(Spinnaker::System* _sys, Spinnaker::Camera* _pCam, bool _triggeredAcquisition) :
			sys(_sys), pCam(_pCam), triggeredAcquisition(_triggeredAcquisition) {
		debugMessage("PG Camera constructor", DEBUG_HIDDEN_INFO);
		channels = 1;
		camType = CAMERA_PG;
		bytesPerPixel = sizeof(pointgrey_t);
		// Get serial number
		Spinnaker::GenApi::INodeMap& tldnmap = pCam->GetTLDeviceNodeMap();
		Spinnaker::GenApi::CStringPtr node = tldnmap.GetNode("DeviceSerialNumber");
		serial = node->GetValue();
	}
	~PointGreyCamera() override {
		debugMessage("~PointGreyCamera", DEBUG_HIDDEN_INFO);
	}

	void initialize() override {
		debugMessage("Initializing PG camera", DEBUG_HIDDEN_INFO);
		try {
			pCam->Init();
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			width = pCam->Width.GetValue();
			height = pCam->Height.GetValue();
			fps = pCam->AcquisitionFrameRate.GetValue();

			// For aligning Point Grey timestamps with Windows timestamps
			//pCam->TimestampReset.Execute();
			//debugMessage("Init timestamp: " + std::to_string(pCam->Timestamp.GetValue()), DEBUG_INFO);
			init_timestamp_ns = pCam->Timestamp.GetValue();
			init_timestamp_win = getClockStamp();
		}
		catch (...) {
			debugMessage("Error while initializing PG. Trying again...", DEBUG_ERROR);
			initialize();
		}
	}

	void finalize() override {
		debugMessage("Finalizing PG camera", DEBUG_HIDDEN_INFO);
		try {
			while (ensureReady(false) < 0) {}
			if (pCam->IsStreaming()) {
				pCam->EndAcquisition();
			}
			if (pCam->IsInitialized()) {
				pCam->DeInit();
			}
		}
		catch (...) {
			debugMessage("Error while finalizing PG. Trying again...", DEBUG_ERROR);
			finalize();
		}
	}

	void beginAcquisition() override {
		if (triggeredAcquisition) { return; } // Ignore if triggered acquisition

		debugMessage("Beginning acquisition PG camera", DEBUG_HIDDEN_INFO);
		try {
			while (ensureReady(false) < 0) {}
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
		try {
			while (ensureReady(false) < 0) {}
			if (pCam->IsStreaming()) {
				pCam->EndAcquisition();
			}
		}
		catch (...) {
			debugMessage("Error while ending PG acquisition. Trying again...", DEBUG_ERROR);
			endAcquisition();
		}
	}

	BaseFrame getFrame() override {
		debugMessage("pg getFrame", DEBUG_HIDDEN_INFO);
		try {
			int res = ensureReady(true);
			//int res = isReady(true);
			if (res < 0) {
				debugMessage("PG camera is not ready. Error message " + std::to_string(res), DEBUG_ERROR);
				return BaseFrame();
			}

			// Pull frame
			Spinnaker::ImagePtr pNewFrame = pCam->GetNextImage();
			if (pNewFrame == nullptr) {
				return BaseFrame();
			}
			if (pNewFrame->IsIncomplete()) {
				debugMessage("PG image incomplete with image status " + std::to_string(pNewFrame->GetImageStatus()), DEBUG_ERROR);
				return BaseFrame();
			}
			// Perform a deep copy and ensure each pixel is 1 byte
			Spinnaker::ImagePtr pgBuffer = pNewFrame->Convert(Spinnaker::PixelFormat_Mono8, Spinnaker::HQ_LINEAR);

			// Copy again in case pgBuffer deletes data when it goes out of scope
			PointGreyFrame frame(getWidth(), getHeight());
			frame.copyDataFromBuffer((pointgrey_t*) pgBuffer->GetData());

			// Set timestamp TODO: more precise? Also kinect
			//uint64_t nowTime = pNewFrame->GetTimeStamp();
			//debugMessage(std::to_string(nowTime) + " - " + std::to_string(init_timestamp_ns) + " = " +
			//	std::to_string((double)(nowTime - init_timestamp_ns)), DEBUG_INFO);
			//double newTimestamp = (double)(nowTime - init_timestamp_ns) / 1000000000.0 + init_timestamp_win;
			double newTimestamp = getClockStamp();
			frame.setTimestamp(newTimestamp);

			// Release image
			pNewFrame->Release();

			totalFrames++;
			return frame;
		}
		catch (...) {
			return BaseFrame();
		}
	}

	// TODO: don't let acquisition start until all cameras ready
	bool isReady() override {
		if (pCam == nullptr) return false;
		if (!pCam->IsValid()) return false;
		if (!pCam->IsInitialized()) return false;
		if (!pCam->IsStreaming()) return false;
		return totalFrames > 0;
	}

	std::string getSerial() {
		return serial;
	}

	double getExposure() {
		ensureReady(false);
		return pCam->ExposureTime.GetValue();
	}

	double getGain() {
		ensureReady(false);
		return pCam->Gain.GetValue();
	}

	double getTemperature() {
		ensureReady(false);
		Spinnaker::GenApi::INodeMap& nmap = pCam->GetNodeMap();
		Spinnaker::GenApi::CFloatPtr tempNode = nmap.GetNode("DeviceTemperature");
		return tempNode->GetValue();
	}
};
