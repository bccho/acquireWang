#include "saver.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

BaseSaver::BaseSaver(std::string& _filename, std::vector<BaseAcquirer*>& _acquirers, const size_t _frameChunkSize) :
		numStreams(_acquirers.size()), filename(_filename), acquirers(_acquirers),
		framesSaved(numStreams, 0), frameChunkSize(_frameChunkSize),
		writeBuffers(numStreams, std::deque<std::reference_wrapper<BaseFrame>>()) {
	debugMessage("BaseSaver constructor", DEBUG_HIDDEN_INFO);
	saving = true;
	saveThread = new std::thread(&BaseSaver::writeLoop, this);
}

BaseSaver::~BaseSaver() {
	debugMessage("~BaseSaver", DEBUG_HIDDEN_INFO);
	if (saving) abortSaving(true);
	debugMessage("~BaseSaver: joined", DEBUG_HIDDEN_INFO);
}

/* * * * * * * * * *
 * PRIVATE METHODS *
 * * * * * * * * * */

bool BaseSaver::moveFrameToWriteBuffer(size_t acqIndex) {
	timers.start(DTIMER_DEQUEUE);
	BaseFrame dequeued = acquirers[acqIndex]->dequeue();
	timers.pause(DTIMER_DEQUEUE);
	bool result = dequeued.isValid();
	if (result) {
		writeBuffers[acqIndex].push_back(std::ref(dequeued));
	}
	return result;
}

void BaseSaver::writeLoop() {
	while (saving) {
		// Move waiting frames to write buffers for each stream
		timers.start(DTIMER_MOVE_WRITE);
		//for (size_t i = 0; i < frameChunkSize * 2; i++) {
		for (size_t i = 0; i < frameChunkSize; i++) {
			bool allDone = true; // break if all cameras have no frames to dequeue
			for (size_t j = 0; j < numStreams; j++) {
				allDone = allDone && !moveFrameToWriteBuffer(j);
			}
			if (allDone) {
				//debugMessage("Break after " + std::to_string(i) + " frames", DEBUG_INFO);
				break;
			}
		}
		timers.pause(DTIMER_MOVE_WRITE);
		
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
		std::deque<std::reference_wrapper<BaseFrame>>& buf = writeBuffers[leastIndex];

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
		
		// Otherwise, if there are enough frames in the buffer to write a chunk...
		else if (buf.size() >= frameChunkSize) {
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
