#include "acquirer.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

/* Constructor and destructor */
BaseAcquirer::BaseAcquirer(const std::string& _name, BaseCamera* _camera) :
		name(_name), camera(_camera),
		queue(FRAME_BUFFER_SIZE), queueGUI(FRAME_BUFFER_SIZE),
		framesToAcquire(0), framesReceived(0), acquiring(true) {
	// Choose default GUI downsample rate
	GUI_downsample_rate = (int)(camera->getFPS() / DISPLAY_FRAME_RATE);
	if (GUI_downsample_rate < 1) GUI_downsample_rate = 1;
	// Start thread
	acquireThread = new std::thread(&BaseAcquirer::acquireLoop, this);
	// Initialize camera
	camera->initialize();
}

BaseAcquirer::BaseAcquirer(const BaseAcquirer& other) :
		BaseAcquirer(other.name, other.camera) {}

// Destructor (finalize camera after passing to acquirer, but do not end acquisition)
BaseAcquirer::~BaseAcquirer() {
	// Finalize camera
	camera->finalize();
	// Empty queues
	emptyQueue();
	emptyQueueGUI();
}

/* Other public methods */

bool BaseAcquirer::dequeue(BaseFrame& frame) {
	return queue.try_dequeue(frame);
}

bool BaseAcquirer::dequeueGUI(BaseFrame& frame) {
	return queueGUI.try_dequeue(frame);
}

double BaseAcquirer::getAcquisitionProgress() {
	// Return progress as seconds' worth of frames acquired
	return (double) framesReceived / camera->getFPS();
}

void BaseAcquirer::reset() {
	// Deinit
	emptyQueue();
	emptyQueueGUI();

	// Reinit
	framesToAcquire = 0;
	framesReceived = 0;
	acquiring = true;
}

/* * * * * * * * * *
 * PRIVATE METHODS *
 * * * * * * * * * */

// Puts received frame onto thread-safe queue
bool BaseAcquirer::enqueueFrame(BaseFrame& frame) {
	bool result = queue.enqueue(frame);
	if (!result) debugMessage("[" + std::to_string(framesReceived.load()) + "] Failed to enqueue " + name, LEVEL_ERROR);
	// Update number of frames received
	framesReceived++;
	return result;
}

bool BaseAcquirer::enqueueFrameGUI(BaseFrame& frame) {
	bool result = queueGUI.enqueue(frame);
	return result;
}

void BaseAcquirer::emptyQueue() {
	BaseFrame dequeued;
	while (queue.try_dequeue(dequeued)) {}
}

void BaseAcquirer::emptyQueueGUI() {
	BaseFrame dequeued;
	while (queueGUI.try_dequeue(dequeued)) {}
}

void BaseAcquirer::getAndEnqueue() {
	try {
		std::pair<bool, BaseFrame> received = camera->getFrame(); // get frame from camera
		double timestamp = getClockStamp(); // get timestamp when received
		if (received.first) {
			// Get frame and set timestamp
			BaseFrame frame = received.second;
			frame.setTimestamp(timestamp);
			// Enqueue for GUI (no copy needed?)
			if (framesReceived % GUI_downsample_rate == 0) {
				enqueueFrameGUI(frame);
			}
			enqueueFrame(frame);
		} else {
			debugMessage("Failed to receive " + name + " frame.", LEVEL_ERROR);
		}
	}
	catch (...) {
		debugMessage("Unhandled exception in getAndEnqueue() for " + name + "!", LEVEL_ERROR);
	}
}

void BaseAcquirer::acquireLoop() {
	while (acquiring) {
		// If not indefinitely acquiring, and we have acquired more than we need, stop acquiring
		if (framesToAcquire > 0 && framesReceived >= framesToAcquire) break;
		// Otherwise, block until new frame arrives on camera, then enqueue
		try { getAndEnqueue(); }
		catch (...) {
			debugMessage("[" + std::to_string(framesReceived.load()) + "] Error receiving " + name + " frame", LEVEL_ERROR);
		}
	}
	debugMessage("[!] Exiting " + name + " acquisition thread (acquired " +
		std::to_string(framesReceived) + " frames).", LEVEL_IMPORTANT_INFO);
}
