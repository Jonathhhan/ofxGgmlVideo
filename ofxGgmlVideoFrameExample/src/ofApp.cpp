#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlVideo smoke example");
	request.videoPath = "videos/clip.mp4";
	status = ofxGgmlVideoUtils::describe(request);
	ofLogNotice("ofxGgmlVideoFrameExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	ofSetColor(240);
	ofDrawBitmapString("ofxGgmlVideo", 32, 48);
	ofDrawBitmapString(status, 32, 78);
}