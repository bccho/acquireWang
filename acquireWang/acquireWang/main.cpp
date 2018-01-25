// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

#pragma warning(push, 0)
// Fundamentals
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <chrono> // Timing

// Externals
#include "H5Cpp.h" // HDF5
#include "serial.h"
#pragma warning(pop)

// Other unit files
#include "acquirer.h"
#include "kincam.h"
#include "pgcam.h"
#include "h5out.h"
#include "previewwindow.h"
#include "debug.h"
#include "utils.h"

/* Global variables */
size_t frameChunkSize;
json config;

// For threads
std::atomic<bool> stopSerialLoop;

// Cameras
std::vector<BaseCamera*> cameras;
std::vector<std::string> camnames;
std::vector<format> formats;
std::vector<PredType> dtypes;
std::vector<DSetCreatPropList> dcpls;

/* Methods */
bool sendSerialTrigger(Serial* serial) {
	if (!serial->IsConnected()) return false;
	return serial->WriteData("T", 1);
}

// Serial thread loop
void serialLoop(Serial* serial, std::string filename) {
	// Preallocate memory
	char incomingData[1 << 10] = "";
	int dataLength = 1 << 10 - 1;
	int readResult = 0;

	// Open CSV file
	std::ofstream csvFile;
	csvFile.open(filename);

	// Serial read loop
	while (serial->IsConnected()) {
		// Break if needed
		if (stopSerialLoop.load()) break;

		readResult = serial->ReadData(incomingData, dataLength);
		// printf("Bytes read: (0 means no data available) %i\n",readResult);
		incomingData[readResult] = 0;

		//printf("%s", incomingData);
		//csvFile << incomingData;
		csvFile.write(incomingData, readResult);
		csvFile.flush();
	}
	csvFile.close();
}

// Recording session
int record(std::string& saveTitle, double duration, bool triggeredAcquisition) {
	/* Start serial */
	debugMessage("Searching for serial connection", DEBUG_INFO);
	Serial* serial = new Serial(config["DAQ_port"].get<std::string>().c_str(), CBR_256000);
	if (serial->IsConnected()) {
		debugMessage("  Connection established", DEBUG_INFO);
	}
	else {
		debugMessage("  Unable to establish connection", DEBUG_INFO);
	}

	/* Prepare acquirers */
	debugMessage(std::to_string(cameras.size()) + " cameras", DEBUG_HIDDEN_INFO);
	std::vector<BaseAcquirer*> acquirers;
	for (size_t i = 0; i < cameras.size(); i++) {
		// Make new acquirer
		acquirers.push_back(new BaseAcquirer(camnames[i], *cameras[i]));
	}

	/* Prepare HDF5 saver */
	// Check if file exists
	if (fileExists(saveTitle + ".h5")) {
		debugMessage("File already exists. Overwriting...", DEBUG_WARNING);
	}
	// Set up file access property list
	H5::FileAccPropList fapl;
	fapl.setCache(65536000, (size_t)config["_rdcc_nslots"], (size_t)config["_rdcc_nbytes"], 0);
	// Create saving object
	H5Out* h5out = new H5Out(saveTitle + ".h5", acquirers, frameChunkSize, camnames, dtypes,
		H5::FileCreatPropList::DEFAULT, fapl, dcpls);

	/* Print camera parameters */
	debugMessage("Camera parameters:", DEBUG_INFO);
	for (size_t i = 0; i < cameras.size(); i++) {
		debugMessage("  " + camnames[i] + ":", DEBUG_INFO);
		debugMessage("    Frame rate (fps) = " + std::to_string(cameras[i]->getFPS()), DEBUG_INFO);
		if (cameras[i]->getCamType() == CAMERA_PG) {
			PointGreyCamera* pCam = dynamic_cast<PointGreyCamera*>(cameras[i]);
			if (pCam != nullptr) {
				debugMessage("    Exposure (us) = " + std::to_string(pCam->getExposure()), DEBUG_INFO);
				debugMessage("    Gain (dB) = " + std::to_string(pCam->getGain()), DEBUG_INFO);
				debugMessage("    Temperature (C) = " + std::to_string(pCam->getTemperature()), DEBUG_INFO);
				debugMessage("    Serial = " + pCam->getSerial(), DEBUG_INFO);
			}
		}
	}
	/* Set up frame counts */
	for (size_t i = 0; i < cameras.size(); i++) {
		size_t totalFrames = round(duration * 60.0 * cameras[i]->getFPS());
		acquirers[i]->setFramesToAcquire(totalFrames);
	}

	/* Start */
	// Prepare GUI
	PreviewWindow preview(960, 720, "Wang Lab behavior acquisition tool (press Q to stop acquisition)",
		acquirers, *h5out, cameras, formats);
	// Start acquisition
	timers.pause(DTIMER_PREP);
	timers.start(DTIMER_ACQUISITION);
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->run();
		acquirers[i]->beginAcquisition();
	}
	// Start serial thread
	stopSerialLoop = false;
	std::thread* serialThread = nullptr;
	if (serial->IsConnected()) {
		serialThread = new std::thread(serialLoop, serial, saveTitle + "_daq.csv");
	}
	if (triggeredAcquisition) {
		debugMessage("Press any key to trigger cameras.", DEBUG_MUST_SHOW);
		std::cin.ignore();
		sendSerialTrigger(serial);
		debugMessage("Trigger signal sent.", DEBUG_INFO);
	}
	else {
		// Wait for cameras to be ready
		debugMessage("Waiting for cameras to be ready...", DEBUG_INFO);
		for (size_t i = 0; i < cameras.size(); i++) {
			while (!cameras[i]->isReady()) {}
		}
	}
	// Start GUI
	preview.run();

	/* Stop */
	// End acquisition
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->endAcquisition();
	}
	timers.pause(DTIMER_ACQUISITION);
	timers.start(DTIMER_CLEANUP);

	// Stop acquiring and saving (i.e. wait for threads to end)
	// (This will block until acquisition and saving threads are joined)
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->abortAcquisition();
	}
	// Stop serial
	stopSerialLoop = true;
	if (serialThread != nullptr) {
		serialThread->join();
		delete serialThread;
		serialThread = nullptr;
	}

	// Stop saving but keep saving acquired frames
	h5out->abortSaving(false); // wait for thread to be joined

	// Write metadata
	for (size_t i = 0; i < acquirers.size(); i++) {
		h5out->writeScalarAttribute(acquirers[i]->getName() + "_fps", cameras[i]->getFPS());
		if (acquirers[i]->getCamType() == CAMERA_PG) { // Point-Grey specific metadata
			PointGreyCamera* pCam = dynamic_cast<PointGreyCamera*>(cameras[i]);
			if (pCam != nullptr) {
				h5out->writeScalarAttribute(acquirers[i]->getName() + "_serial", pCam->getSerial());
				h5out->writeScalarAttribute(acquirers[i]->getName() + "_exposure", pCam->getExposure());
				h5out->writeScalarAttribute(acquirers[i]->getName() + "_gain", pCam->getGain());
			}
		}
	}
	h5out->writeScalarAttribute("deflate", (int)config["_compression"]);

	// Finalize
	delete h5out;
	for (size_t i = 0; i < cameras.size(); i++) {
		delete acquirers[i];
	}
	debugMessage("Exiting recording method", DEBUG_HIDDEN_INFO);
	return EXIT_SUCCESS;
}

// Main method
int main(int argc, char* argv[]) {
	// Memory leak detection
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	/* Parse input arguments */
	double recordingDuration(0); // minutes
	bool fixedlen = true;
	if (argc < 2) {
		debugMessage("Usage:", DEBUG_MUST_SHOW);
		debugMessage("    acquireWang.exe filename [numMinutes = 0]", DEBUG_MUST_SHOW);
		exit(EXIT_FAILURE);
	}
	else if (argc == 2) { // if numMinutes not specified, run without fixed length
		fixedlen = false;
	}
	else {
		recordingDuration = atof(argv[2]);
	}
	std::string saveTitle = std::string(argv[1]);

	/* Read configuration file */
	config = readConfig();

	frameChunkSize = (size_t)config["_frameChunkSize"];

	/* Set up HDF5 DCPLs */
	// Set up dataset creation property lists
	H5::DSetCreatPropList kin_dcpl;
	const int frame_ndims = 4;
	size_t kin_chunk_dims[frame_ndims] = { frameChunkSize, 1, (size_t)config["_kinectYchunk"], (size_t)config["_kinectXchunk"] };
	kin_dcpl.setChunk(frame_ndims, kin_chunk_dims);
	H5::DSetCreatPropList pg_dcpl;
	size_t pg_chunk_dims[frame_ndims] = { frameChunkSize, 1, (size_t)config["_pgYchunk"], (size_t)config["_pgXchunk"] };
	pg_dcpl.setChunk(frame_ndims, pg_chunk_dims);
	if ((int)config["_compression"] > 0) { // GZIP compression
		kin_dcpl.setDeflate((int)config["_compression"]);
		pg_dcpl.setDeflate((int)config["_compression"]);
	}
	// Enable shuffle filter
	kin_dcpl.setShuffle();
	pg_dcpl.setShuffle();
	// Enable the LZ4 filter
	const int H5Z_FILTER_LZ4 = 32004;
	const unsigned int lz4_params[1] = { (unsigned int)config["_lz4_block_size"] }; // block size in bytes (default = 1<<30 == 1.0 GB)
	kin_dcpl.setFilter(H5Z_FILTER_LZ4, H5Z_FLAG_MANDATORY, 1, lz4_params);
	pg_dcpl.setFilter(H5Z_FILTER_LZ4, H5Z_FLAG_MANDATORY, 1, lz4_params);

	/* Set up cameras */
	// Initialize Point Grey system
	Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
	Spinnaker::CameraList camList = system->GetCameras();
	int numPGcameras = camList.GetSize();
	debugMessage("Connected Point Grey devices: " + std::to_string(numPGcameras), DEBUG_INFO);

	// Triggered acquisition?
	bool triggeredAcquisition = false;
	json::iterator item = config.find("trigger_acquisition");
	if (item != config.end()) {
		std::string val = item.value().get<std::string>();
		std::transform(val.begin(), val.end(), val.begin(), ::toupper);
		if (val == "TRUE" || val == "YES" || val == "ON" || val == "Y" || val == "T") {
			triggeredAcquisition = true;
			debugMessage("Config: setting trigger for acquisition start", DEBUG_INFO);
		}
	}

	// Set up Kinect camera
	// TODO: make a class to hold camnames, dtypes, etc.
	KinectCamera* kincam = new KinectCamera;
	if (kincam->isValid()) {
		debugMessage("Found valid Kinect camera", DEBUG_INFO);
		cameras.push_back(kincam);
		camnames.push_back("kinect");
		formats.push_back(DEPTH_16BIT);
		dtypes.push_back(KINECT_H5T);
		dcpls.push_back(kin_dcpl);
	}

	// Set up Point Grey cameras
	for (int i = 0; i < numPGcameras; i++) {
		Spinnaker::CameraPtr pCam = camList.GetByIndex(i);
		pCam->Init();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		// Get serial number
		Spinnaker::GenApi::INodeMap& tldnmap = pCam->GetTLDeviceNodeMap();
		Spinnaker::GenApi::CStringPtr node = tldnmap.GetNode("DeviceSerialNumber");
		std::string serial = node->GetValue();
		// If config file with this serial number exists, apply settings
		std::string pg_config_filename = "pg" + serial + ".json";
		if (fileExists(pg_config_filename)) {
			debugMessage("Point Grey configuration file found: " + pg_config_filename, DEBUG_INFO);
			json pg_config = readJSON(pg_config_filename);
			// Exposure
			json::iterator item = pg_config.find("exposure");
			if (item != pg_config.end()) {
				double val = item.value().get<double>();
				pCam->ExposureAuto.SetValue(Spinnaker::ExposureAuto_Off); // turn off auto-exposure
				pCam->ExposureTime.SetValue(val);
				debugMessage("    Set exposure = " + std::to_string(val), DEBUG_INFO);
			}
			// Gain
			item = pg_config.find("gain");
			if (item != pg_config.end()) {
				double val = item.value().get<double>();
				pCam->GainAuto.SetValue(Spinnaker::GainAuto_Off); // turn off auto-gain
				pCam->Gain.SetValue(val);
				debugMessage("    Set gain = " + std::to_string(val), DEBUG_INFO);
			}
			// Frame rate
			item = pg_config.find("fps");
			if (item != pg_config.end()) {
				double val = item.value().get<double>();
				pCam->AcquisitionFrameRate.SetValue(val);
				debugMessage("    Set frame rate = " + std::to_string(val), DEBUG_INFO);
			}
			// Triggered acquisition?
			if (triggeredAcquisition) {
				// Configure line 0
				pCam->LineSelector.SetValue(Spinnaker::LineSelector_Line0);
				pCam->LineMode.SetValue(Spinnaker::LineMode_Input); // set line 0 to input
				pCam->LineSource.SetValue(Spinnaker::LineSource_Off); // turn off line source for line 0
				// Configure trigger
				pCam->TriggerSelector.SetValue(Spinnaker::TriggerSelector_AcquisitionStart); // set trigger to begin acquisition
				pCam->TriggerMode.SetValue(Spinnaker::TriggerMode_On); // turn on trigger mode
				pCam->TriggerSource.SetValue(Spinnaker::TriggerSource_Line0); // set line 0 as trigger source
				pCam->TriggerActivation.SetValue(Spinnaker::TriggerActivation_RisingEdge); // trigger on rising edge
				pCam->TriggerDelay.SetValue(pCam->TriggerDelay.GetMin()); // minimize trigger delay
				debugMessage("    Trigger for acquisition start turned ON for line 0", DEBUG_INFO);
				debugMessage("      Trigger delay is " + std::to_string(pCam->TriggerDelay.GetValue()) + " us", DEBUG_INFO);
			}
			else {
				pCam->TriggerSelector.SetValue(Spinnaker::TriggerSelector_AcquisitionStart); // select type of trigger
				pCam->TriggerMode.SetValue(Spinnaker::TriggerMode_Off); // turn off trigger mode
				debugMessage("    Trigger for acquisition start turned OFF", DEBUG_INFO);
			}
			// Output exposure signal?
			item = pg_config.find("output_exposure");
			if (item != pg_config.end()) {
				std::string val = item.value().get<std::string>();
				std::transform(val.begin(), val.end(), val.begin(), ::toupper);
				if (val == "TRUE" || val == "YES" || val == "ON" || val == "Y" || val == "T") {
					// Configure line 2
					pCam->LineSelector.SetValue(Spinnaker::LineSelector_Line2);
					pCam->LineMode.SetValue(Spinnaker::LineMode_Output); // set line 2 to output
					pCam->LineSource.SetValue(Spinnaker::LineSource_ExposureActive); // set line 2 source as exposure window
					debugMessage("    Output of exposure signal activated on line 2", DEBUG_INFO);
				}
			}
		}
		pCam->DeInit();
		// Add camera along with system reference
		cameras.push_back(new PointGreyCamera(system.operator->(), pCam, triggeredAcquisition));
		// Add to camnames, dtypes, etc.
		camnames.push_back("pg" + std::to_string(i));
		formats.push_back(GRAY_8BIT);
		dtypes.push_back(POINTGREY_H5T);
		dcpls.push_back(pg_dcpl);
	}

	debugMessage("Initialization complete\n", DEBUG_INFO);
	// Check number of cameras
	if (cameras.size() == 0) {
		debugMessage("No cameras to record from!", DEBUG_ERROR);
	}
	/* Recording loop */
	else if (fixedlen) {
		timers.start(DTIMER_OVERALL);
		timers.start(DTIMER_PREP);
		record(saveTitle, recordingDuration, triggeredAcquisition);
		timers.pause(DTIMER_CLEANUP);
		timers.pause(DTIMER_OVERALL);
		printDebugTimerInfo();
	}
	else {
		const double MAX_DURATION = 20.0; // Set upper limit so we don't fill the hard drive
		int iteration = 0;

		while (true) { // Loop as long as user wants to record
			std::cout << std::string(getConsoleWidth() - 1, '*') << std::endl;
			// Prepare title index to append to provided filename root
			std::string titleIndex;
			if (iteration < 10000) {
				titleIndex = std::string(4 - std::to_string(iteration).length(), '0') + std::to_string(iteration);
			}
			else {
				titleIndex = std::to_string(iteration);
			}
			std::cout << "Begin recording " + saveTitle + "-" + titleIndex + "? (y/n) ";
			// Get user input: require either explicit yes or no
			std::string userInput;
			while (!(userInput == "YES" || userInput == "Y" || userInput == "NO" || userInput == "N")) {
				std::cin >> userInput;
				for (auto & c : userInput) c = toupper(c); // convert to uppercase
			}
			if (userInput == "NO" || userInput == "N")
				break;

			// Record!
			try {
				timers.start(DTIMER_OVERALL);
				timers.start(DTIMER_PREP);
				record(saveTitle + "-" + titleIndex, MAX_DURATION, triggeredAcquisition);
				timers.pause(DTIMER_CLEANUP);
				timers.pause(DTIMER_OVERALL);
				printDebugTimerInfo();
			}
			catch (...) {
				debugMessage("Error while recording.", DEBUG_ERROR);
			}
			iteration++;
		}
	}

	/* Finalize cameras */
	for (auto ptr : cameras) {
		delete ptr;
	}
temp:
	try {
		camList.Clear();
		system->ReleaseInstance();
	}
	catch (...) {
		debugMessage("Error in Point Grey camera system finalization", DEBUG_ERROR);
	}

	// Memory leak detection
	if (_CrtDumpMemoryLeaks()) {
		debugMessage("Memory leaks found!", DEBUG_ERROR);
	}

	exit(EXIT_SUCCESS);
}