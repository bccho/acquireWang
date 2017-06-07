#pragma once
#pragma warning(push, 0)
#include <string>
#include <thread>
#include <vector>
#pragma warning(pop)
#include "acquirer.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class provides an interface for saving data to a file.
 * It manages a single thread to save to the provided filename.
 * Details of the file format should be implemented in derived classes.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class BaseSaver {
protected:
	const size_t numStreams; // Number of acquirers/streams
	const size_t frameChunkSize; // Number of frames per "chunk" to write at one time
								 // (some applications are faster when frames are written in chunks)
	std::vector< std::vector<BaseFrame> > writeBuffers; // Write buffer to pull frames off thread-safe queues
	std::vector<size_t> framesSaved; // Numbers of frames saved for each acquirer/stream
	std::atomic<bool> saving; // Flag to indicate if saving should abort
private:
	std::thread* saveThread; // Thread for saving
	std::vector<BaseAcquirer> acquirers; // Acquirers for reference

	// Methods for thread
	void moveFramesToWriteBuffers(size_t acqIndex); // Returns true if successful
	void writeLoop();
public:
	// Filename to write to
	const std::string filename;

	// Constructor and destructor
	BaseSaver(std::string& _filename, std::vector<BaseAcquirer>& _acquirers, const size_t _frameChunkSize = 1);
	~BaseSaver();

	// Must be overridden to write frame(s)
	virtual bool writeFrames(size_t nFrames, size_t bufIndex) = 0;

	// Saving flag methods
	bool isSaving() { return saving.load(); }
	void abortSaving() { saving = false; }

	// Saving progress, in number of seconds' worth of frames saved
	double getSavingProgress(size_t acqIndex) { return framesSaved[acqIndex] / acquirers[acqIndex].getFPS(); }
};

