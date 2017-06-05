#pragma once
#include <string>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class provides an interface for saving data to a file.
 * It manages a single thread to save to the provided filename.
 * Details of the file format should be implemented in derived classes.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
template <typename T> class BaseSaver {
private:
	std::thread savingThread;
public:
	// Filename to write to
	const std::string filename;

	// Default constructor and destructor
	BaseSaver(const std::string& _filename) : filename(_filename) {}
	~BaseSaver() {}

	// Must be overridden to write frame(s)
	virtual int writeFrames(size_t nFrames, T* buffer) = 0;
};
