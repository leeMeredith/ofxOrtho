# ofxOrtho — openFrameworks addon config.
#
# The kernel (libs/ortho-kernel) is plain C99; the wrapper (src) is C++.
# Both source trees are compiled, and both include dirs are exposed.
#
# The kernel is vendored (not a submodule) — see libs/ortho-kernel/VENDORED.md

meta:
	ADDON_NAME = ofxOrtho
	ADDON_DESCRIPTION = Invented-language generator. Deterministic pseudo-words from a seed.
	ADDON_AUTHOR = Lee Meredith
	ADDON_TAGS = "text" "generative" "language" "procedural"
	ADDON_URL = https://github.com/leeMeredith/ofxOrtho

common:
	ADDON_INCLUDES  = src
	ADDON_INCLUDES += libs/ortho-kernel/include

	ADDON_SOURCES  = src/ofxOrtho.cpp
	ADDON_SOURCES += libs/ortho-kernel/src/ortho.c
	ADDON_SOURCES += libs/ortho-kernel/src/prng.c

	# Keep the tests/ and example/ trees out of the addon build.
	ADDON_SOURCES_EXCLUDE  = tests/%
	ADDON_SOURCES_EXCLUDE += example-basic/%
	ADDON_SOURCES_EXCLUDE += libs/ortho-kernel/tests/%
	ADDON_INCLUDES_EXCLUDE = tests/%
	ADDON_INCLUDES_EXCLUDE += example-basic/%
	ADDON_INCLUDES_EXCLUDE += libs/ortho-kernel/tests/%
