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

		std::string jsonEscape(const std::string & value) {
			std::ostringstream stream;
			for (const auto ch : value) {
				switch (ch) {
				case '\\':
					stream << "\\\\";
					break;
				case '"':
					stream << "\\\"";
					break;
				case '\n':
					stream << "\\n";
					break;
				case '\r':
					stream << "\\r";
					break;
				case '\t':
					stream << "\\t";
					break;
				default:
					stream << ch;
					break;
				}
			}
			return stream.str();
		}

		std::string jsonString(const std::string & value) {
			return "\"" + jsonEscape(value) + "\"";
		}

		void writeStringArray(std::ostringstream & stream,
		                      const std::vector<std::string> & values,
		                      const int indent) {
			stream << "[";
			for (std::size_t i = 0; i < values.size(); ++i) {
				if (i > 0) {
					stream << ", ";
				}
				stream << jsonString(values[i]);
			}
			stream << "]";
			(void)indent;
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

	std::string toMontageManifestJson(const ofxGgmlVideoMontagePlan & plan) {
		if (!plan) {
			return "";
		}

		std::ostringstream stream;
		stream << std::fixed << std::setprecision(3);
		stream << "{\n";
		stream << "  \"kind\": \"ofxGgmlVideoMontageManifest\",\n";
		stream << "  \"version\": 1,\n";
		stream << "  \"prompt\": " << jsonString(plan.prompt) << ",\n";
		stream << "  \"durationSeconds\": " << plan.durationSeconds << ",\n";
		stream << "  \"options\": {\n";
		stream << "    \"transitionKind\": " << jsonString(plan.transitionKind) << ",\n";
		stream << "    \"transitionSeconds\": " << plan.transitionSeconds << ",\n";
		stream << "    \"handleSeconds\": " << plan.handleSeconds << ",\n";
		stream << "    \"overlapTransitions\": " << (plan.overlapTransitions ? "true" : "false") << "\n";
		stream << "  },\n";
		stream << "  \"references\": ";
		writeStringArray(stream, plan.references, 2);
		stream << ",\n";
		stream << "  \"segments\": [\n";
		for (std::size_t i = 0; i < plan.segments.size(); ++i) {
			const auto & segment = plan.segments[i];
			stream << "    {\n";
			stream << "      \"index\": " << segment.index << ",\n";
			stream << "      \"sourcePath\": " << jsonString(segment.sourcePath) << ",\n";
			stream << "      \"label\": " << jsonString(segment.label) << ",\n";
			stream << "      \"sourceStartSeconds\": " << segment.sourceStartSeconds << ",\n";
			stream << "      \"sourceEndSeconds\": " << segment.sourceEndSeconds << ",\n";
			stream << "      \"timelineStartSeconds\": " << segment.timelineStartSeconds << ",\n";
			stream << "      \"timelineEndSeconds\": " << segment.timelineEndSeconds << ",\n";
			stream << "      \"durationSeconds\": " << segment.durationSeconds << ",\n";
			stream << "      \"handleInSeconds\": " << segment.handleInSeconds << ",\n";
			stream << "      \"handleOutSeconds\": " << segment.handleOutSeconds << ",\n";
			stream << "      \"transitionIn\": {\"kind\": " << jsonString(segment.transitionIn.kind)
				<< ", \"durationSeconds\": " << segment.transitionIn.durationSeconds << "},\n";
			stream << "      \"transitionOut\": {\"kind\": " << jsonString(segment.transitionOut.kind)
				<< ", \"durationSeconds\": " << segment.transitionOut.durationSeconds << "},\n";
			stream << "      \"tags\": ";
			writeStringArray(stream, segment.tags, 6);
			stream << ",\n";
			stream << "      \"references\": ";
			writeStringArray(stream, segment.references, 6);
			stream << ",\n";
			stream << "      \"frameSamples\": [";
			for (std::size_t j = 0; j < segment.frameSamples.size(); ++j) {
				const auto & sample = segment.frameSamples[j];
				if (j > 0) {
					stream << ", ";
				}
				stream << "{\"index\": " << sample.index
					<< ", \"timeSeconds\": " << sample.timeSeconds
					<< ", \"reference\": " << jsonString(sample.reference) << "}";
			}
			stream << "]\n";
			stream << "    }";
			if (i + 1 < plan.segments.size()) {
				stream << ",";
			}
			stream << "\n";
		}
		stream << "  ]\n";
		stream << "}\n";
		return stream.str();
	}

	ofxGgmlVideoMontageHandoff makeMontageHandoff(const ofxGgmlVideoMontagePlan & plan,
	                                               const std::string & decisionOwner,
	                                               const std::string & scoringOwner,
	                                               const std::string & bridgeOwner) {
		ofxGgmlVideoMontageHandoff handoff;
		handoff.decisionOwner = decisionOwner.empty() ? "ofxGgmlAgents" : decisionOwner;
		handoff.scoringOwner = scoringOwner.empty() ? "ofxGgmlVision" : scoringOwner;
		handoff.bridgeOwner = bridgeOwner.empty() ? "app-layer" : bridgeOwner;
		if (!plan) {
			handoff.error = plan.error.empty() ? "video: invalid montage handoff source" : plan.error;
			return handoff;
		}

		handoff.prompt = plan.prompt;
		handoff.references = plan.references;
		handoff.warnings.push_back("agentic clip choice belongs to " + handoff.decisionOwner);
		handoff.warnings.push_back("CLIP-style scoring and embeddings belong to " + handoff.scoringOwner);
		handoff.warnings.push_back("external bridge outputs belong to " + handoff.bridgeOwner);
		for (const auto & segment : plan.segments) {
			ofxGgmlVideoMontageHandoffSegment handoffSegment;
			handoffSegment.segmentIndex = segment.index;
			handoffSegment.sourcePath = segment.sourcePath;
			handoffSegment.label = segment.label;
			handoffSegment.agentReason = "pending agent decision";
			handoffSegment.temporalSummary = "pending temporal summary";
			handoffSegment.scoreKind = "unscored";
			handoffSegment.embeddingReference = "pending embedding reference";
			handoffSegment.bridgeOutputKind = "none";
			handoffSegment.bridgeOutputReference = "none";
			handoffSegment.references = segment.references;
			handoff.segments.push_back(handoffSegment);
		}

		handoff.success = true;
		handoff.text = toMontageHandoffText(handoff);
		return handoff;
	}

	std::string toMontageHandoffText(const ofxGgmlVideoMontageHandoff & handoff) {
		if (!handoff) {
			return "";
		}

		std::ostringstream stream;
		stream << "CONTRACT " << handoff.contractVersion << "\n";
		stream << "DECISION_OWNER " << handoff.decisionOwner << "\n";
		stream << "SCORING_OWNER " << handoff.scoringOwner << "\n";
		stream << "BRIDGE_OWNER " << handoff.bridgeOwner << "\n";
		stream << "PROMPT " << (handoff.prompt.empty() ? "-" : handoff.prompt) << "\n";
		for (const auto & warning : handoff.warnings) {
			stream << "WARNING " << warning << "\n";
		}
		for (const auto & segment : handoff.segments) {
			stream << "HANDOFF_SEGMENT " << std::setw(3) << std::setfill('0') << segment.segmentIndex << std::setfill(' ')
				<< " SRC " << segment.sourcePath
				<< " LABEL " << segment.label
				<< " SCORE " << segment.scoreKind << ":" << std::fixed << std::setprecision(3) << segment.score
				<< "\n";
			stream << "AGENT_REASON " << segment.agentReason << "\n";
			stream << "TEMPORAL_SUMMARY " << segment.temporalSummary << "\n";
			stream << "EMBEDDING " << segment.embeddingReference
				<< " DIMS " << segment.embeddingDimensions
				<< "\n";
			stream << "BRIDGE_OUTPUT " << segment.bridgeOutputKind
				<< " " << segment.bridgeOutputReference
				<< "\n";
			for (const auto & reference : segment.references) {
				stream << "REF " << reference << "\n";
			}
		}
		return stream.str();
	}
}
