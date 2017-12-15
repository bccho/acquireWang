#include "acquirer.h"

/* * * * * * * * * *
 * PUBLIC METHODS  *
 * * * * * * * * * */

/* Constructor and destructor */
BaseAcquirer::BaseAcquirer(const std::string& _name, BaseCamera& _camera) :
		name(_name), camera(_camera), acquireThread(nullptr),
		queue(FRAME_BUFFER_SIZE), queueGUI(FRAME_BUFFER_SIZE),
		framesToAcquire(0), framesReceived(0), acquiring(true) {
	debugMessage("BaseAcquirer constructor " + name, DEBUG_HIDDEN_INFO);
	// Initialize camera
	camera.initialize();
	// Choose default GUI downsample rate
	GUI_downsample_rate = (int)(camera.getFPS() / DISPLAY_FRAME_RATE);
	if (GUI_downsample_rate < 1) GUI_downsample_rate = 1;
}

// Destructor (finalize camera after passing to acquirer, but do not end acquisition)
BaseAcquirer::~BaseAcquirer() {
	// End thread
	if (acquiring) abortAcquisition();
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
	//queue.wait_dequeue_timed(result, TIME_WAIT_QUEUE);
	queue.try_dequeue(result);
	if (result.isValid()) {
		cnt++;
		debugMessage(name + ": dequeued " + std::to_string(cnt) + " valid frames", DEBUG_HIDDEN_INFO);
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
		timers.start(DTIMER_GET_FRAME);
		BaseFrame received = camera.getFrame(); // get frame from camera
		timers.pause(DTIMER_GET_FRAME);
		if (received.isValid()) { // i.e. success
			// Enqueue for GUI (implicit copy)
			if (framesReceived % GUI_downsample_rate == 0) {
				enqueueFrameGUI(received);
			}
			enqueueFrame(received);
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
