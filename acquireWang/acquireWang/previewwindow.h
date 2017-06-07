#pragma once
#pragma warning(push, 0)
#include <algorithm>
#include <vector>
#include <string>
#pragma warning(pop)
#include "visualization.h"
#include "debug.h"
#include "acquirer.h"

enum format { DEPTH_16BIT, GRAY_8BIT, GRAY_16BIT };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This class provides a wrapper around GLFW methods to make a basic camera
 * output visualization window. This class is intended only to simplify the
 * code in main.cpp. I foresee this class being modified heavily to add and
 * modify various GUI features.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class PreviewWindow {
private:
	size_t numBuffers;
	int nRows, nCols;
	const int PROGRESSBAR_HEIGHT = 30;
	GLFWwindow* win; // window handle
	std::vector<stream_format> formats; // array of stream formats for displaying frames
	std::vector<texture_buffer> buffers; // array of buffers to draw items
	std::vector<BaseAcquirer*>& acquirers; // array of acquirers so that frames can be pulled from the GUI queue

	bool shouldClose; // flag to indicate if the window should close
public:
	PreviewWindow(int width, int height, const char* title, std::vector<BaseAcquirer*> _acquirers, std::vector<format> _formats) :
			numBuffers(_acquirers.size()), acquirers(_acquirers), shouldClose(false),
			buffers(numBuffers) {
		// Populate formats[] using enum values provided
		for (size_t i = 0; i < _formats.size(); i++) {
			switch (_formats[i]) {
				case DEPTH_16BIT: formats.push_back(stream_format::z16); break;
				case GRAY_8BIT: formats.push_back(stream_format::y8); break;
				case GRAY_16BIT: formats.push_back(stream_format::y16); break;
			}
		}
		// Initialize window
		glfwInit();
		win = glfwCreateWindow(width, height, title, 0, 0);
		glfwMakeContextCurrent(win);
		// Buffer array sizes
		nRows = (int) std::ceil(std::sqrt((double) numBuffers));
		nCols = (int) std::ceil((double) numBuffers / (double) nRows);
	}

	~PreviewWindow() {
		debugMessage("~PreviewWindow", LEVEL_INFO);
	}

	void run() {
		try {
			while (true) {
				// GUI events
				glfwPollEvents();
				int state = glfwGetKey(win, GLFW_KEY_Q); // when you press Q or click the exit button, stop acquisition
				if (shouldClose || state == GLFW_PRESS || glfwWindowShouldClose(win)) {
					break;
				}

				// Draw frames if all GUI queues have something to show
				if (std::all_of(acquirers.begin(), acquirers.end(), [](BaseAcquirer* acq) { return acq->shouldDraw(); })) {
					// Get frame buffer dimensions and clear frame buffer
					int w, h;
					glfwGetFramebufferSize(win, &w, &h);
					glViewport(0, 0, w, h);
					glClear(GL_COLOR_BUFFER_BIT);

					glPushMatrix();
					glfwGetWindowSize(win, &w, &h);
					glOrtho(0, w, h, 0, -1, +1);

					int buf_w = w / nCols;
					int buf_h = (h - PROGRESSBAR_HEIGHT * 2 - 10) / nRows;

					// Update buffers
					for (size_t i = 0; i < numBuffers; i++) {
						int rx = buf_w * (int) i / nRows;
						int ry = buf_h * ((int) i % nRows);
						// Get frame and show
						BaseFrame frame;
						if (acquirers[i]->getMostRecentGUI(frame))
							showFrame(i, frame, rx, ry, buf_w, buf_h, acquirers[i]->getName());
					}

					//// Progress bars
					//double acqProgress = 0;
					//int cnt = 0;
					//for (size_t i = 0; i < numBuffers; i++) {
					//	if (acquirers[i].getFramesToAcquire() > 0) {
					//		acqProgress += acquirers[i].getAcquisitionProgress();
					//		cnt++;
					//	}
					//}
					//acqProgress /= (double)(cnt);

					//double pgProgress = 0;
					//for (auto str : pgStreams) {
					//	pgProgress += str->getSavingProgress();
					//}
					//pgProgress /= (double)(numPGcameras);
					//GUI::progress_bar({ 30, buf_h * 2 + 10, w - 30, buf_h * 2 + 30 }, acquisitionProgress, "Acquisition progress");
					//GUI::progress_bar({ 30, buf_h * 2 + 40, w - 30, buf_h * 2 + 60 }, kinProgress, "Kinect camera saving progress");
					//GUI::progress_bar({ 30, buf_h * 2 + 70, w - 30, buf_h * 2 + 90 }, pgProgress, "Point Grey camera saving progress");

					// Show on screen
					glPopMatrix();
					glfwSwapBuffers(win);
				}
			}
		} catch (...) {
			debugMessage("Error in GUI update loop", LEVEL_ERROR);
		}
	}

	void showFrame(size_t bufInd, BaseFrame& frame, int rx, int ry, int rw, int rh, const std::string caption = "") {
		void* frameData = std::malloc(frame.getBytes());
		frame.copyDataToBuffer(frameData);
		buffers[bufInd].show(frameData, (int) frame.getWidth(), (int) frame.getHeight(), formats[bufInd], caption, rx, ry, rw, rh);
		std::free(frameData);
	}

	void close() {
		shouldClose = true;
	}
};