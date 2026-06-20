#pragma once

// ofxDawnParticles - a GPU particle system simulated with a WebGPU compute
// shader (via Dawn) and rendered with openFrameworks.
//
// The simulation runs entirely on the GPU in a storage buffer. Each frame the
// positions are copied back to the CPU and pushed into an ofVboMesh of points,
// so rendering stays in plain OF/OpenGL - no GL<->Metal interop required.
//
// Reading back every frame costs a GPU sync; it is the simplest correct path and is
// fine for tens of thousands of particles. A zero-copy IOSurface path is the
// natural next optimization (see README).

#include "ofMain.h"
#include "ofxDawn.h"

class ofxDawnParticles {
public:
	// count: number of particles. bounds: simulation area in pixels.
	bool setup(ofxDawn & dawn, int count, glm::vec2 bounds);

	// Advances the simulation by dt seconds and pulls the result back into the
	// mesh. attractor is a point particles are pulled toward (e.g. the mouse).
	void update(float dt, glm::vec2 attractor);

	// Draws the particles as points using the current OF color / blend state.
	void draw();

	int getCount() const { return count; }
	ofVboMesh & getMesh() { return mesh; }

private:
	// Must match the SimParams layout in the WGSL shader (std140-style, 32 bytes).
	struct SimParams {
		float dt;
		float time;
		uint32_t count;
		uint32_t _pad;
		float attractorX, attractorY;
		float boundsX, boundsY;
	};

	// Must match the Particle layout in the WGSL shader (16 bytes).
	struct Particle {
		float px, py;
		float vx, vy;
	};

	ofxDawn * dawn = nullptr;
	int count = 0;
	uint64_t particleBytes = 0;
	SimParams params {};

	wgpu::ComputePipeline pipeline;
	wgpu::BindGroup bindGroup;
	wgpu::Buffer particleBuffer; // storage, simulated in place
	wgpu::Buffer paramBuffer;    // uniform
	wgpu::Buffer readbackBuffer; // MapRead copy target

	ofVboMesh mesh;
};
