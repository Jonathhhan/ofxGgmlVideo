#pragma once

#include "ofMain.h"
#include "ofxGgmlVideo.h"
#include "ofxImGui.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlVideoRequest request;
	std::string status;
	ofxImGui::Gui gui;
};
