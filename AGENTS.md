# AGENTS.md — ofxDawn

Operational notes for AI agents. Architecture, usage, and roadmap live in
[README.md](README.md) — read it first; this file only adds the agent-specific
build/verify loop and the gotchas that waste time.

## Build & verify loop

This is the real correctness check — the standalone editor cannot resolve OF or
Dawn headers, so trust the build, not the linter.

```sh
cd example
ofGen          # generates chalet/zed/xcode projects from of.yml
chalet build   # compiles OF + the addon (~10s incremental)
./build/arm64-apple-darwin_Release/example   # run it
```

A healthy run logs exactly these three lines:

```
ofxDawn: WebGPU ready - device: <gpu> | backend: 5      (5 = Metal)
ofxDawnSharedTexture: shared <w>x<h> IOSurface ready
ofxDawnParticles: simulating + rendering <n> particles on the GPU
```

No Dawn validation errors should appear. After verifying, delete the generated
example artifacts (they're gitignored): `build/ .zed/ *.xcodeproj/ chalet.yaml
Project.xcconfig compile_flags.txt of.entitlements bin/ .chaletrc`.

## Gotchas

- **Editor diagnostics are false here.** `'ofMain.h' file not found`, undeclared
  `wgpu`, unknown `ofTexture`, etc. appear because there's no compile_flags
  outside a generated project. They are NOT real — only `chalet build` decides.
- **Platform string is `macos`, not `osx`.** ofGen auto-detects the static lib
  only under `libs/dawn/lib/macos/`, and reads `ADDON_FRAMEWORKS` only from the
  `common:` / `macos:` sections of `addon_config.mk`. The `osx:` section exists
  for stock openFrameworks PG/makefiles and is ignored by ofGen.
- **Dawn is the monolithic `libwebgpu_dawn.a`** — it exports the standard `wgpu*`
  C symbols, so no proc-table wiring. Include `<webgpu/webgpu_cpp.h>`. This is a
  recent Dawn (Futures + `StringView` API); async setup is driven synchronously
  by spinning `instance.ProcessEvents()` (see `ofxDawn::setup`).
- **macOS only.** `ofxDawnSharedTexture::setup` needs a current CGL context, so
  call it from `ofApp::setup` (after the window exists), not before.
- **`.mm` not `.cpp`** for `ofxDawnSharedTexture` — it uses IOSurface / CGL /
  Foundation. The chalet base compiles it as Objective-C++ with ARC.
- **Keep this file lean.** Don't duplicate the README; update both if the build
  flow or these invariants change.
