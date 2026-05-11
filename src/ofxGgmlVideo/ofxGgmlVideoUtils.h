#pragma once

#include "ofxGgmlVideoTypes.h"

#include <string>

namespace ofxGgmlVideoUtils {
	bool hasInput(const ofxGgmlVideoRequest & request);
	std::string describe(const ofxGgmlVideoRequest & request);
}