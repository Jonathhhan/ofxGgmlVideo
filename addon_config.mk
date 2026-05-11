meta:
	ADDON_NAME = ofxGgmlVideo
	ADDON_DESCRIPTION = Companion addon for local video workflow workflows on top of ofxGgmlCore
	ADDON_AUTHOR = Jonathan Frank
	ADDON_TAGS = "ggml,ai,video,frames,media"
	ADDON_URL = https://github.com/Jonathhhan/ofxGgmlVideo

common:
	ADDON_DEPENDENCIES += ofxGgmlCore
	ADDON_INCLUDES += src
	ADDON_SOURCES_EXCLUDE += build/%
	ADDON_SOURCES_EXCLUDE += libs/*/build/%
	ADDON_SOURCES_EXCLUDE += libs/*/build*/%
	ADDON_INCLUDES_EXCLUDE += build/%
	ADDON_INCLUDES_EXCLUDE += libs/*/build/%
	ADDON_INCLUDES_EXCLUDE += libs/*/build*/%