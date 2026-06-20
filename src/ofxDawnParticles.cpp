#include "ofxDawnParticles.h"

namespace {

// WGSL compute shader. One thread per particle: pull toward the attractor,
// integrate, damp, and wrap around the simulation bounds.
const char * kComputeWGSL = R"WGSL(
struct Particle {
	pos : vec2<f32>,
	vel : vec2<f32>,
};

struct SimParams {
	dt        : f32,
	time      : f32,
	count     : u32,
	_pad      : u32,
	attractor : vec2<f32>,
	bounds    : vec2<f32>,
};

@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : SimParams;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
	let i = gid.x;
	if (i >= params.count) {
		return;
	}

	var p = particles[i];

	let toAttractor = params.attractor - p.pos;
	let dist = max(length(toAttractor), 16.0);
	let dir = toAttractor / dist;
	// Inverse-falloff pull, strongest near the attractor.
	p.vel = p.vel + dir * (6000.0 / dist) * params.dt;
	p.vel = p.vel * 0.985; // damping

	p.pos = p.pos + p.vel * params.dt;

	// Toroidal wrap so particles never escape the view.
	if (p.pos.x < 0.0)            { p.pos.x = p.pos.x + params.bounds.x; }
	if (p.pos.x > params.bounds.x) { p.pos.x = p.pos.x - params.bounds.x; }
	if (p.pos.y < 0.0)            { p.pos.y = p.pos.y + params.bounds.y; }
	if (p.pos.y > params.bounds.y) { p.pos.y = p.pos.y - params.bounds.y; }

	particles[i] = p;
}
)WGSL";

} // namespace

bool ofxDawnParticles::setup(ofxDawn & dawnRef, int particleCount, glm::vec2 bounds) {
	dawn = &dawnRef;
	if (!dawn->isSetup()) {
		ofLogError("ofxDawnParticles") << "ofxDawn is not set up";
		return false;
	}

	count = particleCount;
	particleBytes = static_cast<uint64_t>(count) * sizeof(Particle);

	params.count = static_cast<uint32_t>(count);
	params.boundsX = bounds.x;
	params.boundsY = bounds.y;
	params.time = 0.0f;

	const wgpu::Device & device = dawn->getDevice();

	// --- Buffers ---
	wgpu::BufferDescriptor particleDesc {};
	particleDesc.label = "particles";
	particleDesc.size = particleBytes;
	particleDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
	particleBuffer = device.CreateBuffer(&particleDesc);

	wgpu::BufferDescriptor paramDesc {};
	paramDesc.label = "sim params";
	paramDesc.size = sizeof(SimParams);
	paramDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
	paramBuffer = device.CreateBuffer(&paramDesc);

	wgpu::BufferDescriptor readbackDesc {};
	readbackDesc.label = "readback";
	readbackDesc.size = particleBytes;
	readbackDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
	readbackBuffer = device.CreateBuffer(&readbackDesc);

	// --- Seed initial state ---
	std::vector<Particle> initial(count);
	for (auto & p : initial) {
		p.px = ofRandom(bounds.x);
		p.py = ofRandom(bounds.y);
		p.vx = ofRandom(-40.0f, 40.0f);
		p.vy = ofRandom(-40.0f, 40.0f);
	}
	dawn->getQueue().WriteBuffer(particleBuffer, 0, initial.data(), particleBytes);

	// --- Pipeline (auto layout from the shader) ---
	wgpu::ShaderModule module = dawn->createShaderModule(kComputeWGSL, "particles compute");

	wgpu::ComputePipelineDescriptor pipelineDesc {};
	pipelineDesc.label = "particles pipeline";
	pipelineDesc.compute.module = module;
	pipelineDesc.compute.entryPoint = "main";
	pipeline = device.CreateComputePipeline(&pipelineDesc);

	// --- Bind group (buffers are stable, so build it once) ---
	wgpu::BindGroupEntry entries[2] {};
	entries[0].binding = 0;
	entries[0].buffer = particleBuffer;
	entries[0].offset = 0;
	entries[0].size = particleBytes;
	entries[1].binding = 1;
	entries[1].buffer = paramBuffer;
	entries[1].offset = 0;
	entries[1].size = sizeof(SimParams);

	wgpu::BindGroupDescriptor bindGroupDesc {};
	bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
	bindGroupDesc.entryCount = 2;
	bindGroupDesc.entries = entries;
	bindGroup = device.CreateBindGroup(&bindGroupDesc);

	// --- Mesh ---
	mesh.setMode(OF_PRIMITIVE_POINTS);
	mesh.getVertices().resize(count, glm::vec3(0.0f));

	ofLogNotice("ofxDawnParticles") << "simulating " << count << " particles on the GPU";
	return true;
}

void ofxDawnParticles::update(float dt, glm::vec2 attractor) {
	if (!pipeline) return;

	const wgpu::Device & device = dawn->getDevice();
	const wgpu::Queue & queue = dawn->getQueue();

	params.dt = dt;
	params.time += dt;
	params.attractorX = attractor.x;
	params.attractorY = attractor.y;
	queue.WriteBuffer(paramBuffer, 0, &params, sizeof(SimParams));

	// Encode: compute pass, then copy results to the readback buffer.
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	{
		wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
		pass.SetPipeline(pipeline);
		pass.SetBindGroup(0, bindGroup);
		const uint32_t groups = (static_cast<uint32_t>(count) + 63u) / 64u;
		pass.DispatchWorkgroups(groups);
		pass.End();
	}
	encoder.CopyBufferToBuffer(particleBuffer, 0, readbackBuffer, 0, particleBytes);
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	// Map the readback buffer (synchronously, by pumping events).
	bool mapDone = false;
	bool mapOk = false;
	readbackBuffer.MapAsync(
		wgpu::MapMode::Read, 0, particleBytes, wgpu::CallbackMode::AllowProcessEvents,
		[&](wgpu::MapAsyncStatus status, wgpu::StringView) {
			mapOk = (status == wgpu::MapAsyncStatus::Success);
			mapDone = true;
		});
	while (!mapDone) dawn->processEvents();

	if (mapOk) {
		const Particle * data = static_cast<const Particle *>(
			readbackBuffer.GetConstMappedRange(0, particleBytes));
		auto & verts = mesh.getVertices();
		for (int i = 0; i < count; ++i) {
			verts[i].x = data[i].px;
			verts[i].y = data[i].py;
		}
		readbackBuffer.Unmap();
	}
}

void ofxDawnParticles::draw() {
	mesh.draw();
}
