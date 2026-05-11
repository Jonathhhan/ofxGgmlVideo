#include "ofxGgmlVideo.h"

#include <iostream>

int main() {
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