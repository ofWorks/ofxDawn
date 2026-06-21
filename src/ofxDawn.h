#pragma once

// ofxDawn - WebGPU (Dawn) context for openFrameworks on macOS.
//
// Wraps the boilerplate of bringing up a headless WebGPU device on top of
// Dawn's Metal backend: instance -> adapter -> device -> queue. The async
// WebGPU requests are driven synchronously here by spinning ProcessEvents(),
// which is fine for one-off setup.

#include "ofMain.h"
#include <webgpu/webgpu_cpp.h>

class ofxDawn {
public:
	// Brings up the WebGPU device. Returns false if no adapter/device could be
	// acquired. Safe to call once; subsequent calls are no-ops.
	bool setup();

	bool isSetup() const { return bSetup; }

	// Pumps Dawn's event loop. Needed to resolve futures (map callbacks,
	// async requests). Call after submitting work you need to wait on.
	void processEvents();

	// Blocks until all submitted GPU work has completed. Used to make WebGPU
	// (Metal) results safe to read from OpenGL, which can't wait on Metal
	// fences directly.
	void waitForGPU();

	// Compiles a WGSL source string into a shader module.
	wgpu::ShaderModule createShaderModule(const std::string & wgsl, const std::string & label = "ofxDawn shader");

	const wgpu::Instance & getInstance() const { return instance; }
	const wgpu::Adapter & getAdapter() const { return adapter; }
	const wgpu::Device & getDevice() const { return device; }
	const wgpu::Queue & getQueue() const { return queue; }

private:
	bool bSetup = false;
	wgpu::Instance instance;
	wgpu::Adapter adapter;
	wgpu::Device device;
	wgpu::Queue queue;
};
