#include "ofxDawnParticles.h"

namespace {

// Shared struct definitions used by both shaders.
const char * kCommonWGSL = R"WGSL(
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
)WGSL";

// Compute: one thread per particle - pull toward the attractor, integrate,
// damp, and wrap around the simulation bounds.
const char * kComputeWGSL = R"WGSL(
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
	p.vel = p.vel + dir * (6000.0 / dist) * params.dt;
	p.vel = p.vel * 0.985;

	p.pos = p.pos + p.vel * params.dt;

	if (p.pos.x < 0.0)             { p.pos.x = p.pos.x + params.bounds.x; }
	if (p.pos.x > params.bounds.x) { p.pos.x = p.pos.x - params.bounds.x; }
	if (p.pos.y < 0.0)             { p.pos.y = p.pos.y + params.bounds.y; }
	if (p.pos.y > params.bounds.y) { p.pos.y = p.pos.y - params.bounds.y; }

	particles[i] = p;
}
)WGSL";

// Render: read each particle from the (read-only) storage buffer in the vertex
// shader and draw it as a point. Colour ramps with speed.
const char * kRenderWGSL = R"WGSL(
@group(0) @binding(0) var<storage, read> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : SimParams;

struct VSOut {
	@builtin(position) clip : vec4<f32>,
	@location(0) speed : f32,
};

@vertex
fn vs(@builtin(vertex_index) vid : u32) -> VSOut {
	let p = particles[vid];
	// Pixel space -> NDC. Flip Y so it matches OF's top-left origin.
	let x = p.pos.x / params.bounds.x * 2.0 - 1.0;
	let y = -(p.pos.y / params.bounds.y * 2.0 - 1.0);
	var o : VSOut;
	o.clip = vec4<f32>(x, y, 0.0, 1.0);
	o.speed = length(p.vel);
	return o;
}

@fragment
fn fs(in : VSOut) -> @location(0) vec4<f32> {
	let t = clamp(in.speed / 400.0, 0.0, 1.0);
	let col = mix(vec3<f32>(0.10, 0.30, 0.80), vec3<f32>(1.0, 0.7, 0.2), t);
	return vec4<f32>(col, 1.0);
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
	particleDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
	particleBuffer = device.CreateBuffer(&particleDesc);

	wgpu::BufferDescriptor paramDesc {};
	paramDesc.label = "sim params";
	paramDesc.size = sizeof(SimParams);
	paramDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
	paramBuffer = device.CreateBuffer(&paramDesc);

	// --- Seed initial state ---
	std::vector<Particle> initial(count);
	for (auto & p : initial) {
		p.px = ofRandom(bounds.x);
		p.py = ofRandom(bounds.y);
		p.vx = ofRandom(-40.0f, 40.0f);
		p.vy = ofRandom(-40.0f, 40.0f);
	}
	dawn->getQueue().WriteBuffer(particleBuffer, 0, initial.data(), particleBytes);

	// --- Compute pipeline (auto layout) ---
	wgpu::ShaderModule computeModule = dawn->createShaderModule(
		std::string(kCommonWGSL) + kComputeWGSL, "particles compute");

	wgpu::ComputePipelineDescriptor computeDesc {};
	computeDesc.label = "particles compute pipeline";
	computeDesc.compute.module = computeModule;
	computeDesc.compute.entryPoint = "main";
	computePipeline = device.CreateComputePipeline(&computeDesc);

	wgpu::BindGroupEntry computeEntries[2] {};
	computeEntries[0].binding = 0;
	computeEntries[0].buffer = particleBuffer;
	computeEntries[0].size = particleBytes;
	computeEntries[1].binding = 1;
	computeEntries[1].buffer = paramBuffer;
	computeEntries[1].size = sizeof(SimParams);

	wgpu::BindGroupDescriptor computeBgDesc {};
	computeBgDesc.layout = computePipeline.GetBindGroupLayout(0);
	computeBgDesc.entryCount = 2;
	computeBgDesc.entries = computeEntries;
	computeBindGroup = device.CreateBindGroup(&computeBgDesc);

	// --- Render pipeline ---
	wgpu::ShaderModule renderModule = dawn->createShaderModule(
		std::string(kCommonWGSL) + kRenderWGSL, "particles render");

	// Additive blending so overlapping particles accumulate brightness.
	wgpu::BlendState blend {};
	blend.color.srcFactor = wgpu::BlendFactor::One;
	blend.color.dstFactor = wgpu::BlendFactor::One;
	blend.color.operation = wgpu::BlendOperation::Add;
	blend.alpha.srcFactor = wgpu::BlendFactor::One;
	blend.alpha.dstFactor = wgpu::BlendFactor::One;
	blend.alpha.operation = wgpu::BlendOperation::Add;

	wgpu::ColorTargetState colorTarget {};
	colorTarget.format = wgpu::TextureFormat::BGRA8Unorm; // matches the IOSurface
	colorTarget.blend = &blend;

	wgpu::FragmentState fragment {};
	fragment.module = renderModule;
	fragment.entryPoint = "fs";
	fragment.targetCount = 1;
	fragment.targets = &colorTarget;

	wgpu::RenderPipelineDescriptor renderDesc {};
	renderDesc.label = "particles render pipeline";
	renderDesc.vertex.module = renderModule;
	renderDesc.vertex.entryPoint = "vs";
	renderDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
	renderDesc.fragment = &fragment;
	renderPipeline = device.CreateRenderPipeline(&renderDesc);

	wgpu::BindGroupEntry renderEntries[2] {};
	renderEntries[0].binding = 0;
	renderEntries[0].buffer = particleBuffer;
	renderEntries[0].size = particleBytes;
	renderEntries[1].binding = 1;
	renderEntries[1].buffer = paramBuffer;
	renderEntries[1].size = sizeof(SimParams);

	wgpu::BindGroupDescriptor renderBgDesc {};
	renderBgDesc.layout = renderPipeline.GetBindGroupLayout(0);
	renderBgDesc.entryCount = 2;
	renderBgDesc.entries = renderEntries;
	renderBindGroup = device.CreateBindGroup(&renderBgDesc);

	ofLogNotice("ofxDawnParticles") << "simulating + rendering " << count << " particles on the GPU";
	return true;
}

void ofxDawnParticles::update(float dt, glm::vec2 attractor, ofxDawnSharedTexture & target) {
	if (!computePipeline || !target.isSetup()) return;

	const wgpu::Device & device = dawn->getDevice();
	const wgpu::Queue & queue = dawn->getQueue();

	params.dt = dt;
	params.time += dt;
	params.attractorX = attractor.x;
	params.attractorY = attractor.y;
	queue.WriteBuffer(paramBuffer, 0, &params, sizeof(SimParams));

	target.beginAccess();

	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	// Simulate.
	{
		wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
		pass.SetPipeline(computePipeline);
		pass.SetBindGroup(0, computeBindGroup);
		pass.DispatchWorkgroups((static_cast<uint32_t>(count) + 63u) / 64u);
		pass.End();
	}

	// Render into the shared texture.
	{
		wgpu::RenderPassColorAttachment color {};
		color.view = target.getTextureView();
		color.loadOp = wgpu::LoadOp::Clear;
		color.storeOp = wgpu::StoreOp::Store;
		color.clearValue = { 0.0, 0.0, 0.0, 1.0 };

		wgpu::RenderPassDescriptor renderPass {};
		renderPass.colorAttachmentCount = 1;
		renderPass.colorAttachments = &color;

		wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
		pass.SetPipeline(renderPipeline);
		pass.SetBindGroup(0, renderBindGroup);
		pass.Draw(static_cast<uint32_t>(count));
		pass.End();
	}

	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	target.endAccess();

	// Make the Metal work visible to OpenGL before OF samples the texture.
	dawn->waitForGPU();
}
