meta:
	ADDON_NAME = ofxDawn
	ADDON_DESCRIPTION = WebGPU (Dawn) for openFrameworks on macOS - GPU compute & particle systems
	ADDON_AUTHOR = Dimitre
	ADDON_TAGS = "webgpu" "dawn" "gpgpu" "compute" "particles"
	ADDON_URL = https://github.com/dimitre/ofxDawn

common:
	# Dawn ships a single monolithic static lib (libwebgpu_dawn.a) that exports
	# the standard wgpu* C symbols, so no proc-table wiring is needed.
	# The static lib is auto-detected at libs/dawn/lib/<platform>/*.a
	ADDON_INCLUDES += libs/dawn/include
	ADDON_INCLUDES += src

# ofGen / ofWorks (chalet) platform string is "macos".
macos:
	# Frameworks pulled in by the Dawn Metal backend. The ofWorks chalet base
	# already links IOKit / Cocoa / QuartzCore; these are the extra ones.
	ADDON_FRAMEWORKS = Metal
	ADDON_FRAMEWORKS += IOSurface
	ADDON_FRAMEWORKS += Foundation
	ADDON_LIBS_EXCLUDE = libs/dawn/lib/cmake/%
	ADDON_INCLUDES_EXCLUDE = libs/dawn/lib/cmake/%

# Standard openFrameworks Project Generator / makefiles use "osx".
osx:
	ADDON_FRAMEWORKS = Metal
	ADDON_FRAMEWORKS += IOSurface
	ADDON_FRAMEWORKS += Foundation
	ADDON_FRAMEWORKS += QuartzCore
	ADDON_FRAMEWORKS += IOKit
	ADDON_FRAMEWORKS += Cocoa
	ADDON_LIBS_EXCLUDE = libs/dawn/lib/cmake/%
	ADDON_INCLUDES_EXCLUDE = libs/dawn/lib/cmake/%
