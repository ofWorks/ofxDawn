#pragma once

// ofxDawnSharedTexture - zero-copy bridge between a WebGPU (Metal) render
// target and an openFrameworks ofTexture, via a shared IOSurface.
//
// The same IOSurface backs:
//   - a wgpu::Texture (imported through SharedTextureMemory) that WebGPU
//     renders into, and
//   - a GL_TEXTURE_RECTANGLE bound with CGLTexImageIOSurface2D, exposed as an
//     ofTexture for OF to draw.
//
// No pixels are copied: WebGPU writes the surface, OF samples it directly.
// Wrap GPU work between beginAccess() and endAccess(); after submitting, call
// ofxDawn::waitForGPU() before drawing so the Metal work is visible to GL.

#include "ofMain.h"
#include "ofxDawn.h"

class ofxDawnSharedTexture {
public:
	~ofxDawnSharedTexture();

	bool setup(ofxDawn & dawn, int width, int height);

	// Transitions the underlying texture into WebGPU access. Call before
	// encoding a render pass that targets it.
	void beginAccess();
	// Releases WebGPU access (call after Submit).
	void endAccess();

	const wgpu::TextureView & getTextureView() const { return textureView; }
	ofTexture & getTexture() { return texture; }
	int getWidth() const { return width; }
	int getHeight() const { return height; }
	bool isSetup() const { return bSetup; }

private:
	bool bSetup = false;
	bool accessInitialized = false;
	int width = 0;
	int height = 0;

	void * ioSurface = nullptr; // IOSurfaceRef
	unsigned int glTexID = 0;
	ofTexture texture;

	wgpu::SharedTextureMemory sharedMem;
	wgpu::Texture sharedTexture;
	wgpu::TextureView textureView;
};
