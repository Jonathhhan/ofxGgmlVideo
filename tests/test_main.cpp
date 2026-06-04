#include "ofxGgmlVideo.h"

#include <iostream>

int main() {
	if (OFXGGML_VIDEO_VERSION_MAJOR != 1 ||
		OFXGGML_VIDEO_VERSION_MINOR != 0 ||
		OFXGGML_VIDEO_VERSION_PATCH != 1 ||
		std::string(OFXGGML_VIDEO_VERSION_STRING) != "1.0.1" ||
		std::string(ofxGgmlVideoGetVersionString()) != "1.0.1") {
		std::cerr << "unexpected video addon version metadata\n";
		return 1;
	}

	ofxGgmlVideoRequest request;
	if (ofxGgmlVideoUtils::hasInput(request)) {
		std::cerr << "empty request reported as configured\n";
		return 1;
	}

	request.videoPath = "videos/clip.mp4";
	if (!ofxGgmlVideoUtils::hasInput(request)) {
		std::cerr << "configured request reported as empty\n";
		return 1;
	}

	const auto description = ofxGgmlVideoUtils::describe(request);
	if (description.find(request.videoPath) == std::string::npos) {
		std::cerr << "description did not include request input\n";
		return 1;
	}

	request.prompt = "find motion beats";
	request.temporalWindow.startSeconds = 1.0;
	request.temporalWindow.durationSeconds = 2.0;
	request.temporalWindow.sampleRateFps = 2.0;
	request.temporalWindow.maxFrames = 3;

	if (!ofxGgmlVideoUtils::hasTemporalWindow(request)) {
		std::cerr << "valid clip window reported as invalid\n";
		return 1;
	}

	const auto samples = ofxGgmlVideoUtils::planFrameSamples(request.temporalWindow);
	if (samples.size() != 3 ||
		samples[0].reference != "frame@1.000s" ||
		samples[1].reference != "frame@1.500s" ||
		samples[2].reference != "frame@2.000s") {
		std::cerr << "unexpected deterministic sample plan\n";
		return 1;
	}

	const auto clipPlan = ofxGgmlVideoUtils::planClip(request);
	if (!clipPlan ||
		clipPlan.frameSamples.size() != samples.size() ||
		clipPlan.references.size() != samples.size() ||
		clipPlan.text.find("clip plan") == std::string::npos ||
		clipPlan.text.find(request.prompt) == std::string::npos) {
		std::cerr << "clip plan did not include expected sample metadata\n";
		return 1;
	}

	ofxGgmlVideoRequest invalidWindowRequest;
	invalidWindowRequest.videoPath = request.videoPath;
	invalidWindowRequest.temporalWindow.durationSeconds = -1.0;
	if (ofxGgmlVideoUtils::planClip(invalidWindowRequest)) {
		std::cerr << "invalid clip window produced a successful plan\n";
		return 1;
	}

	ofxGgmlVideoRequest cutaway;
	cutaway.videoPath = "videos/cutaway.mp4";
	cutaway.prompt = "reaction shot";
	cutaway.tags = {"reaction", "closeup"};
	cutaway.temporalWindow.startSeconds = 4.0;
	cutaway.temporalWindow.durationSeconds = 1.0;
	cutaway.temporalWindow.sampleRateFps = 1.0;

	const auto montage = ofxGgmlVideoUtils::planMontage({request, cutaway}, "montageautomat draft");
	if (!montage ||
		montage.segments.size() != 2 ||
		montage.durationSeconds != 3.0 ||
		montage.segments[0].timelineStartSeconds != 0.0 ||
		montage.segments[1].timelineStartSeconds != 2.0 ||
		montage.references.empty() ||
		montage.references[0].find("videos/clip.mp4#frame@1.000s") == std::string::npos ||
		ofxGgmlVideoUtils::describe(montage).find("segments=2") == std::string::npos) {
		std::cerr << "montage plan did not include expected timeline metadata\n";
		return 1;
	}

	const auto edl = ofxGgmlVideoUtils::toMontageEdl(montage);
	if (edl.find("TITLE montageautomat draft") == std::string::npos ||
		edl.find("SEGMENT 000 TL 0.000s-2.000s +2.000s SRC videos/clip.mp4 @1.000s-3.000s") == std::string::npos ||
		edl.find("TAGS reaction,closeup") == std::string::npos ||
		edl.find("HANDLE IN 0.000s OUT 0.000s") == std::string::npos ||
		edl.find("REF videos/clip.mp4#frame@1.000s") == std::string::npos) {
		std::cerr << "montage EDL did not include expected edit decisions\n";
		return 1;
	}

	ofxGgmlVideoMontageOptions options;
	options.prompt = "crossfade draft";
	options.defaultTransitionKind = "crossfade";
	options.transitionSeconds = 0.5;
	options.handleSeconds = 0.25;
	options.overlapTransitions = true;
	const auto crossfadeMontage = ofxGgmlVideoUtils::planMontage({request, cutaway}, options);
	if (!crossfadeMontage ||
		crossfadeMontage.durationSeconds != 2.5 ||
		crossfadeMontage.transitionKind != "crossfade" ||
		crossfadeMontage.segments[0].timelineEndSeconds != 2.0 ||
		crossfadeMontage.segments[1].timelineStartSeconds != 1.5 ||
		crossfadeMontage.segments[0].transitionOut.durationSeconds != 0.5 ||
		crossfadeMontage.segments[1].transitionIn.kind != "crossfade" ||
		crossfadeMontage.segments[1].handleInSeconds != 0.25) {
		std::cerr << "crossfade montage options did not produce expected timeline metadata\n";
		return 1;
	}
	const auto crossfadeEdl = ofxGgmlVideoUtils::toMontageEdl(crossfadeMontage);
	if (crossfadeEdl.find("TRANSITION OUT crossfade 0.500s") == std::string::npos ||
		crossfadeEdl.find("TRANSITION IN crossfade 0.500s") == std::string::npos ||
		ofxGgmlVideoUtils::describe(crossfadeMontage).find("handles=0.250s") == std::string::npos) {
		std::cerr << "crossfade montage EDL or description did not include transition metadata\n";
		return 1;
	}

	const auto handoff = ofxGgmlVideoUtils::makeMontageHandoff(crossfadeMontage);
	const auto handoffText = ofxGgmlVideoUtils::toMontageHandoffText(handoff);
	if (!handoff ||
		handoff.contractVersion != "montage-handoff-v1" ||
		handoff.decisionOwner != "ofxGgmlAgents" ||
		handoff.scoringOwner != "ofxGgmlVision" ||
		handoff.segments.size() != crossfadeMontage.segments.size() ||
		handoff.segments[0].scoreKind != "unscored" ||
		handoff.segments[0].embeddingReference.find("pending") == std::string::npos ||
		handoffText.find("CONTRACT montage-handoff-v1") == std::string::npos ||
		handoffText.find("DECISION_OWNER ofxGgmlAgents") == std::string::npos ||
		handoffText.find("SCORING_OWNER ofxGgmlVision") == std::string::npos ||
		handoffText.find("HANDOFF_SEGMENT 000 SRC videos/clip.mp4") == std::string::npos ||
		handoffText.find("EMBEDDING pending embedding reference DIMS 0") == std::string::npos) {
		std::cerr << "montage handoff contract did not include expected agent and scoring metadata\n";
		return 1;
	}

	const auto customHandoff = ofxGgmlVideoUtils::makeMontageHandoff(crossfadeMontage, "editor-agent", "clip-index", "bridge-runner");
	if (!customHandoff ||
		customHandoff.decisionOwner != "editor-agent" ||
		customHandoff.scoringOwner != "clip-index" ||
		customHandoff.bridgeOwner != "bridge-runner") {
		std::cerr << "montage handoff did not preserve custom owners\n";
		return 1;
	}

	options.overlapTransitions = false;
	const auto nonOverlapMontage = ofxGgmlVideoUtils::planMontage({request, cutaway}, options);
	if (!nonOverlapMontage ||
		nonOverlapMontage.durationSeconds != 3.0 ||
		nonOverlapMontage.segments[1].timelineStartSeconds != 2.0 ||
		nonOverlapMontage.segments[0].transitionOut.durationSeconds != 0.5) {
		std::cerr << "non-overlap transition metadata changed timeline unexpectedly\n";
		return 1;
	}

	if (ofxGgmlVideoUtils::planMontage({})) {
		std::cerr << "empty montage produced a successful plan\n";
		return 1;
	}

	return 0;
}
