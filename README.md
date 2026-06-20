# ofxDawn

WebGPU for openFrameworks on macOS, powered by [Dawn](https://dawn.googlesource.com/dawn)
(the same WebGPU implementation used in Chromium).

The addon brings up a headless WebGPU device on Dawn's **Metal** backend and
provides a GPU **particle system** simulated with a WGSL compute shader and
rendered through openFrameworks.

> Platform: **macOS only** (Apple Silicon + Intel — the bundled lib is a
> universal binary). The Dawn backend here is Metal.

## What's included

- `ofxDawn` — WebGPU context: instance → adapter → device → queue, plus a
  `createShaderModule()` helper. Async WebGPU requests are driven synchronously
  by pumping `ProcessEvents()`.
- `ofxDawnParticles` — a GPU particle system. The simulation lives entirely in a
  WebGPU storage buffer and is advanced by a compute shader each frame; the
  positions are read back and drawn as an `ofVboMesh` of points.
- `example/` — particles that chase the mouse (200k by default).

## The bundled library

`libs/dawn/lib/macos/libwebgpu_dawn.a` is Dawn's monolithic static build. It
exports the standard `wgpu*` C symbols directly, so there's no proc-table wiring
— `#include <webgpu/webgpu_cpp.h>` and call `wgpu::CreateInstance()`.

The Metal backend pulls in these frameworks (declared in `addon_config.mk`):
`Metal`, `IOSurface`, `Foundation` (plus `QuartzCore`, `IOKit`, `Cocoa`, which
the ofWorks core already links).

## Building (ofWorks / ofGen)

```sh
cd example
ofGen          # generates chalet/zed/xcode projects from of.yml
chalet build   # build
chalet run     # run
```

`ofGen` reads `addon_config.mk` and auto-detects the static lib under
`libs/dawn/lib/macos/`, the headers under `libs/dawn/include/`, and the
frameworks from the `macos:` section — so a project's `of.yml` only needs to
list `ofxDawn` under `addons:`.

## Usage

```cpp
#include "ofxDawn.h"
#include "ofxDawnParticles.h"

ofxDawn dawn;
ofxDawnParticles particles;

void ofApp::setup() {
    dawn.setup();
    particles.setup(dawn, 200000, glm::vec2(ofGetWidth(), ofGetHeight()));
}

void ofApp::update() {
    particles.update(ofGetLastFrameTime(), glm::vec2(ofGetMouseX(), ofGetMouseY()));
}

void ofApp::draw() {
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    particles.draw();
}
```

## How rendering works (and the next step)

Right now the compute result is **read back** to the CPU each frame and pushed
into an `ofVboMesh`. This keeps rendering in plain OF/OpenGL with zero interop
code and is correct and simple; the per-frame map is a GPU sync, comfortable for
tens of thousands of particles.

The natural optimization is **zero-copy interop**: render the particles in
WebGPU into an `IOSurface`-backed texture (`SharedTextureMemoryIOSurfaceDescriptor`,
already present in the Dawn headers) and wrap that same `IOSurface` as an OpenGL
texture via `CGLTexImageIOSurface2D` — exposing it to OF as an `ofTexture` with
no CPU round-trip. That's planned but not yet implemented.

## License

Dawn is BSD-licensed (see `libs/dawn/LICENSE`).
