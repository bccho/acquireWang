#include "saver.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

BaseSaver::BaseSaver(std::string& _filename, std::vector<BaseAcquirer*>& _acquirers, const size_t _frameChunkSize) :
		numStreams(_acquirers.size()), filename(_filename), acquirers(_acquirers),
		framesSaved(numStreams, 0), frameChunkSize(_frameChunkSize),
		writeBuffers(numStreams, std::deque<BaseFrame>()) {
	saving = true;
	saveThread = new std::thread(&BaseSaver::writeLoop, this);
}

BaseSaver::~BaseSaver() {
	debugMessage("~BaseSaver", DEBUG_INFO);
	// End thread
	abortSaving();
	saveThread->join();
	debugMessage("~BaseSaver: joined", DEBUG_INFO);
	delete saveThread;
}

/* * * * * * * * * *
 * PRIVATE METHODS *
 * * * * * * * * * */

void BaseSaver::moveFramesToWriteBuffers(size_t acqIndex) {
	BaseFrame dequeued = acquirers[acqIndex]->dequeue();
	if (dequeued.isValid()) {
		writeBuffers[acqIndex].push_back(dequeued);
	}
}

void BaseSaver::writeLoop() {
	while (saving) {
		// Move one waiting frame to write buffers for each stream
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
		BaseAcquirer* acq = acquirers[leastIndex];
		std::deque<BaseFrame> buf = writeBuffers[leastIndex];

		// If there are enough frames in the buffer to write a chunk...
		if (buf.size() >= frameChunkSize) {
			debugMessage("Writing chunk...", DEBUG_INFO);
			// Write frames to file
			bool res = writeFrames(frameChunkSize, leastIndex);
			// Remove those frames from the write buffer if successful
			if (res) {
				for (size_t i = 0; i < frameChunkSize; i++) { buf.pop_front(); }
			}
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), DEBUG_ERROR);
		}

		// ... or if we are at the end of acquisition
		else if (acq->getFramesToAcquire() > 0 && // (i.e. if not indefinite acquisition
				acq->getFramesReceived() >= acq->getFramesToAcquire() && // and we are done acquiring
				buf.size() + framesSaved[leastIndex] >= acq->getFramesToAcquire()) { // and enough frames are sitting in the write buffer)
			// Write frames to file
			bool res = writeFrames(buf.size(), leastIndex);
			// Remove those frames from the write buffer if successful
			if (res) { buf.clear(); }
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), DEBUG_ERROR);
		}
	}
	std::string numbers;
	for (size_t i = 0; i < numStreams; i++) {
		numbers = numbers + std::to_string(framesSaved[i]) + ", ";
	}
	debugMessage("[!] Exiting saving thread. Saved " + numbers + "frames.", DEBUG_IMPORTANT_INFO);
}
