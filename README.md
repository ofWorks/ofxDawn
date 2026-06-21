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
  WebGPU storage buffer (advanced by a compute shader) and is **rendered on the
  GPU** as points straight into a shared texture — the data never leaves the GPU.
- `ofxDawnSharedTexture` — **zero-copy** bridge: an `IOSurface` backs both a
  WebGPU render target and an OpenGL texture, exposed as an `ofTexture`.
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
#include "ofxDawnSharedTexture.h"

ofxDawn dawn;
ofxDawnParticles particles;
ofxDawnSharedTexture sharedTex;

void ofApp::setup() {
    dawn.setup();
    sharedTex.setup(dawn, ofGetWidth(), ofGetHeight());
    particles.setup(dawn, 200000, glm::vec2(ofGetWidth(), ofGetHeight()));
}

void ofApp::update() {
    // simulate + render on the GPU, into the shared texture
    particles.update(ofGetLastFrameTime(),
                     glm::vec2(ofGetMouseX(), ofGetMouseY()), sharedTex);
}

void ofApp::draw() {
    sharedTex.getTexture().draw(0, 0); // zero-copy, no readback
}
```

## How rendering works (zero-copy interop)

Particles are simulated by a compute shader and **rendered on the GPU** as a
point list, straight into a WebGPU texture that is backed by an `IOSurface`
(imported via `SharedTextureMemoryIOSurfaceDescriptor`). That same `IOSurface`
is bound to a `GL_TEXTURE_RECTANGLE` with `CGLTexImageIOSurface2D` and wrapped
as an `ofTexture`, so OF draws it directly — **no CPU round-trip and no vertex
upload**. `ofxDawnSharedTexture` owns this bridge; the surface is rendered
between `beginAccess()` / `endAccess()`.

Synchronization: OpenGL can't wait on Metal's `MTLSharedEvent` fences, so after
submitting the WebGPU work the addon does a CPU `waitForGPU()` (queue
`OnSubmittedWorkDone`) before OF samples the texture. This is the one remaining
GPU sync per frame; eliminating it (e.g. by pipelining across frames) is a
possible future optimization.

## Roadmap

- [x] WebGPU device on the Metal backend + WGSL compute particle system
- [x] Zero-copy `IOSurface` interop (WebGPU render target ⇄ `ofTexture`),
      replacing the CPU readback / `ofVboMesh` path
- [ ] Remove the per-frame CPU `waitForGPU()` sync (pipeline across frames so
      the GPU isn't stalled each draw)
- [ ] Handle window resize (re-create the shared texture and update bounds)
- [ ] Sized/round point sprites instead of 1px points (instanced quads)
- [ ] Port the "Fulcrum MARU S³" 3D gravity demo (multi-pass: compute → HDR
      trail → tonemap → IOSurface, glm camera, instanced well/probe geometry,
      ofxMicroUI controls). Shaders port nearly verbatim; main work is the
      extra render passes, uniform-layout matching, and UI wiring.
- [ ] CT / DICOM volume visualizer — load tomography (e.g. skull) data as
      particles, slice by hardness (HU window), color-map via a transfer
      function, view in 3D. Shares the glm-camera / 3D-point milestone with the
      Fulcrum port. Plan + conversion workflow in
      [docs/CT_VOLUME.md](docs/CT_VOLUME.md); pre-convert DICOM with
      [tools/dicom_to_nrrd.py](tools/dicom_to_nrrd.py).

## License

Dawn is BSD-licensed (see `libs/dawn/LICENSE`).
