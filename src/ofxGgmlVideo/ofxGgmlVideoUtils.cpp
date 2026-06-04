#include "ofxGgmlVideoUtils.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ofxGgmlVideoUtils {
	namespace {
		std::string formatSeconds(const double seconds) {
			std::ostringstream stream;
			stream << std::fixed << std::setprecision(3) << seconds << "s";
			return stream.str();
		}

		std::string joinTags(const std::vector<std::string> & tags) {
			if (tags.empty()) {
				return "-";
			}

			std::ostringstream stream;
			for (std::size_t i = 0; i < tags.size(); ++i) {
				if (i > 0) {
					stream << ",";
				}
				stream << tags[i];
			}
			return stream.str();
		}
	}

	bool hasInput(const ofxGgmlVideoRequest & request) {
		return !request.videoPath.empty();
	}

	bool hasTemporalWindow(const ofxGgmlVideoRequest & request) {
		return isValid(request.temporalWindow);
	}

	bool isValid(const ofxGgmlVideoTemporalWindow & window) {
		return window.startSeconds >= 0.0 &&
			window.durationSeconds > 0.0 &&
			window.sampleRateFps > 0.0 &&
			window.maxFrames >= 0;
	}

	std::string describe(const ofxGgmlVideoRequest & request) {
		if (!hasInput(request)) {
			return "video: empty request";
		}
		std::string description = "video: " + request.videoPath;
		if (hasTemporalWindow(request)) {
			const auto & window = request.temporalWindow;
			description += " window=" + formatSeconds(window.startSeconds) +
				"+" + formatSeconds(window.durationSeconds) +
				" sampleRate=" + std::to_string(window.sampleRateFps) + "fps";
		}
		return description;
	}

	std::string describe(const ofxGgmlVideoMontagePlan & plan) {
		if (!plan) {
			return plan.error.empty() ? "video: empty montage plan" : plan.error;
		}

		std::ostringstream description;
		description << "video: montage plan segments=" << plan.segments.size()
			<< " duration=" << formatSeconds(plan.durationSeconds);
		if (!plan.prompt.empty()) {
			description << " prompt=\"" << plan.prompt << "\"";
		}
		return description.str();
	}

	std::vector<ofxGgmlVideoFrameSample> planFrameSamples(const ofxGgmlVideoTemporalWindow & window) {
		std::vector<ofxGgmlVideoFrameSample> samples;
		if (!isValid(window)) {
			return samples;
		}

		const auto plannedCount = std::max(1, static_cast<int>(std::ceil(window.durationSeconds * window.sampleRateFps)));
		const auto sampleCount = window.maxFrames > 0 ? std::min(plannedCount, window.maxFrames) : plannedCount;
		samples.reserve(static_cast<std::size_t>(sampleCount));

		for (int i = 0; i < sampleCount; ++i) {
			ofxGgmlVideoFrameSample sample;
			sample.index = i;
			sample.timeSeconds = window.startSeconds + (static_cast<double>(i) / window.sampleRateFps);
			sample.reference = "frame@" + formatSeconds(sample.timeSeconds);
			samples.push_back(sample);
		}

		return samples;
	}

	ofxGgmlVideoResult planClip(const ofxGgmlVideoRequest & request) {
		ofxGgmlVideoResult result;
		if (!hasInput(request)) {
			result.error = "video: missing input";
			return result;
		}
		if (!hasTemporalWindow(request)) {
			result.error = "video: invalid clip window";
			return result;
		}

		result.frameSamples = planFrameSamples(request.temporalWindow);
		for (const auto & sample : result.frameSamples) {
			result.references.push_back(sample.reference);
		}

		std::ostringstream text;
		text << "video: clip plan for " << request.videoPath
			<< " samples=" << result.frameSamples.size()
			<< " start=" << formatSeconds(request.temporalWindow.startSeconds)
			<< " duration=" << formatSeconds(request.temporalWindow.durationSeconds);
		if (!request.prompt.empty()) {
			text << " prompt=\"" << request.prompt << "\"";
		}

		result.success = true;
		result.text = text.str();
		return result;
	}

	ofxGgmlVideoMontageSegment makeMontageSegment(const ofxGgmlVideoRequest & request, const int index, const double timelineStartSeconds) {
		ofxGgmlVideoMontageSegment segment;
		segment.index = index;
		segment.sourcePath = request.videoPath;
		segment.label = request.prompt.empty() ? request.videoPath : request.prompt;
		segment.sourceStartSeconds = request.temporalWindow.startSeconds;
		segment.durationSeconds = request.temporalWindow.durationSeconds;
		segment.timelineStartSeconds = timelineStartSeconds;
		segment.tags = request.tags;
		segment.frameSamples = planFrameSamples(request.temporalWindow);
		for (const auto & sample : segment.frameSamples) {
			segment.references.push_back(segment.sourcePath + "#" + sample.reference);
		}
		return segment;
	}

	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const std::string & prompt) {
		ofxGgmlVideoMontagePlan plan;
		plan.prompt = prompt;
		if (requests.empty()) {
			plan.error = "video: empty montage";
			return plan;
		}

		double timelineStartSeconds = 0.0;
		for (std::size_t i = 0; i < requests.size(); ++i) {
			const auto clipPlan = planClip(requests[i]);
			if (!clipPlan) {
				plan.error = "video: montage segment " + std::to_string(i) + " failed: " + clipPlan.error;
				return plan;
			}

			auto segment = makeMontageSegment(requests[i], static_cast<int>(i), timelineStartSeconds);
			for (const auto & reference : segment.references) {
				plan.references.push_back(reference);
			}
			timelineStartSeconds += segment.durationSeconds;
			plan.segments.push_back(segment);
		}

		plan.success = true;
		plan.durationSeconds = timelineStartSeconds;
		plan.text = describe(plan);
		return plan;
	}

	std::string toMontageEdl(const ofxGgmlVideoMontagePlan & plan) {
		if (!plan) {
			return "";
		}

		std::ostringstream stream;
		stream << "TITLE " << (plan.prompt.empty() ? "ofxGgmlVideo montage" : plan.prompt) << "\n";
		stream << "DURATION " << formatSeconds(plan.durationSeconds) << "\n";
		for (const auto & segment : plan.segments) {
			stream << "SEGMENT " << std::setw(3) << std::setfill('0') << segment.index << std::setfill(' ')
				<< " TL " << formatSeconds(segment.timelineStartSeconds)
				<< " +" << formatSeconds(segment.durationSeconds)
				<< " SRC " << segment.sourcePath
				<< " @" << formatSeconds(segment.sourceStartSeconds)
				<< " LABEL " << segment.label
				<< " TAGS " << joinTags(segment.tags)
				<< "\n";
			for (const auto & reference : segment.references) {
				stream << "REF " << reference << "\n";
			}
		}
		return stream.str();
	}
}
