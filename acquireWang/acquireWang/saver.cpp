#include "saver.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

BaseSaver::BaseSaver(std::string& _filename, std::vector<BaseAcquirer>& _acquirers, const size_t _frameChunkSize) :
		numStreams(_acquirers.size()), filename(_filename), acquirers(_acquirers),
		framesSaved(numStreams, 0), frameChunkSize(_frameChunkSize),
		writeBuffers(numStreams, std::vector<BaseFrame>()) {
	saving = true;
	saveThread = new std::thread(&BaseSaver::writeLoop, this);
}

BaseSaver::~BaseSaver() {
	saveThread->join();
	delete saveThread;
}

/* * * * * * * * * *
 * PRIVATE METHODS *
 * * * * * * * * * */

void BaseSaver::writeLoop() {
	while (saving) {
		// Move all waiting frames to write buffers for all streams
		for (size_t i = 0; i < numStreams; i++) {
			moveFramesToWriteBuffers(i);
		}
		
		// Find stream with least saving progress
		double leastSoFar = DBL_MAX;
		size_t leastIndex = 0;
		for (size_t i = 0; i < numStreams; i++) {
			if (getSavingProgress(i) < leastSoFar) {
				leastSoFar = getSavingProgress(i);
				leastIndex = i;
			}
		}

		/* Now, we deal only with the acquirer with the least saving progress */
		BaseAcquirer acq = acquirers[leastIndex];
		std::vector<BaseFrame> buf = writeBuffers[leastIndex];

		// If there are enough frames in the buffer to write a chunk...
		if (buf.size() >= frameChunkSize) {
			// Write frames to file
			bool res = writeFrames(frameChunkSize, leastIndex);
			// Remove those frames from the write buffer if successful
			std::vector<BaseFrame>::iterator start_it;
			if (res) buf.erase(start_it, start_it + frameChunkSize);
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), LEVEL_ERROR);
		}

		// ... or if we are at the end of acquisition
		else if (acq.getFramesToAcquire() > 0 && // (i.e. if not indefinite acquisition
				acq.getFramesReceived() >= acq.getFramesToAcquire() && // and we are done acquiring
				buf.size() + framesSaved[leastIndex] >= acq.getFramesToAcquire()) { // and enough frames are sitting in the write buffer)
			// Write frames to file
			bool res = writeFrames(buf.size(), leastIndex);
			// Remove those frames from the write buffer if successful
			if (res) buf.clear();
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), LEVEL_ERROR);
		}
	}
}

void BaseSaver::moveFramesToWriteBuffers(size_t acqIndex) {
	while (!acquirers[acqIndex].isQueueEmpty()) {
		BaseFrame dequeued;
		bool succeeded = acquirers[acqIndex].dequeue(dequeued);
		if (succeeded) { writeBuffers[acqIndex].push_back(dequeued); }
	}
}