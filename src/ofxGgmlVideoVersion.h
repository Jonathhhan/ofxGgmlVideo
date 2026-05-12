#pragma once

#define OFXGGML_VIDEO_VERSION_MAJOR 1
#define OFXGGML_VIDEO_VERSION_MINOR 0
#define OFXGGML_VIDEO_VERSION_PATCH 1
#define OFXGGML_VIDEO_VERSION_STRING "1.0.1"

inline const char * ofxGgmlVideoGetVersionString() {
	return OFXGGML_VIDEO_VERSION_STRING;
}
