#pragma once

// ofxDawnParticles - a GPU particle system simulated AND rendered with WebGPU
// (Dawn), then composited zero-copy into openFrameworks via ofxDawnSharedTexture.
//
// Each frame a compute shader advances the particles in a storage buffer, then
// a render pass draws them as points straight into a shared IOSurface texture -
// the particle data never leaves the GPU.

#include "ofMain.h"
#include "ofxDawn.h"
#include "ofxDawnSharedTexture.h"

class ofxDawnParticles {
public:
	// count: number of particles. bounds: simulation area in pixels (match the
	// target texture size).
	bool setup(ofxDawn & dawn, int count, glm::vec2 bounds);

	// Advances the simulation by dt seconds (attractor pulls particles toward
	// it) and renders the result into target. After this returns, target's
	// ofTexture is ready to draw.
	void update(float dt, glm::vec2 attractor, ofxDawnSharedTexture & target);

	int getCount() const { return count; }

private:
	// Must match the SimParams layout in the WGSL shaders (32 bytes).
	struct SimParams {
		float dt;
		float time;
		uint32_t count;
		uint32_t _pad;
		float attractorX, attractorY;
		float boundsX, boundsY;
	};

	// Must match the Particle layout in the WGSL shaders (16 bytes).
	struct Particle {
		float px, py;
		float vx, vy;
	};

	ofxDawn * dawn = nullptr;
	int count = 0;
	uint64_t particleBytes = 0;
	SimParams params {};

	wgpu::ComputePipeline computePipeline;
	wgpu::RenderPipeline renderPipeline;
	wgpu::BindGroup computeBindGroup;
	wgpu::BindGroup renderBindGroup;
	wgpu::Buffer particleBuffer; // storage, simulated in place
	wgpu::Buffer paramBuffer;    // uniform (shared by compute + render)
};
