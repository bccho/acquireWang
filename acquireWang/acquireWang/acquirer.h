#pragma once
#pragma warning(push, 0)
#include <vector>
#include <thread>
#include <readerwriterqueue.h>
#pragma warning(pop)
#include "camera.h"
#include "timer.h"
#include "debug.h"

using namespace moodycamel;

// Typedefs and defines
#define frame_t std::pair<timestamp_t, T*>
#define DISPLAY_FRAME_RATE 30.0
#define FRAME_BUFFER_SIZE 100
const int64_t TIME_WAIT_QUEUE = 50000; // [microseconds], so 50000 = 50 ms


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class provides an interface for acquiring rapidly from a single
 * camera. It manages one thread per class instance that calls the camera
 * methods, and exposes the image data via a queue-esque API for BaseSaver
 * derived classes. It also supports piping some frames for display on a GUI.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class BaseAcquirer {
protected:
	std::string name;
	BaseCamera& camera;
private:
	// Thread-safe queues
	BlockingReaderWriterQueue<BaseFrame> queue; // timestamp, pointer to stored object
	BlockingReaderWriterQueue<BaseFrame> queueGUI;

	int GUI_downsample_rate; // How often we should skip frames when preparing frames for the GUI (1 = no frames skipped)
	// Numbers of frames to acquire, and frames received
	size_t framesToAcquire; // default value of 0 indicates indefinite acquisition
	std::atomic<size_t> framesReceived;

	std::thread* acquireThread; // Thread for acquisition loop
	std::atomic<bool> acquiring; // Flag to indicate if we should abort acquisition

	/* Methods */
	bool enqueueFrame(BaseFrame& frame); // return true if successful
	bool enqueueFrameGUI(BaseFrame& frame);
	void emptyQueue();
	void emptyQueueGUI();

	// Methods for thread
	void getAndEnqueue();
	void acquireLoop();

	// Debug
	int cnt = 0;

	// Disable assignment operator and copy constructor
	BaseAcquirer& operator=(const BaseAcquirer& other) = delete;
	BaseAcquirer(const BaseAcquirer& other) = delete;

public:
	// Constructor (do not initialize camera before passing to acquirer)
	BaseAcquirer(const std::string& _name, BaseCamera& _camera);

	// Destructor (do not finalize camera after passing to acquirer)
	virtual ~BaseAcquirer();

	/* Getter and setter methods */
	std::string getName() { return name; }
	size_t getFramesReceived() { return framesReceived; }
	size_t getFramesToAcquire() { return framesToAcquire; }
	void setFramesToAcquire(size_t _framesToAcquire) { framesToAcquire = _framesToAcquire; }
	double getSecondsToAcquire() { return (double) framesToAcquire / camera.getFPS(); }
	bool isAcquiring() { return acquiring && framesReceived < framesToAcquire; }
	void abortAcquisition() {
		acquiring = false;
		framesToAcquire = framesReceived;
		if (acquireThread != nullptr) {
			acquireThread->join();
			delete acquireThread;
			acquireThread = nullptr;
		}
	}
	
	/* Queue APIs */
	size_t getQueueSizeApprox() { return queue.size_approx(); }
	size_t getQueueGUISizeApprox() { return queueGUI.size_approx(); }
	bool isQueueEmpty() { return queue.peek() == nullptr; }
	bool isQueueGUIEmpty() { return queueGUI.peek() == nullptr; }
	BaseFrame dequeue(); // Return true if successful
	BaseFrame dequeueGUI();
	BaseFrame getMostRecentGUI();

	/* Methods */
	// Camera access methods (for the Law of Demeter)
	void beginAcquisition() { camera.beginAcquisition(); }
	void endAcquisition() { camera.endAcquisition(); }
	size_t getWidth() { return camera.getWidth(); }
	size_t getHeight() { return camera.getHeight(); }
	size_t getChannels() { return camera.getChannels(); }
	size_t getFrameSize() { return camera.getFrameSize(); }
	size_t getBytesPerPixel() { return camera.getBytesPerPixel(); }
	size_t getFrameBytes() { return camera.getBytes(); }
	double getFPS() { return camera.getFPS(); }
	double getCamType() { return camera.getCamType(); }
	std::vector<size_t> getDims() {
		std::vector<size_t> res = { camera.getChannels(), camera.getHeight(), camera.getWidth() };
		return res;
	}

	// Starts acquisition threads, etc.
	void run();
	// Gets acquisition progress (in number of seconds' worth of frames acquired)
	double getAcquisitionProgress();
	// Resets acquirer member variables as though freshly constructed
	void reset();
	// Returns true if there is a frame available to show on the GUI
	bool readyForGUI() { return (queueGUI.peek() != nullptr); }
	// Returns true if the GUI in the main loop should stop blocking while waiting for this acquirer
	bool shouldDraw() { return readyForGUI() || (framesReceived == framesToAcquire); }
};
