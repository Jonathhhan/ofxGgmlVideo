#pragma once

#include <string>
#include <vector>

struct ofxGgmlVideoTemporalWindow {
	double startSeconds = 0.0;
	double durationSeconds = 0.0;
	double sampleRateFps = 1.0;
	int maxFrames = 0;
};

struct ofxGgmlVideoFrameSample {
	int index = 0;
	double timeSeconds = 0.0;
	std::string reference;
};

struct ofxGgmlVideoRequest {
	std::string videoPath;
	std::string prompt;
	std::vector<std::string> tags;
	ofxGgmlVideoTemporalWindow temporalWindow;
};

struct ofxGgmlVideoMontageTransition {
	std::string kind = "cut";
	double durationSeconds = 0.0;
};

struct ofxGgmlVideoMontageOptions {
	std::string prompt;
	std::string defaultTransitionKind = "cut";
	double transitionSeconds = 0.0;
	double handleSeconds = 0.0;
	bool overlapTransitions = false;
};

struct ofxGgmlVideoResult {
	bool success = false;
	std::string text;
	std::string error;
	std::vector<std::string> references;
	std::vector<ofxGgmlVideoFrameSample> frameSamples;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlVideoMontageSegment {
	int index = 0;
	std::string sourcePath;
	std::string label;
	double sourceStartSeconds = 0.0;
	double sourceEndSeconds = 0.0;
	double durationSeconds = 0.0;
	double timelineStartSeconds = 0.0;
	double timelineEndSeconds = 0.0;
	double handleInSeconds = 0.0;
	double handleOutSeconds = 0.0;
	ofxGgmlVideoMontageTransition transitionIn;
	ofxGgmlVideoMontageTransition transitionOut;
	std::vector<std::string> tags;
	std::vector<std::string> references;
	std::vector<ofxGgmlVideoFrameSample> frameSamples;
};

struct ofxGgmlVideoMontagePlan {
	bool success = false;
	std::string prompt;
	std::string text;
	std::string error;
	double durationSeconds = 0.0;
	double handleSeconds = 0.0;
	double transitionSeconds = 0.0;
	std::string transitionKind = "cut";
	bool overlapTransitions = false;
	std::vector<std::string> references;
	std::vector<ofxGgmlVideoMontageSegment> segments;

	explicit operator bool() const {
		return success;
	}
};

struct ofxGgmlVideoMontageHandoffSegment {
	int segmentIndex = 0;
	std::string sourcePath;
	std::string label;
	std::string agentReason;
	std::string temporalSummary;
	std::string scoreKind = "unscored";
	double score = 0.0;
	std::string embeddingReference;
	int embeddingDimensions = 0;
	std::string bridgeOutputKind;
	std::string bridgeOutputReference;
	std::vector<std::string> references;
};

struct ofxGgmlVideoMontageHandoff {
	bool success = false;
	std::string contractVersion = "montage-handoff-v1";
	std::string decisionOwner = "ofxGgmlAgents";
	std::string scoringOwner = "ofxGgmlVision";
	std::string bridgeOwner = "app-layer";
	std::string prompt;
	std::string text;
	std::string error;
	std::vector<std::string> warnings;
	std::vector<std::string> references;
	std::vector<ofxGgmlVideoMontageHandoffSegment> segments;

	explicit operator bool() const {
		return success;
	}
};
