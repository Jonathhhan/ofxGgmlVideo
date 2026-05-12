#include "ofxGgmlVideo.h"

#include <iostream>

int main() {
	if (OFXGGML_VIDEO_VERSION_MAJOR != 1 ||
		OFXGGML_VIDEO_VERSION_MINOR != 0 ||
		OFXGGML_VIDEO_VERSION_PATCH != 1 ||
		std::string(OFXGGML_VIDEO_VERSION_STRING) != "1.0.1" ||
		std::string(ofxGgmlVideoGetVersionString()) != "1.0.1") {
		std::cerr << "unexpected video addon version metadata\n";
		return 1;
	}

	ofxGgmlVideoRequest request;
	if (ofxGgmlVideoUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.videoPath = "videos/clip.mp4";
	if (!ofxGgmlVideoUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto description = ofxGgmlVideoUtils::describe(request);
	if (description.find(request.videoPath) == std::string::npos) {
		std::cerr << "description did not include request input\n";
		return 1;
	}

	return 0;
}
