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

		double clampNonNegative(const double value) {
			return std::max(0.0, value);
		}

		std::string normalizedTransitionKind(const std::string & kind) {
			return kind.empty() ? "cut" : kind;
		}

		double transitionBetween(const ofxGgmlVideoMontageOptions & options,
		                         const ofxGgmlVideoRequest & previous,
		                         const ofxGgmlVideoRequest & current) {
			const auto requested = clampNonNegative(options.transitionSeconds);
			if (requested <= 0.0) {
				return 0.0;
			}
			return std::min({requested,
				previous.temporalWindow.durationSeconds * 0.5,
				current.temporalWindow.durationSeconds * 0.5});
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
		if (plan.transitionSeconds > 0.0) {
			description << " transition=" << plan.transitionKind
				<< "+" << formatSeconds(plan.transitionSeconds);
			if (plan.overlapTransitions) {
				description << " overlap";
			}
		}
		if (plan.handleSeconds > 0.0) {
			description << " handles=" << formatSeconds(plan.handleSeconds);
		}
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
		ofxGgmlVideoMontageOptions options;
		return makeMontageSegment(request, index, timelineStartSeconds, options);
	}

	ofxGgmlVideoMontageSegment makeMontageSegment(const ofxGgmlVideoRequest & request,
	                                              const int index,
	                                              const double timelineStartSeconds,
	                                              const ofxGgmlVideoMontageOptions & options) {
		ofxGgmlVideoMontageSegment segment;
		segment.index = index;
		segment.sourcePath = request.videoPath;
		segment.label = request.prompt.empty() ? request.videoPath : request.prompt;
		segment.sourceStartSeconds = request.temporalWindow.startSeconds;
		segment.sourceEndSeconds = request.temporalWindow.startSeconds + request.temporalWindow.durationSeconds;
		segment.durationSeconds = request.temporalWindow.durationSeconds;
		segment.timelineStartSeconds = timelineStartSeconds;
		segment.timelineEndSeconds = timelineStartSeconds + segment.durationSeconds;
		segment.handleInSeconds = std::min(clampNonNegative(options.handleSeconds), segment.sourceStartSeconds);
		segment.handleOutSeconds = clampNonNegative(options.handleSeconds);
		segment.transitionIn.kind = "cut";
		segment.transitionOut.kind = normalizedTransitionKind(options.defaultTransitionKind);
		segment.transitionIn.durationSeconds = 0.0;
		segment.transitionOut.durationSeconds = clampNonNegative(options.transitionSeconds);
		segment.tags = request.tags;
		segment.frameSamples = planFrameSamples(request.temporalWindow);
		for (const auto & sample : segment.frameSamples) {
			segment.references.push_back(segment.sourcePath + "#" + sample.reference);
		}
		return segment;
	}

	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const std::string & prompt) {
		ofxGgmlVideoMontageOptions options;
		options.prompt = prompt;
		return planMontage(requests, options);
	}

	ofxGgmlVideoMontagePlan planMontage(const std::vector<ofxGgmlVideoRequest> & requests, const ofxGgmlVideoMontageOptions & options) {
		ofxGgmlVideoMontagePlan plan;
		plan.prompt = options.prompt;
		plan.handleSeconds = clampNonNegative(options.handleSeconds);
		plan.transitionSeconds = clampNonNegative(options.transitionSeconds);
		plan.transitionKind = normalizedTransitionKind(options.defaultTransitionKind);
		plan.overlapTransitions = options.overlapTransitions;
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

			const auto transitionSeconds = i > 0 ? transitionBetween(options, requests[i - 1], requests[i]) : 0.0;
			if (i > 0 && options.overlapTransitions) {
				timelineStartSeconds = std::max(0.0, timelineStartSeconds - transitionSeconds);
			}

			auto segment = makeMontageSegment(requests[i], static_cast<int>(i), timelineStartSeconds, options);
			if (i > 0) {
				segment.transitionIn.kind = plan.transitionKind;
				segment.transitionIn.durationSeconds = transitionSeconds;
				if (!plan.segments.empty()) {
					plan.segments.back().transitionOut.kind = plan.transitionKind;
					plan.segments.back().transitionOut.durationSeconds = transitionSeconds;
				}
			}
			if (i + 1 == requests.size()) {
				segment.transitionOut.kind = "cut";
				segment.transitionOut.durationSeconds = 0.0;
			}
			for (const auto & reference : segment.references) {
				plan.references.push_back(reference);
			}
			timelineStartSeconds = segment.timelineEndSeconds;
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
				<< "-" << formatSeconds(segment.timelineEndSeconds)
				<< " +" << formatSeconds(segment.durationSeconds)
				<< " SRC " << segment.sourcePath
				<< " @" << formatSeconds(segment.sourceStartSeconds)
				<< "-" << formatSeconds(segment.sourceEndSeconds)
				<< " LABEL " << segment.label
				<< " TAGS " << joinTags(segment.tags)
				<< "\n";
			stream << "HANDLE IN " << formatSeconds(segment.handleInSeconds)
				<< " OUT " << formatSeconds(segment.handleOutSeconds)
				<< "\n";
			if (segment.transitionIn.durationSeconds > 0.0) {
				stream << "TRANSITION IN " << segment.transitionIn.kind
					<< " " << formatSeconds(segment.transitionIn.durationSeconds)
					<< "\n";
			}
			if (segment.transitionOut.durationSeconds > 0.0) {
				stream << "TRANSITION OUT " << segment.transitionOut.kind
					<< " " << formatSeconds(segment.transitionOut.durationSeconds)
					<< "\n";
			}
			for (const auto & reference : segment.references) {
				stream << "REF " << reference << "\n";
			}
		}
		return stream.str();
	}
}
