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
#include <map>

#include <chrono> // Timing

// Externals
#include "H5Cpp.h" // HDF5
#include "json.hpp" // JSON config files
#pragma warning(pop)

// Other unit files
#include "acquirer.h"
#include "kincam.h"
#include "pgcam.h"
#include "h5out.h"
#include "previewwindow.h"
#include "debug.h"

/* Global variables */
size_t frameChunkSize;
std::map<std::string, size_t> params;

// For threads
std::atomic<bool> acquiring;
std::atomic<bool> saving;

// GUI
GLFWwindow* win;
texture_buffer buffers[4];

// Cameras
std::vector<BaseCamera*> cameras;
std::vector<std::string> camnames;
std::vector<format> formats;
std::vector<PredType> dtypes;
std::vector<DSetCreatPropList> dcpls;

/* Methods */

// Read configurations
std::map<std::string, size_t> readConfig() {
	std::map<std::string, size_t> params;

	std::ifstream fParams("params.json");
	if (fParams.good()) {
		// Load from file
		std::stringstream paramsBuffer;
		paramsBuffer << fParams.rdbuf();
		auto parsed = nlohmann::json::parse(paramsBuffer.str());
		params = parsed.get<std::map<std::string, size_t>>();

		debugMessage("Loaded parameters from params.json", DEBUG_HIDDEN_INFO);
	}
	else {
		/* Create JSON file with defaults */
		// Video parameters
		params["_frameChunkSize"] = 50;
		params["_kinectXchunk"] = 32;
		params["_kinectYchunk"] = 53;
		params["_pgXchunk"] = 32;
		params["_pgYchunk"] = 32;
		params["_compression"] = 0;

		// Access parameters for efficient writing
		params["_lz4_block_size"] = 1 << 30;
		params["_mdc_nelmnts"] = 1024;
		//params["_rdcc_nslots"] = 3209; // prime number close to 3200
		params["_rdcc_nslots"] = 32009; // prime number close to 32000
		params["_rdcc_nbytes"] = 50 * 1024 * 1280 * 8;
		params["_sievebufsize"] = 8388608;

		// Save
		nlohmann::json j_map(params);
		std::ofstream f2("params.json");
		f2 << j_map.dump(4);
		f2.close();

		debugMessage("Using default parameters (saved to params.json).", DEBUG_INFO);
	}
	fParams.close();

	return params;
}

// Utilities
bool fileExists(const std::string& name) {
	if (FILE *file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	} else { return false; }
}

int getConsoleWidth() {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	int columns;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	return columns;
}

// Recording session
int record(std::string& saveTitle, double duration) {
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
	fapl.setCache(65536000, params["_rdcc_nslots"], params["_rdcc_nbytes"], 0);
	// Create saving object
	H5Out* h5out = new H5Out(saveTitle + ".h5", acquirers, frameChunkSize, camnames, dtypes,
		H5::FileCreatPropList::DEFAULT, fapl, dcpls);

	/* Set up frame counts */
	for (size_t i = 0; i < cameras.size(); i++) {
		size_t totalFrames = round(duration * 60.0 * cameras[i]->getFPS());
		acquirers[i]->setFramesToAcquire(totalFrames);
	}

	/* Start GUI */
	PreviewWindow preview(960, 720, "Wang Lab behavior acquisition tool (press Q to stop acquisition)",
		acquirers, *h5out, formats);
	timers.pause(1);
	timers.start(3);
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->run();
		acquirers[i]->beginAcquisition();
	}
	//// Wait for acquirers to be ready
	//while (!std::all_of(acquirers.begin(), acquirers.end(), [](BaseAcquirer* acq) { return acq->ready(); })) {}
	preview.run();
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->endAcquisition();
	}
	timers.pause(3);
	timers.start(2);

	// This will block until acquisition and saving threads are joined
	for (size_t i = 0; i < cameras.size(); i++) {
		acquirers[i]->abortAcquisition();
	}
	h5out->abortSaving();

	// Write metadata
	for (size_t i = 0; i < acquirers.size(); i++) {
		h5out->writeScalarAttribute(acquirers[i]->getName() + "_fps", cameras[i]->getFPS());
	}
	h5out->writeScalarAttribute("deflate", params["_compression"]);

	// Finalizez
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
		debugMessage("Usage:\n\tacquireWang.exe filename [numMinutes = 0]", DEBUG_MUST_SHOW);
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
	params = readConfig();

	frameChunkSize = params["_frameChunkSize"];

	/* Set up HDF5 DCPLs */
	// Set up dataset creation property lists
	H5::DSetCreatPropList kin_dcpl;
	const int frame_ndims = 4;
	size_t kin_chunk_dims[frame_ndims] = { frameChunkSize, 1, params["_kinectYchunk"], params["_kinectXchunk"] };
	kin_dcpl.setChunk(frame_ndims, kin_chunk_dims);
	H5::DSetCreatPropList pg_dcpl;
	size_t pg_chunk_dims[frame_ndims] = { frameChunkSize, 1, params["_pgYchunk"], params["_pgXchunk"] };
	pg_dcpl.setChunk(frame_ndims, pg_chunk_dims);
	if (params["_compression"] > 0) {
		kin_dcpl.setDeflate(params["_compression"]);
		kin_dcpl.setShuffle();
		pg_dcpl.setDeflate(params["_compression"]);
		pg_dcpl.setShuffle();
	}
	// Enable the LZ4 filter
	const int H5Z_FILTER_LZ4 = 32004;
	const unsigned int lz4_params[1] = { params["_lz4_block_size"] }; // block size in bytes (default = 1<<30 == 1.0 GB)
	kin_dcpl.setFilter(H5Z_FILTER_LZ4, H5Z_FLAG_MANDATORY, 1, lz4_params);
	pg_dcpl.setFilter(H5Z_FILTER_LZ4, H5Z_FLAG_MANDATORY, 1, lz4_params);

	/* Set up cameras */
	// Initialize Point Grey system
	Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
	Spinnaker::CameraList camList = system->GetCameras();
	int numPGcameras = camList.GetSize();
	debugMessage("Connected Point Grey devices: " + std::to_string(numPGcameras), DEBUG_INFO);

	// Set up Kinect camera
	// TODO: make a class to hold camnames, dtypes, etc.
	cameras.push_back(new KinectCamera);
	camnames.push_back("kinect");
	formats.push_back(DEPTH_16BIT);
	dtypes.push_back(KINECT_H5T);
	dcpls.push_back(kin_dcpl);

	// Set up Point Grey cameras
	std::vector<Spinnaker::Camera*> pgCameras;
	for (int i = 0; i < numPGcameras; i++) {
		pgCameras.push_back(camList.GetByIndex(i).operator->());
		cameras.push_back(new PointGreyCamera(pgCameras[i]));
		// Add to camnames, dtypes, etc.
		camnames.push_back("pg" + std::to_string(i));
		formats.push_back(GRAY_8BIT);
		dtypes.push_back(POINTGREY_H5T);
		dcpls.push_back(pg_dcpl);
	}

	/* Recording loop */
	if (fixedlen) {
		timers.start(0);
		timers.start(1);
		record(saveTitle, recordingDuration);
		timers.pause(2);
		timers.pause(0);
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
				timers.start(0);
				timers.start(1);
				record(saveTitle + "-" + titleIndex, MAX_DURATION);
				timers.pause(2);
				timers.pause(0);
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
	try {
		for (auto ptr : pgCameras) {
			ptr->DeInit();
			ptr = NULL;
		}
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