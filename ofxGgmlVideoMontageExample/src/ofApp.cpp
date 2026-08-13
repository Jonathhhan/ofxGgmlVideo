#include "ofApp.h"

#include "ImHelpers.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace {
	std::string getEnvironmentValue(const char * name) {
#ifdef TARGET_WIN32
		char * value = nullptr;
		size_t valueLength = 0;
		if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr) {
			return {};
		}
		const std::string result(value);
		std::free(value);
		return result;
#else
		const char * value = std::getenv(name);
		return value == nullptr ? std::string{} : std::string(value);
#endif
	}

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
	// ofxImGui::AddImage only accepts normalized GL_TEXTURE_2D textures.
	// Set the openFrameworks allocation mode before ofVideoPlayer loads a frame.
	ofDisableArbTex();
	ofSetWindowTitle("ofxGgmlVideo montage example");
	gui.setup(nullptr, false);
	const auto addonRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
	workflowScript = (addonRoot / "scripts" / "run-video-montage-workflow.ps1").string();
	if (const auto configuredModel = getEnvironmentValue("OFXGGML_VISION_SERVER_MODEL"); !configuredModel.empty()) {
		visionModel = configuredModel;
		useExternalVisionServer = true;
	}
	if (const auto configuredServer = getEnvironmentValue("OFXGGML_VISION_SERVER_URL"); !configuredServer.empty()) {
		visionServerUrl = configuredServer;
	}
	if (const auto configuredModelPath = getEnvironmentValue("OFXGGML_VISION_MODEL"); !configuredModelPath.empty()) {
		visionModelPath = configuredModelPath;
		useExternalVisionServer = false;
	}
	if (const auto configuredMmprojPath = getEnvironmentValue("OFXGGML_VISION_MMPROJ"); !configuredMmprojPath.empty()) {
		visionMmprojPath = configuredMmprojPath;
	}
	if (const auto configuredGpuLayers = getEnvironmentValue("OFXGGML_VISION_GPU_LAYERS"); configuredGpuLayers == "0") {
		localVisionBackendIndex = 1;
	}

	clips.push_back(makeClip("videos/interview.mp4", "opening statement", 4.0, 3.0, 1.0, 3, {"dialog", "select"}));
	clips.push_back(makeClip("videos/broll.mp4", "hands and process", 18.0, 4.0, 1.0, 4, {"broll", "texture"}));
	clips.push_back(makeClip("videos/reaction.mp4", "reaction beat", 8.0, 2.0, 2.0, 4, {"reaction", "closeup"}));

	montageOptions.prompt = "montageautomat assembly";
	montageOptions.defaultTransitionKind = "crossfade";
	montageOptions.transitionSeconds = 0.5;
	montageOptions.handleSeconds = 0.25;
	montageOptions.beatBpm = 120.0;
	montageOptions.beatsPerBar = 4;
	montageOptions.overlapTransitions = true;

	rebuildMontage();
}

void ofApp::update() {
	if (videoLoaded) {
		videoPlayer.update();
	}
	if (renderRunning && renderFuture.valid() &&
		renderFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
		const int exitCode = renderFuture.get();
		renderRunning = false;
		if (exitCode == 0 && ofFile::doesFileExist(activeRenderOutputPath)) {
			renderStatus = "Rendered planned clip windows as MP4 (hard cuts): " + activeRenderOutputPath;
			ofLogNotice("ofxGgmlVideoMontageExample") << renderStatus;
		} else {
			renderStatus = "Render failed with exit code " + ofToString(exitCode) + ". See the console for the first FFmpeg or Vision error.";
			ofLogError("ofxGgmlVideoMontageExample") << renderStatus;
		}
		if (!activeManifestPath.empty()) {
			std::error_code removeError;
			std::filesystem::remove(activeManifestPath, removeError);
			if (removeError) {
				ofLogWarning("ofxGgmlVideoMontageExample")
					<< "Could not remove temporary montage manifest: " << removeError.message();
			}
			activeManifestPath.clear();
		}
		activeRenderOutputPath.clear();
	}
}

bool ofApp::loadVideo(const std::string & path) {
	videoPlayer.close();
	if (!videoPlayer.load(path)) {
		videoLoaded = false;
		renderStatus = "Could not decode video: " + path;
		ofLogError("ofxGgmlVideoMontageExample") << renderStatus;
		return false;
	}

	videoPath = ofFilePath::getAbsolutePath(path);
	videoLoaded = true;
	videoPaused = false;
	videoPlayer.setLoopState(OF_LOOP_NORMAL);
	videoPlayer.play();
	const double sourceDuration = std::max(0.1, static_cast<double>(videoPlayer.getDuration()));
	const double clipDuration = std::min(4.0, std::max(0.5, sourceDuration / 6.0));
	clips.clear();
	for (int index = 0; index < 3; ++index) {
		const double center = (static_cast<double>(index) + 0.5) * sourceDuration / 3.0;
		const double start = std::max(0.0, std::min(sourceDuration - clipDuration, center - clipDuration / 2.0));
		clips.push_back(makeClip(videoPath,
			"sample " + ofToString(index + 1),
			start,
			clipDuration,
			1.0,
			4,
			{"decoded-video", "sample"}));
	}
	const std::filesystem::path inputPath(videoPath);
	renderOutputPath = (inputPath.parent_path() / (inputPath.stem().string() + ".montage.mp4")).string();
	renderStatus = "Loaded " + videoPath + " (" + ofToString(sourceDuration, 2) + "s).";
	rebuildMontage();
	return true;
}

void ofApp::chooseVideo() {
	auto result = ofSystemLoadDialog("Choose a video for the montage workflow", false);
	if (result.bSuccess) {
		loadVideo(result.getPath());
	}
}

void ofApp::chooseVisionModel() {
	auto result = ofSystemLoadDialog("Choose local Vision model GGUF", false, visionModelPath);
	if (result.bSuccess) {
		visionModelPath = result.getPath();
		useExternalVisionServer = false;
		renderStatus = "Selected local Vision model: " + visionModelPath;
	}
}

void ofApp::chooseVisionMmproj() {
	auto result = ofSystemLoadDialog("Choose matching Vision mmproj GGUF", false, visionMmprojPath);
	if (result.bSuccess) {
		visionMmprojPath = result.getPath();
		useExternalVisionServer = false;
		renderStatus = "Selected local Vision projector: " + visionMmprojPath;
	}
}

void ofApp::dragEvent(ofDragInfo dragInfo) {
	if (!dragInfo.files.empty()) {
		loadVideo(dragInfo.files.front().string());
	}
}

void ofApp::startRender(const bool modelBacked) {
	if (!videoLoaded || renderRunning) {
		return;
	}
	if (!ofFile::doesFileExist(workflowScript)) {
		renderStatus = "Workflow script was not found: " + workflowScript;
		ofLogError("ofxGgmlVideoMontageExample") << renderStatus;
		return;
	}
	if (!montagePlan || montagePlan.segments.empty() || manifestJson.empty()) {
		renderStatus = "Build a valid montage with at least one clip before rendering.";
		ofLogError("ofxGgmlVideoMontageExample") << renderStatus;
		return;
	}
	if (modelBacked) {
		if (useExternalVisionServer && visionModel.empty()) {
			renderStatus = "Enter the external Vision model ID before starting a model-ranked render.";
			return;
		}
		if (!useExternalVisionServer && !ofFile::doesFileExist(visionModelPath)) {
			renderStatus = "Choose a readable local Vision model GGUF first.";
			return;
		}
		if (!useExternalVisionServer && !ofFile::doesFileExist(visionMmprojPath)) {
			renderStatus = "Choose the matching local Vision mmproj GGUF first.";
			return;
		}
	}

	auto quote = [](std::string value) {
		std::replace(value.begin(), value.end(), '"', '\'');
		return "\"" + value + "\"";
	};
	const auto manifestPath = std::filesystem::temp_directory_path() /
		("ofxGgmlVideo-montage-" + ofToString(ofGetSystemTimeMillis()) + ".json");
	ofBuffer manifestBuffer;
	manifestBuffer.set(manifestJson.data(), manifestJson.size());
	if (!ofBufferToFile(manifestPath.string(), manifestBuffer, false)) {
		renderStatus = "Could not write the temporary montage manifest: " + manifestPath.string();
		ofLogError("ofxGgmlVideoMontageExample") << renderStatus;
		return;
	}
	std::ostringstream command;
	command << "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " << quote(workflowScript)
		<< " -Video " << quote(videoPath)
		<< " -OutputPath " << quote(renderOutputPath)
		<< " -MontagePrompt " << quote(montageOptions.prompt)
		<< " -MontageManifestPath " << quote(manifestPath.string())
		<< " -MaxOutputSegments " << montagePlan.segments.size();
	if (modelBacked) {
		if (useExternalVisionServer) {
			command << " -VisionModel " << quote(visionModel)
				<< " -VisionServerUrl " << quote(visionServerUrl);
		} else {
			command << " -VisionModelPath " << quote(visionModelPath)
				<< " -VisionMmprojPath " << quote(visionMmprojPath)
				<< " -VisionBackend " << (localVisionBackendIndex == 0 ? "cuda" : "cpu");
		}
	} else {
		command << " -SkipVision";
	}

	const std::string localBackendLabel = localVisionBackendIndex == 0 ? "CUDA" : "CPU";
	renderStatus = modelBacked
		? (useExternalVisionServer
			? "External Vision ranking of the planned clip windows and MP4 render running..."
			: "Local " + localBackendLabel + " Vision ranking of the planned clip windows and MP4 render running...")
		: "Rendering the planned clip windows as a deterministic MP4...";
	renderRunning = true;
	activeRenderOutputPath = renderOutputPath;
	activeManifestPath = manifestPath.string();
	const std::string commandText = command.str();
	renderFuture = std::async(std::launch::async, [commandText]() {
		return std::system(commandText.c_str());
	});
}

void ofApp::rebuildMontage() {
	montagePlan = ofxGgmlVideoUtils::planMontage(clips, montageOptions);
	status = ofxGgmlVideoUtils::describe(montagePlan);
	editDecisionList = ofxGgmlVideoUtils::toMontageEdl(montagePlan);
	manifestJson = ofxGgmlVideoUtils::toMontageManifestJson(montagePlan);
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
		drawVideoInput();
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
		ImGui::TextUnformatted("Manifest JSON");
		ImGui::BeginChild("manifest", ImVec2(0.0f, 140.0f), true);
		ImGui::TextUnformatted(manifestJson.c_str());
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

void ofApp::drawVideoInput() {
	ImGui::TextUnformatted("User video workflow");
	if (ImGui::Button("Choose video...")) {
		chooseVideo();
	}
	ImGui::SameLine();
	ImGui::TextWrapped("%s", videoLoaded ? videoPath.c_str() : "Drop an MP4, MOV, or other FFmpeg/openFrameworks-readable video onto the window.");

	if (videoLoaded) {
		const float sourceWidth = std::max(1.0f, videoPlayer.getWidth());
		const float sourceHeight = std::max(1.0f, videoPlayer.getHeight());
		const float previewWidth = std::min(520.0f, ImGui::GetContentRegionAvail().x);
		const float previewHeight = previewWidth * sourceHeight / sourceWidth;
		if (videoPlayer.getTexture().isAllocated()) {
			ofxImGui::AddImage(videoPlayer.getTexture(), glm::vec2(previewWidth, previewHeight));
		}
		float position = videoPlayer.getPosition();
		if (ImGui::SliderFloat("preview position", &position, 0.0f, 1.0f, "%.3f")) {
			videoPlayer.setPosition(position);
		}
		if (ImGui::Button(videoPaused ? "Play preview" : "Pause preview")) {
			videoPaused = !videoPaused;
			videoPlayer.setPaused(videoPaused);
		}
		ImGui::SameLine();
		ImGui::Text("%.2fs / %.2fs", videoPlayer.getPosition() * videoPlayer.getDuration(), videoPlayer.getDuration());

		ImGui::InputText("output MP4", &renderOutputPath);
		ImGui::Checkbox("Use external Vision server", &useExternalVisionServer);
		if (useExternalVisionServer) {
			ImGui::InputText("Vision model ID", &visionModel);
			ImGui::InputText("Vision server URL", &visionServerUrl);
		} else {
			const char * localBackends[] = {"CUDA", "CPU"};
			ImGui::Combo("Local Vision backend", &localVisionBackendIndex, localBackends, 2);
			ImGui::Text("Selected backend: %s", localBackends[localVisionBackendIndex]);
			ImGui::InputText("Vision model GGUF", &visionModelPath);
			if (ImGui::Button("Browse local Vision model GGUF...")) {
				chooseVisionModel();
			}
			ImGui::InputText("Vision mmproj GGUF", &visionMmprojPath);
			if (ImGui::Button("Browse Vision mmproj GGUF...")) {
				chooseVisionMmproj();
			}
			ImGui::TextUnformatted(localVisionBackendIndex == 0
				? "CUDA offloads model layers to the GPU."
				: "CPU keeps all model layers on the CPU.");
		}
		if (ImGui::Button("Render deterministic MP4")) {
			startRender(false);
		}
		ImGui::SameLine();
		if (ImGui::Button(useExternalVisionServer
			? "Render Vision-ranked MP4"
			: (localVisionBackendIndex == 0
				? "Render Vision-ranked MP4 (local CUDA)"
				: "Render Vision-ranked MP4 (local CPU)"))) {
			startRender(true);
		}
		ImGui::TextUnformatted("Source windows come from the visible montage; transition metadata currently renders as hard cuts.");
	}
	if (renderRunning) {
		ImGui::TextUnformatted("Render running; the UI remains responsive.");
	}
	ImGui::TextWrapped("%s", renderStatus.c_str());
}

void ofApp::drawClipControls() {
	bool changed = false;
	ImGui::TextUnformatted("Montage options");
	changed |= ImGui::InputText("montage prompt", &montageOptions.prompt);
	float transitionSeconds = static_cast<float>(montageOptions.transitionSeconds);
	float handleSeconds = static_cast<float>(montageOptions.handleSeconds);
	float beatBpm = static_cast<float>(montageOptions.beatBpm);
	int beatsPerBar = montageOptions.beatsPerBar;
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
	changed |= ImGui::DragFloat("beat bpm", &beatBpm, 1.0f, 0.0f, 300.0f, "%.1f");
	changed |= ImGui::SliderInt("beats per bar", &beatsPerBar, 1, 16);
	changed |= ImGui::Checkbox("overlap transitions", &overlapTransitions);
	montageOptions.defaultTransitionKind = transitionKinds[transitionIndex];
	montageOptions.transitionSeconds = transitionSeconds;
	montageOptions.handleSeconds = handleSeconds;
	montageOptions.beatBpm = beatBpm;
	montageOptions.beatsPerBar = beatsPerBar;
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

	for (const auto & marker : montagePlan.markers) {
		const float x = canvasPos.x + 12.0f + static_cast<float>(marker.timelineSeconds) * scale;
		const ImU32 color = marker.kind == "bar" ? IM_COL32(255, 255, 255, 150) : IM_COL32(180, 190, 200, 80);
		drawList->AddLine(ImVec2(x, canvasPos.y + 8.0f), ImVec2(x, canvasPos.y + canvasSize.y - 8.0f), color, marker.kind == "bar" ? 2.0f : 1.0f);
		if (marker.kind == "bar") {
			drawList->AddText(ImVec2(x + 3.0f, canvasPos.y + 4.0f), IM_COL32(255, 255, 255, 210), marker.label.c_str());
		}
	}

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
