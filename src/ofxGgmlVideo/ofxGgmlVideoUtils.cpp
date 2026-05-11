#include "ofxGgmlVideoUtils.h"

namespace ofxGgmlVideoUtils {
	bool hasInput(const ofxGgmlVideoRequest & request) {
		return !request.videoPath.empty();
	}

	std::string describe(const ofxGgmlVideoRequest & request) {
		if (!hasInput(request)) {
			return "video: empty request";
		}
		return "video: " + request.videoPath;
	}
}