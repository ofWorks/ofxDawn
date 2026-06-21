#include "ofxDawn.h"

namespace {
std::string sv(wgpu::StringView s) {
	if (s.data == nullptr) return {};
	// WGPU_STRLEN sentinel means "null terminated".
	if (s.length == WGPU_STRLEN) return std::string(s.data);
	return std::string(s.data, s.length);
}
}

bool ofxDawn::setup() {
	if (bSetup) return true;

	instance = wgpu::CreateInstance(nullptr);
	if (!instance) {
		ofLogError("ofxDawn") << "failed to create WebGPU instance";
		return false;
	}

	// --- Adapter ---
	wgpu::RequestAdapterOptions adapterOpts {};
	adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

	bool adapterDone = false;
	instance.RequestAdapter(
		&adapterOpts, wgpu::CallbackMode::AllowProcessEvents,
		[&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView message) {
			if (status == wgpu::RequestAdapterStatus::Success) {
				adapter = std::move(a);
			} else {
				ofLogError("ofxDawn") << "RequestAdapter failed: " << sv(message);
			}
			adapterDone = true;
		});
	while (!adapterDone) instance.ProcessEvents();
	if (!adapter) return false;

	// --- Device ---
	// Features needed for zero-copy IOSurface <-> OpenGL interop.
	std::vector<wgpu::FeatureName> requiredFeatures;
	for (wgpu::FeatureName f : { wgpu::FeatureName::SharedTextureMemoryIOSurface,
								 wgpu::FeatureName::SharedFenceMTLSharedEvent }) {
		if (adapter.HasFeature(f)) {
			requiredFeatures.push_back(f);
		} else {
			ofLogWarning("ofxDawn") << "adapter missing feature " << static_cast<int>(f)
									<< " - zero-copy texture sharing will be unavailable";
		}
	}

	wgpu::DeviceDescriptor deviceDesc {};
	deviceDesc.label = "ofxDawn device";
	deviceDesc.requiredFeatureCount = requiredFeatures.size();
	deviceDesc.requiredFeatures = requiredFeatures.data();
	deviceDesc.SetUncapturedErrorCallback(
		[](const wgpu::Device &, wgpu::ErrorType type, wgpu::StringView message) {
			ofLogError("ofxDawn") << "uncaptured error (" << static_cast<int>(type) << "): " << sv(message);
		});
	deviceDesc.SetDeviceLostCallback(
		wgpu::CallbackMode::AllowProcessEvents,
		[](const wgpu::Device &, wgpu::DeviceLostReason reason, wgpu::StringView message) {
			// Intentional teardown reports as Destroyed; only warn on the rest.
			if (reason != wgpu::DeviceLostReason::Destroyed) {
				ofLogWarning("ofxDawn") << "device lost (" << static_cast<int>(reason) << "): " << sv(message);
			}
		});

	bool deviceDone = false;
	adapter.RequestDevice(
		&deviceDesc, wgpu::CallbackMode::AllowProcessEvents,
		[&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView message) {
			if (status == wgpu::RequestDeviceStatus::Success) {
				device = std::move(d);
			} else {
				ofLogError("ofxDawn") << "RequestDevice failed: " << sv(message);
			}
			deviceDone = true;
		});
	while (!deviceDone) instance.ProcessEvents();
	if (!device) return false;

	queue = device.GetQueue();

	wgpu::AdapterInfo info {};
	adapter.GetInfo(&info);
	ofLogNotice("ofxDawn") << "WebGPU ready - device: " << sv(info.device)
						   << " | backend: " << static_cast<int>(info.backendType);

	bSetup = true;
	return true;
}

void ofxDawn::processEvents() {
	if (instance) instance.ProcessEvents();
}

void ofxDawn::waitForGPU() {
	if (!queue) return;
	bool done = false;
	queue.OnSubmittedWorkDone(
		wgpu::CallbackMode::AllowProcessEvents,
		[&](wgpu::QueueWorkDoneStatus, wgpu::StringView) { done = true; });
	while (!done) instance.ProcessEvents();
}

wgpu::ShaderModule ofxDawn::createShaderModule(const std::string & wgsl, const std::string & label) {
	wgpu::ShaderSourceWGSL wgslSource {};
	wgslSource.code = std::string_view(wgsl);

	wgpu::ShaderModuleDescriptor desc {};
	desc.nextInChain = &wgslSource;
	desc.label = std::string_view(label);

	return device.CreateShaderModule(&desc);
}
