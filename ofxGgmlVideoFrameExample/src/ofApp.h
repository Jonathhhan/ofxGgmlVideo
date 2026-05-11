#pragma once

#include "ofMain.h"
#include "ofxGgmlVideo.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void draw() override;

private:
	ofxGgmlVideoRequest request;
	std::string status;
};