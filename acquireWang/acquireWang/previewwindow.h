#pragma once
#pragma warning(push, 0)
#include <algorithm>
#include <vector>
#include <string>
#pragma warning(pop)
#include "visualization.h"
#include "debug.h"
#include "acquirer.h"
#include "saver.h"

enum format { DEPTH_16BIT, GRAY_8BIT, GRAY_16BIT };
const int PROGRESSBAR_HEIGHT = 20;
const int PROGRESSBAR_GAP = 5;

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
	GLFWwindow* win; // window handle
	std::vector<stream_format> formats; // array of stream formats for displaying frames
	std::vector<texture_buffer> buffers; // array of buffers to draw items
	std::vector<BaseAcquirer*>& acquirers; // array of acquirers so that frames can be pulled from the GUI queue
	BaseSaver& saver; // saver

	bool shouldClose; // flag to indicate if the window should close
public:
	PreviewWindow(int width, int height, const char* title,
				std::vector<BaseAcquirer*>& _acquirers, BaseSaver& _saver, std::vector<format>& _formats) :
			numBuffers(_acquirers.size()), acquirers(_acquirers), saver(_saver), shouldClose(false),
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
		debugMessage("~PreviewWindow", DEBUG_HIDDEN_INFO);
		glfwDestroyWindow(win); // exit GUI
		glfwTerminate();
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
					int buf_h = h / nRows;

					// Update buffers
					for (size_t i = 0; i < numBuffers; i++) {
						// Coordinates
						int rx = buf_w * ((int) i / nRows); // left of frame
						int ry = buf_h * ((int) i % nRows); // top of frame
						int x1 = rx + PROGRESSBAR_GAP; // left of progress bars
						int x2 = rx + buf_w - PROGRESSBAR_GAP; // right of progress bars
						int y0 = ry + buf_h - PROGRESSBAR_HEIGHT * 2 - PROGRESSBAR_GAP * 3; // bottom of frame
						int y1 = y0 + PROGRESSBAR_GAP; // top of first bar
						int y2 = y1 + PROGRESSBAR_HEIGHT; // bottom of first bar
						int y3 = y2 + PROGRESSBAR_GAP; // top of second bar
						int y4 = y3 + PROGRESSBAR_HEIGHT; // bottom of second bar
						// Get frame and show
						BaseFrame frame = acquirers[i]->getMostRecentGUI();
						if (frame.isValid()) {
							showFrame(i, frame, rx, ry, buf_w, y0 - ry, acquirers[i]->getName());
						}
						// Progress bars
						if (acquirers[i]->getFramesToAcquire() > 0) {
							double acquisitionProgress = acquirers[i]->getAcquisitionProgress() / acquirers[i]->getSecondsToAcquire();
							GUI::progress_bar({ x1, y1, x2, y2 }, acquisitionProgress, acquirers[i]->getName() + " acquisition progress");

							double savingProgress = saver.getSavingProgress(i) / acquirers[i]->getSecondsToAcquire();
							GUI::progress_bar({ x1, y3, x2, y4 }, savingProgress, acquirers[i]->getName() + " saving progress");

							if (acquisitionProgress < savingProgress) {
								//debugMessage("Lol wut", DEBUG_INFO);
							}
						}
					}

					// Show on screen
					glPopMatrix();
					glfwSwapBuffers(win);
					
					// Break condition
					if (!saver.isSaving()) break;
				}
			}
		} catch (...) {
			debugMessage("Error in GUI update loop", DEBUG_ERROR);
		}
	}

	void showFrame(size_t bufInd, BaseFrame& frame, int rx, int ry, int rw, int rh, const std::string caption = "") {
		if (!frame.isValid()) return;
		char* frameData = new char[frame.getBytes()];
		frame.copyDataToBuffer(frameData);
		buffers[bufInd].show(frameData, (int) frame.getWidth(), (int) frame.getHeight(), formats[bufInd], caption, rx, ry, rw, rh);
		delete[] frameData;
	}

	void close() {
		shouldClose = true;
	}
};