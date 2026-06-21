#include "ofxDawnSharedTexture.h"

#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>

ofxDawnSharedTexture::~ofxDawnSharedTexture() {
	if (glTexID != 0) {
		glDeleteTextures(1, &glTexID);
		glTexID = 0;
	}
	if (ioSurface != nullptr) {
		CFRelease(static_cast<IOSurfaceRef>(ioSurface));
		ioSurface = nullptr;
	}
}

bool ofxDawnSharedTexture::setup(ofxDawn & dawn, int w, int h) {
	if (!dawn.isSetup()) {
		ofLogError("ofxDawnSharedTexture") << "ofxDawn is not set up";
		return false;
	}
	width = w;
	height = h;

	// --- 1. Create the shared IOSurface (BGRA8) ---
	NSDictionary * props = @{
		(id)kIOSurfaceWidth : @(w),
		(id)kIOSurfaceHeight : @(h),
		(id)kIOSurfaceBytesPerElement : @(4),
		(id)kIOSurfacePixelFormat : @((unsigned int)'BGRA'),
	};
	IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
	if (surface == nullptr) {
		ofLogError("ofxDawnSharedTexture") << "IOSurfaceCreate failed";
		return false;
	}
	ioSurface = surface;

	// --- 2. Import it into WebGPU as a render target ---
	const wgpu::Device & device = dawn.getDevice();

	wgpu::SharedTextureMemoryIOSurfaceDescriptor ioDesc {};
	ioDesc.ioSurface = ioSurface;
	wgpu::SharedTextureMemoryDescriptor stmDesc {};
	stmDesc.label = "ofxDawn IOSurface";
	stmDesc.nextInChain = &ioDesc;
	sharedMem = device.ImportSharedTextureMemory(&stmDesc);
	if (!sharedMem) {
		ofLogError("ofxDawnSharedTexture") << "ImportSharedTextureMemory failed";
		return false;
	}
	// nullptr -> texture uses all usages supported by the IOSurface memory
	// (includes RenderAttachment + TextureBinding).
	sharedTexture = sharedMem.CreateTexture();
	textureView = sharedTexture.CreateView();

	// --- 3. Bind the same IOSurface to a GL rectangle texture ---
	CGLContextObj cgl = CGLGetCurrentContext();
	if (cgl == nullptr) {
		ofLogError("ofxDawnSharedTexture") << "no current CGL context (call setup after the OF window exists)";
		return false;
	}
	glGenTextures(1, &glTexID);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, glTexID);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	CGLError err = CGLTexImageIOSurface2D(
		cgl, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, w, h,
		GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	if (err != kCGLNoError) {
		ofLogError("ofxDawnSharedTexture") << "CGLTexImageIOSurface2D failed: " << CGLErrorString(err);
		return false;
	}

	// --- 4. Wrap the GL texture as an ofTexture ---
	texture.setUseExternalTextureID(glTexID);
	ofTextureData & td = texture.texData;
	td.textureID = glTexID;
	td.textureTarget = GL_TEXTURE_RECTANGLE_ARB;
	td.width = w;
	td.height = h;
	td.tex_w = w;
	td.tex_h = h;
	td.tex_t = w; // rectangle textures use pixel coordinates
	td.tex_u = h;
	td.glInternalFormat = GL_RGBA;
	td.bFlipTexture = false;
	td.bAllocated = true;

	bSetup = true;
	ofLogNotice("ofxDawnSharedTexture") << "shared " << w << "x" << h << " IOSurface ready";
	return true;
}

void ofxDawnSharedTexture::beginAccess() {
	if (!bSetup) return;
	wgpu::SharedTextureMemoryBeginAccessDescriptor desc {};
	desc.concurrentRead = false;
	desc.initialized = accessInitialized;
	desc.fenceCount = 0;
	desc.signaledValueCount = 0;
	sharedMem.BeginAccess(sharedTexture, &desc);
}

void ofxDawnSharedTexture::endAccess() {
	if (!bSetup) return;
	wgpu::SharedTextureMemoryEndAccessState state {};
	sharedMem.EndAccess(sharedTexture, &state);
	// After the first render the surface holds valid content.
	accessInitialized = true;
}
