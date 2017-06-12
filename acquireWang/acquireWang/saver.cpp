#include "saver.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

BaseSaver::BaseSaver(std::string& _filename, std::vector<BaseAcquirer*>& _acquirers, const size_t _frameChunkSize) :
		numStreams(_acquirers.size()), filename(_filename), acquirers(_acquirers),
		framesSaved(numStreams, 0), frameChunkSize(_frameChunkSize),
		writeBuffers(numStreams, std::deque<BaseFrame>()) {
	debugMessage("BaseSaver constructor", DEBUG_HIDDEN_INFO);
	saving = true;
	saveThread = new std::thread(&BaseSaver::writeLoop, this);
}

BaseSaver::~BaseSaver() {
	debugMessage("~BaseSaver", DEBUG_HIDDEN_INFO);
	// End thread
	abortSaving();
	saveThread->join();
	debugMessage("~BaseSaver: joined", DEBUG_HIDDEN_INFO);
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
		
		double leastSoFar = DBL_MAX;
		size_t leastIndex = 0;
		bool done = true;
		for (size_t i = 0; i < numStreams; i++) {
			// Find stream with least saving progress
			if (getSavingProgress(i) < leastSoFar) {
				leastSoFar = getSavingProgress(i);
				leastIndex = i;
			}
			// Exit condition not yet satisfied
			if (acquirers[i]->getFramesToAcquire() == 0 ||
					framesSaved[i] < acquirers[i]->getFramesToAcquire())
				done = false;
		}

		// Exit condition
		if (done) break;

		/* Now, we deal only with the acquirer with the least saving progress */
		BaseAcquirer* acq = acquirers[leastIndex];
		std::deque<BaseFrame> buf = writeBuffers[leastIndex];

		// If we are at the end of acquisition
		if (acq->getFramesToAcquire() > 0 && // (i.e. if not indefinite acquisition
				!acq->isAcquiring() && // and we are done acquiring
				buf.size() + framesSaved[leastIndex] >= acq->getFramesToAcquire()) { // and enough frames are sitting in the write buffer)
			debugMessage("Last chunk", DEBUG_HIDDEN_INFO);
			// Write frames to file
			bool res = writeFrames(acq->getFramesToAcquire() - framesSaved[leastIndex], leastIndex);
			// Remove those frames from the write buffer if successful
			if (res) { buf.clear(); }
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), DEBUG_ERROR);
		}
		
		// ... or if there are enough frames in the buffer to write a chunk...
		else if (buf.size() >= frameChunkSize && // (i.e. enough frames in buffer
					acq->isAcquiring()) { // and we are still acquiring)
			// Write frames to file
			bool res = writeFrames(frameChunkSize, leastIndex);
			// Remove those frames from the write buffer if successful
			if (res) {
				for (size_t i = 0; i < frameChunkSize; i++) { buf.pop_front(); }
			}
			else debugMessage("Failed to write chunk for acquirer #" + std::to_string(leastIndex), DEBUG_ERROR);
		}
	}
	std::string numbers;
	for (size_t i = 0; i < numStreams; i++) {
		numbers = numbers + std::to_string(framesSaved[i]) + ", ";
	}
	debugMessage("[!] Exiting saving thread. Saved " + numbers + "frames.", DEBUG_IMPORTANT_INFO);
}
