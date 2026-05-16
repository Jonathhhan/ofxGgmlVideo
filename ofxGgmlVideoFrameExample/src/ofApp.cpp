#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlVideo smoke example");
	gui.setup(nullptr, false);
	request.videoPath = "videos/clip.mp4";
	status = ofxGgmlVideoUtils::describe(request);
	ofLogNotice("ofxGgmlVideoFrameExample") << status;
}

void ofApp::draw() {
	ofBackground(18);
	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(560.0f, 220.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlVideo Frame Example")) {
		ImGui::TextUnformatted("Video Request");
		ImGui::Separator();
		ImGui::TextWrapped("%s", status.c_str());
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
