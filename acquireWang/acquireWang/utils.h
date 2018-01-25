#pragma once
#pragma warning(push, 0)
#include <map>
#include "json.hpp" // JSON config files
#pragma warning(pop)

using json = nlohmann::json;

// Read JSON configurations from file
json readJSON(std::string filename) {
	std::ifstream fConfig(filename);
	json result;
	if (fConfig.good()) {
		// Load from file
		std::stringstream buf;
		buf << fConfig.rdbuf();
		result = json::parse(buf.str());
	}
	fConfig.close();
	return result;
}

// Read configurations
json readConfig() {
	json config;
	std::string config_filename = "config.json";

	std::ifstream fConfig(config_filename);
	if (fConfig.good()) {
		debugMessage("Loading parameters from " + config_filename, DEBUG_HIDDEN_INFO);

		// Load from file
		std::stringstream buf;
		buf << fConfig.rdbuf();
		config = json::parse(buf.str());
	}
	else {
		/* TODO: some sort of defaultdict set-up with "getParam" function; templated. */

		/* Create JSON file with defaults */
		// Video parameters
		config["_frameChunkSize"] = 50;
		config["_kinectXchunk"] = 32;
		config["_kinectYchunk"] = 53;
		config["_pgXchunk"] = 32;
		config["_pgYchunk"] = 32;
		config["_compression"] = 0;

		// Access parameters for efficient writing
		config["_lz4_block_size"] = 1 << 30;
		config["_mdc_nelmnts"] = 1024;
		config["_rdcc_nslots"] = 32009; // prime number close to 32000
			//params["_rdcc_nslots"] = 3209; // prime number close to 3200
		config["_rdcc_nbytes"] = 50 * 1024 * 1280 * 8;
		config["_sievebufsize"] = 8388608;

		// Save
		std::ofstream fout(config_filename);
		fout << config.dump(4);
		fout.close();

		debugMessage("Using default parameters (saved to " + config_filename + ")", DEBUG_INFO);
	}
	fConfig.close();

	return config;
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

