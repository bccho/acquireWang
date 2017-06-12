#include "acquirer.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

/* Constructor and destructor */
BaseAcquirer::BaseAcquirer(const std::string& _name, BaseCamera& _camera) :
		name(_name), camera(_camera), acquireThread(nullptr),
		queue(FRAME_BUFFER_SIZE), queueGUI(FRAME_BUFFER_SIZE),
		framesToAcquire(0), framesReceived(0), acquiring(true) {
	debugMessage("BaseAcquirer constructor " + name, DEBUG_INFO);
	// Initialize camera
	camera.initialize();
	// Choose default GUI downsample rate
	GUI_downsample_rate = (int)(camera.getFPS() / DISPLAY_FRAME_RATE);
	if (GUI_downsample_rate < 1) GUI_downsample_rate = 1;
}

//BaseAcquirer::BaseAcquirer(const BaseAcquirer& other) :
//		BaseAcquirer(other.name, other.camera) {
//	debugMessage("BaseAcquirer copy constructor " + name, DEBUG_INFO);
//}

// Destructor (finalize camera after passing to acquirer, but do not end acquisition)
BaseAcquirer::~BaseAcquirer() {
	// End thread
	abortAcquisition();
	if (acquireThread != nullptr) {
		acquireThread->join();
		delete acquireThread;
	}
	// Finalize camera
	camera.finalize();
	// Empty queues
	emptyQueue();
	emptyQueueGUI();
}

/* Other public methods */

void BaseAcquirer::run() {
	// Start thread
	acquireThread = new std::thread(&BaseAcquirer::acquireLoop, this);
}

BaseFrame BaseAcquirer::dequeue() {
	BaseFrame result;
	debugMessage("dequeue() " + name + ": queue.peek() = " + std::to_string((long long)queue.peek()), DEBUG_INFO);
	//if (queue.peek() != nullptr)
	//	debugMessage("          frame valid? " + std::to_string(queue.peek()->isValid()), DEBUG_INFO);
	queue.try_dequeue(result);
	debugMessage("          " + name + ": queue.peek() = " + std::to_string((long long)queue.peek()), DEBUG_INFO);
	debugMessage("          frame valid? " + std::to_string(result.isValid()), DEBUG_INFO);
	if (result.isValid()) {
		debugMessage("dequeued valid frame", DEBUG_INFO);
	}
	return result;
}

BaseFrame BaseAcquirer::dequeueGUI() {
	BaseFrame result;
	queueGUI.try_dequeue(result);
	return result;
}

BaseFrame BaseAcquirer::getMostRecentGUI() {
	BaseFrame result;
	// While there are things on the queue, dequeue
	while (!isQueueGUIEmpty()) {
		result = dequeueGUI();
	}
	return result;
}

double BaseAcquirer::getAcquisitionProgress() {
	// Return progress as seconds' worth of frames acquired
	return (double) framesReceived / camera.getFPS();
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
	if (!result) debugMessage("[" + std::to_string(framesReceived.load()) + "] Failed to enqueue " + name, DEBUG_ERROR);
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
		BaseFrame received = camera.getFrame(); // get frame from camera
		double timestamp = getClockStamp(); // get timestamp when received
		if (received.isValid()) { // i.e. success
			// Get frame and set timestamp
			received.setTimestamp(timestamp);
			// Enqueue for GUI (implicit copy)
			if (framesReceived % GUI_downsample_rate == 0) {
				enqueueFrameGUI(received);
			}
			enqueueFrame(received);
			debugMessage("getAndEnqueue() " + name + ": queue.peek() = " + std::to_string((long long) queue.peek()), DEBUG_INFO);
		} else {
			debugMessage("Failed to receive " + name + " frame.", DEBUG_ERROR);
		}
	}
	catch (...) {
		debugMessage("Unhandled exception in getAndEnqueue() for " + name + "!", DEBUG_ERROR);
	}
}

void BaseAcquirer::acquireLoop() {
	while (acquiring) {
		//debugMessage("acquireLoop " + getName(), DEBUG_INFO);
		// If not indefinitely acquiring, and we have acquired more than we need, stop acquiring
		if (framesToAcquire > 0 && framesReceived >= framesToAcquire) break;
		// Otherwise, block until new frame arrives on camera, then enqueue
		try { getAndEnqueue(); }
		catch (...) {
			debugMessage("[" + std::to_string(framesReceived.load()) + "] Error receiving " + name + " frame", DEBUG_ERROR);
		}
	}
	debugMessage("[!] Exiting " + name + " acquisition thread (acquired " +
		std::to_string(framesReceived) + " frames).", DEBUG_IMPORTANT_INFO);
}
