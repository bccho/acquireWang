#pragma once
//#pragma warning(push, 0)
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "basecamera.h"
//#pragma warning(pop)

using namespace std;
typedef uint8_t pointgrey_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class implements the Point Grey camera class, which derives from the
 * BaseCamera class. This requires the Point Grey context to be managed
 * outside the class.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class PointGreyCamera : public BaseCamera<pointgrey_t> {
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
	PointGreyCamera(Spinnaker::CameraPtr& _pCam) : pCam(_pCam) {}

	void initialize() {
		pCam->Init();
		// cout << "PG camera initialized" << endl;
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

	virtual pair<bool, void*> getFrame() {
		try {
			int res = ensureOK(true);
			if (res < 0) {
				cout << "Point Grey camera is not cooperating. Error message " << res << endl;
				return make_pair(false, nullptr);
			}

			// Pull frame
			Spinnaker::ImagePtr pNewFrame = pCam->GetNextImage();
			if (pNewFrame->IsIncomplete()) {
				cout << "Image incomplete with image status " << pNewFrame->GetImageStatus() << endl;
			}
			// Perform a deep copy and ensure each pixel is 1 byte
			Spinnaker::ImagePtr copiedFrame = pNewFrame->Convert(Spinnaker::PixelFormat_Mono8, Spinnaker::HQ_LINEAR);

			// Copy again in case copiedFrame deletes data when it goes out of scope
			void* copied = new uint8_t[getFrameSize()];
			memcpy(copied, copiedFrame->GetData(), getFrameSize());

			// Release image
			pNewFrame->Release();

			return make_pair(true, copied);
		}
		catch (...) {
			return make_pair(false, nullptr);
		}
	}

	double getExposure() {
		ensureOK(false);
		return pCam->ExposureTime.GetValue();
	}
};
