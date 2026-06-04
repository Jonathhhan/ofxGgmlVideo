#include "ofApp.h"

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlVideo smoke example");
	gui.setup(nullptr, false);
	request.videoPath = "videos/clip.mp4";
	request.prompt = "prepare frame samples for a vision/CLIP scorer";
	request.temporalWindow.startSeconds = 0.0;
	request.temporalWindow.durationSeconds = 3.0;
	request.temporalWindow.sampleRateFps = 1.0;
	request.temporalWindow.maxFrames = 3;
	montagePlan = ofxGgmlVideoUtils::planMontage({request}, "montageautomat draft");
	status = ofxGgmlVideoUtils::describe(montagePlan);
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
		ImGui::Spacing();
		ImGui::TextUnformatted("Montage Segments");
		for (const auto & segment : montagePlan.segments) {
			ImGui::BulletText("%02d %.3fs + %.3fs %s",
				segment.index,
				segment.timelineStartSeconds,
				segment.durationSeconds,
				segment.label.c_str());
			for (const auto & reference : segment.references) {
				ImGui::TextWrapped("  %s", reference.c_str());
			}
		}
	}
	ImGui::End();
	gui.end();
	gui.draw();
}
