#pragma once
#pragma warning(push, 0)
#include <map>
#include "json.hpp" // JSON config files
#pragma warning(pop)

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

