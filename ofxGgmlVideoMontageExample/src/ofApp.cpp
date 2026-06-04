#include "ofApp.h"

#include <algorithm>

namespace {
	ofxGgmlVideoRequest makeClip(const std::string & path,
	                             const std::string & label,
	                             const double startSeconds,
	                             const double durationSeconds,
	                             const double sampleRateFps,
	                             const int maxFrames,
	                             const std::vector<std::string> & tags) {
		ofxGgmlVideoRequest request;
		request.videoPath = path;
		request.prompt = label;
		request.tags = tags;
		request.temporalWindow.startSeconds = startSeconds;
		request.temporalWindow.durationSeconds = durationSeconds;
		request.temporalWindow.sampleRateFps = sampleRateFps;
		request.temporalWindow.maxFrames = maxFrames;
		return request;
	}
}

void ofApp::setup() {
	ofSetWindowTitle("ofxGgmlVideo montage example");
	gui.setup(nullptr, false);

	clips.push_back(makeClip("videos/interview.mp4", "opening statement", 4.0, 3.0, 1.0, 3, {"dialog", "select"}));
	clips.push_back(makeClip("videos/broll.mp4", "hands and process", 18.0, 4.0, 1.0, 4, {"broll", "texture"}));
	clips.push_back(makeClip("videos/reaction.mp4", "reaction beat", 8.0, 2.0, 2.0, 4, {"reaction", "closeup"}));

	montageOptions.prompt = "montageautomat assembly";
	montageOptions.defaultTransitionKind = "crossfade";
	montageOptions.transitionSeconds = 0.5;
	montageOptions.handleSeconds = 0.25;
	montageOptions.overlapTransitions = true;

	rebuildMontage();
}

void ofApp::rebuildMontage() {
	montagePlan = ofxGgmlVideoUtils::planMontage(clips, montageOptions);
	status = ofxGgmlVideoUtils::describe(montagePlan);
	editDecisionList = ofxGgmlVideoUtils::toMontageEdl(montagePlan);
	montageHandoff = ofxGgmlVideoUtils::makeMontageHandoff(montagePlan);
	handoffText = ofxGgmlVideoUtils::toMontageHandoffText(montageHandoff);
	selectedSegment = std::min(selectedSegment, std::max(0, static_cast<int>(montagePlan.segments.size()) - 1));
	ofLogNotice("ofxGgmlVideoMontageExample") << status;
}

void ofApp::draw() {
	ofBackground(16);

	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_Once);
	if (ImGui::Begin("ofxGgmlVideo Montage Example")) {
		ImGui::TextWrapped("%s", status.c_str());
		ImGui::Separator();
		drawClipControls();
		ImGui::Separator();
		drawTimeline();
		ImGui::Separator();
		drawSegmentDetails();
		ImGui::Separator();
		ImGui::TextUnformatted("Edit Decision List");
		ImGui::BeginChild("edl", ImVec2(0.0f, 120.0f), true);
		ImGui::TextUnformatted(editDecisionList.c_str());
		ImGui::EndChild();
		ImGui::TextUnformatted("Montage Handoff");
		ImGui::BeginChild("handoff", ImVec2(0.0f, 120.0f), true);
		ImGui::TextUnformatted(handoffText.c_str());
		ImGui::EndChild();
	}
	ImGui::End();
	gui.end();
	gui.draw();
}

void ofApp::drawClipControls() {
	bool changed = false;
	ImGui::TextUnformatted("Montage options");
	float transitionSeconds = static_cast<float>(montageOptions.transitionSeconds);
	float handleSeconds = static_cast<float>(montageOptions.handleSeconds);
	bool overlapTransitions = montageOptions.overlapTransitions;
	const char * transitionKinds[] = {"cut", "crossfade", "dip", "wipe"};
	int transitionIndex = 0;
	for (int i = 0; i < 4; ++i) {
		if (montageOptions.defaultTransitionKind == transitionKinds[i]) {
			transitionIndex = i;
			break;
		}
	}
	changed |= ImGui::Combo("transition", &transitionIndex, transitionKinds, 4);
	changed |= ImGui::DragFloat("transition duration", &transitionSeconds, 0.05f, 0.0f, 10.0f, "%.2fs");
	changed |= ImGui::DragFloat("source handles", &handleSeconds, 0.05f, 0.0f, 30.0f, "%.2fs");
	changed |= ImGui::Checkbox("overlap transitions", &overlapTransitions);
	montageOptions.defaultTransitionKind = transitionKinds[transitionIndex];
	montageOptions.transitionSeconds = transitionSeconds;
	montageOptions.handleSeconds = handleSeconds;
	montageOptions.overlapTransitions = overlapTransitions;

	ImGui::Separator();
	ImGui::TextUnformatted("Clips");
	for (std::size_t i = 0; i < clips.size(); ++i) {
		auto & clip = clips[i];
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::CollapsingHeader(clip.prompt.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextWrapped("%s", clip.videoPath.c_str());

			float start = static_cast<float>(clip.temporalWindow.startSeconds);
			float duration = static_cast<float>(clip.temporalWindow.durationSeconds);
			float sampleRate = static_cast<float>(clip.temporalWindow.sampleRateFps);
			int maxFrames = clip.temporalWindow.maxFrames;

			changed |= ImGui::DragFloat("source start", &start, 0.1f, 0.0f, 3600.0f, "%.2fs");
			changed |= ImGui::DragFloat("duration", &duration, 0.1f, 0.1f, 600.0f, "%.2fs");
			changed |= ImGui::DragFloat("sample fps", &sampleRate, 0.1f, 0.1f, 60.0f, "%.2f");
			changed |= ImGui::SliderInt("max frames", &maxFrames, 0, 300);

			clip.temporalWindow.startSeconds = start;
			clip.temporalWindow.durationSeconds = duration;
			clip.temporalWindow.sampleRateFps = sampleRate;
			clip.temporalWindow.maxFrames = maxFrames;
		}
		ImGui::PopID();
	}

	if (changed || ImGui::Button("Rebuild Montage")) {
		rebuildMontage();
	}
}

void ofApp::drawTimeline() {
	ImGui::TextUnformatted("Timeline");
	const auto canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 120.0f);
	ImDrawList * drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasPos,
		ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
		IM_COL32(28, 32, 38, 255));
	drawList->AddRect(canvasPos,
		ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
		IM_COL32(90, 100, 110, 255));

	const float usableWidth = std::max(1.0f, canvasSize.x - 24.0f);
	const float scale = montagePlan.durationSeconds > 0.0
		? usableWidth / static_cast<float>(montagePlan.durationSeconds)
		: usableWidth;

	for (const auto & segment : montagePlan.segments) {
		const float x = canvasPos.x + 12.0f + static_cast<float>(segment.timelineStartSeconds) * scale;
		const float y = canvasPos.y + 20.0f + static_cast<float>(segment.index) * 28.0f;
		const float w = std::max(6.0f, static_cast<float>(segment.durationSeconds) * scale);
		const ImU32 color = segment.index % 2 == 0 ? IM_COL32(67, 132, 180, 255) : IM_COL32(88, 160, 104, 255);
		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + 20.0f), color, 3.0f);
		if (segment.transitionIn.durationSeconds > 0.0) {
			const float transitionWidth = static_cast<float>(segment.transitionIn.durationSeconds) * scale;
			drawList->AddRectFilled(ImVec2(x, y),
				ImVec2(x + transitionWidth, y + 20.0f),
				IM_COL32(240, 190, 80, 140), 3.0f);
		}
		if (segment.transitionOut.durationSeconds > 0.0) {
			const float transitionWidth = static_cast<float>(segment.transitionOut.durationSeconds) * scale;
			drawList->AddRectFilled(ImVec2(x + w - transitionWidth, y),
				ImVec2(x + w, y + 20.0f),
				IM_COL32(240, 190, 80, 140), 3.0f);
		}
		drawList->AddText(ImVec2(x + 6.0f, y + 3.0f), IM_COL32(255, 255, 255, 255), segment.label.c_str());
	}

	ImGui::Dummy(canvasSize);
}

void ofApp::drawSegmentDetails() {
	ImGui::TextUnformatted("Segments");
	for (const auto & segment : montagePlan.segments) {
		ImGui::PushID(segment.index);
		const bool selected = selectedSegment == segment.index;
		if (ImGui::Selectable(segment.label.c_str(), selected)) {
			selectedSegment = segment.index;
		}
		ImGui::PopID();
	}

	if (selectedSegment >= 0 && selectedSegment < static_cast<int>(montagePlan.segments.size())) {
		const auto & segment = montagePlan.segments[static_cast<std::size_t>(selectedSegment)];
		ImGui::Spacing();
		ImGui::TextWrapped("source: %s", segment.sourcePath.c_str());
		ImGui::Text("timeline: %.3fs - %.3fs (+%.3fs)", segment.timelineStartSeconds, segment.timelineEndSeconds, segment.durationSeconds);
		ImGui::Text("source: %.3fs - %.3fs (+%.3fs)", segment.sourceStartSeconds, segment.sourceEndSeconds, segment.durationSeconds);
		ImGui::Text("handles: in %.3fs out %.3fs", segment.handleInSeconds, segment.handleOutSeconds);
		ImGui::Text("transition in: %s %.3fs", segment.transitionIn.kind.c_str(), segment.transitionIn.durationSeconds);
		ImGui::Text("transition out: %s %.3fs", segment.transitionOut.kind.c_str(), segment.transitionOut.durationSeconds);
		ImGui::TextUnformatted("frame references");
		for (const auto & reference : segment.references) {
			ImGui::BulletText("%s", reference.c_str());
		}
	}
}
