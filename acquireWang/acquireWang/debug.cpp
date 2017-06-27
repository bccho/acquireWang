#include "debug.h"

DebugTimers timers(10);
/*
	0: overall
	1: prep
	2: cleanup
	3: from start-stop acquisitio
	4: saving
	5: copying frames
	6: getting frames
	7: moving frames to write buffers
	8: dequeueing frames
*/

void printDebugTimerInfo() {
	debugMessage("Overall:                    " + std::to_string(timers.getTotalTime(0)), DEBUG_INFO);
	debugMessage("Main thread:", DEBUG_INFO);
	debugMessage("  Initialiation:            " + std::to_string(timers.getTotalTime(1)), DEBUG_INFO);
	debugMessage("  Finalization:             " + std::to_string(timers.getTotalTime(2)), DEBUG_INFO);
	debugMessage("  Acquisition:              " + std::to_string(timers.getTotalTime(3)), DEBUG_INFO);
	debugMessage("Acquisition threads (total):", DEBUG_INFO);
	debugMessage("  Getting frames:           " + std::to_string(timers.getTotalTime(6)), DEBUG_INFO);
	debugMessage("Saving thread:", DEBUG_INFO);
	debugMessage("  Writing frames:           " + std::to_string(timers.getTotalTime(4)), DEBUG_INFO);
	debugMessage("  Moving frames to buffers: " + std::to_string(timers.getTotalTime(7)), DEBUG_INFO);
	debugMessage("    Dequeueing frames:      " + std::to_string(timers.getTotalTime(7)), DEBUG_INFO);
	debugMessage("General:", DEBUG_INFO);
	debugMessage("  Copying frames:           " + std::to_string(timers.getTotalTime(5)), DEBUG_INFO);
}
