#pragma once

#include <string>
#include <vector>

struct ofxGgmlVideoRequest {
	std::string videoPath;
	std::string prompt;
	std::vector<std::string> tags;
};

struct ofxGgmlVideoResult {
	bool success = false;
	std::string text;
	std::string error;
	std::vector<std::string> references;

	explicit operator bool() const {
		return success;
	}
};