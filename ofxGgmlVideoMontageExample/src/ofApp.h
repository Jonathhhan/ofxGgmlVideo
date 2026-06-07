#pragma once

#include "ofMain.h"
#include "ofxGgmlVideo.h"
#include "ofxImGui.h"

#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	void rebuildMontage();
	void drawClipControls();
	void drawTimeline();
	void drawSegmentDetails();

	std::vector<ofxGgmlVideoRequest> clips;
	ofxGgmlVideoMontageOptions montageOptions;
	ofxGgmlVideoMontagePlan montagePlan;
	ofxGgmlVideoMontageHandoff montageHandoff;
	std::string status;
	std::string editDecisionList;
	std::string manifestJson;
	std::string handoffText;
	int selectedSegment = 0;
	ofxImGui::Gui gui;
};
