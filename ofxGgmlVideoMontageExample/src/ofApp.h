#pragma once

#include "ofMain.h"
#include "ofxGgmlVideo.h"
#include "ofxImGui.h"

#include <future>
#include <string>
#include <vector>

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void dragEvent(ofDragInfo dragInfo) override;

private:
	bool loadVideo(const std::string & path);
	void chooseVideo();
	void chooseVisionModel();
	void chooseVisionMmproj();
	void startRender(bool modelBacked);
	void drawVideoInput();
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
	std::string videoPath;
	std::string renderOutputPath;
	std::string activeRenderOutputPath;
	std::string activeManifestPath;
	std::string workflowScript;
	std::string visionModel;
	std::string visionModelPath;
	std::string visionMmprojPath;
	std::string visionServerUrl = "http://127.0.0.1:8080";
	std::string renderStatus = "Drop a video here or choose a file.";
	int selectedSegment = 0;
	bool videoLoaded = false;
	bool renderRunning = false;
	bool videoPaused = false;
	bool useExternalVisionServer = false;
	int localVisionBackendIndex = 0;
	std::future<int> renderFuture;
	ofVideoPlayer videoPlayer;
	ofxImGui::Gui gui;
};
