#pragma once

#include "ofxGgmlVideoTypes.h"

#include <string>
#include <vector>

namespace ofxGgmlVideoUtils {
	bool hasInput(const ofxGgmlVideoRequest & request);
	bool hasTemporalWindow(const ofxGgmlVideoRequest & request);
	bool isValid(const ofxGgmlVideoTemporalWindow & window);
	std::string describe(const ofxGgmlVideoRequest & request);
	std::string describe(const ofxGgmlVideoMontagePlan & plan);
	std::vector<ofxGgmlVideoFrameSample> planFrameSamples(const ofxGgmlVideoTemporalWindow & window);
	ofxGgmlVideoResult planClip(const ofxGgmlVideoRequest & request);
	ofxGgmlVideoMontageSegment makeMontageSegment(const ofxGgmlVideoRequest & request, int index, double timelineStartSeconds);
	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const std::string & prompt = "");
	std::string toMontageEdl(const ofxGgmlVideoMontagePlan & plan);
}
