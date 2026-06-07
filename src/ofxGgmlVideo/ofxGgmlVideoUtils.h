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
	std::vector<ofxGgmlVideoMontageMarker> planBeatMarkers(double durationSeconds, double bpm, int beatsPerBar = 4);
	ofxGgmlVideoResult planClip(const ofxGgmlVideoRequest & request);
	ofxGgmlVideoMontageSegment makeMontageSegment(const ofxGgmlVideoRequest & request, int index, double timelineStartSeconds);
	ofxGgmlVideoMontageSegment makeMontageSegment(const ofxGgmlVideoRequest & request, int index, double timelineStartSeconds, const ofxGgmlVideoMontageOptions & options);
	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const std::string & prompt = "");
	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const ofxGgmlVideoMontageOptions & options);
	std::string toMontageEdl(const ofxGgmlVideoMontagePlan & plan);
	std::string toMontageManifestJson(const ofxGgmlVideoMontagePlan & plan);
	ofxGgmlVideoMontageHandoff makeMontageHandoff(const ofxGgmlVideoMontagePlan & plan,
	                                               const std::string & decisionOwner = "ofxGgmlAgents",
	                                               const std::string & scoringOwner = "ofxGgmlVision",
	                                               const std::string & bridgeOwner = "app-layer");
	std::string toMontageHandoffText(const ofxGgmlVideoMontageHandoff & handoff);
}
