# PS Vita port feasibility audit

## Goal

Investigate a native PlayStation Vita homebrew port of ICO using this repository's
reconstructed C game code. This is a single-game native port, not a PS2 emulator.
Do not claim the game boots or is playable until a VPK has been tested on Vita
hardware or a suitable emulator.

## Confirmed baseline

- The current host build is C11/GNU C and its CMake presets target x86-64
  Windows/Linux and 64-bit Android ARM (AArch64).
- The Android toolchain preset is explicitly ARM64; the Vita's userland target
  is 32-bit ARMv7-A. The existing Android preset is not reusable as-is.
- The game calls a host compatibility layer under `port/` for system services,
  scheduling, memory, file access, audio, input, timing and rendering.
- The rendering abstraction under `port/render/` targets a Vulkan RHI today;
  the existing Vulkan backend and SDL window/surface setup cannot be assumed to
  work on Vita. A Vita-native renderer or an independently verified compatible
  backend is required.
- The current project has no VitaSDK CMake preset or Vita-specific platform
  implementation.
- The build does not bundle game data. Runtime testing must use the supported
  user-provided PAL disc image, and no disc image, ELF, extracted game assets,
  or asset-derived data may be committed.

## Source-level compatibility findings

These are confirmed from the current CMake files and source includes, not from a
Vita game build:

- **Build target:** the root `CMakeLists.txt` defines only the desktop/Android
  host build, applies `ICO_HOST=1`, and conditionally selects a headless or SDL
  window build. `CMakePresets.json` has no Vita preset. The new CI smoke project
  is deliberately separate from the root build.
- **Renderer:** `port/rhi/CMakeLists.txt` builds the Vulkan backend (and D3D12
  for 64-bit Windows); the Vulkan device/swapchain paths include SDL3 Vulkan
  surface support. The game's renderer therefore cannot be made Vita-compatible
  by merely changing the compiler target or turning off D3D12. A Vita renderer
  path and shader/RHI capability mapping remain a major blocker.
- **Platform services:** `port/platform/CMakeLists.txt` adds host scheduling,
  fiber, kernel and lifecycle code and links `Threads::Threads`. The windowed
  executable also pulls in host-loop/window code. Vita needs a deliberate
  platform implementation, not a wholesale reuse of these host files.
- **Input:** `port/input/CMakeLists.txt` separates portable mapping, virtual-pad
  and recording logic from `input_sdl.c`, which is only included for the SDL
  window build. The portable mapping logic is a reuse candidate; a Vita
  controller adapter still needs to be implemented and tested.
- **Audio:** `port/audio/CMakeLists.txt` separates the software SPU2 and
  ADPCM/mixing logic from the SDL output sink (`out_sdl.c`). A Vita audio sink
  and lifecycle integration are still required.
- **Measured ARMv7 scope:** the CI probe cross-compiles `port/math/ps2float.c`
  plus the portable SPU2 core (`adpcm.c`, `spu2.c`, `spu2_sd.c`,
  `spu2_tables.c`, `volume.c`, `mix_gain.c`) with `-Wall -Wextra -Werror` and
  checks every object header for ELF32/ARM. This verifies those translation
  units only; it is not a game-wide compile audit.

## Feasibility risks to measure early

1. **Compiler/ABI:** unsupported host APIs, ARMv7 compiler errors, alignment,
   64-bit assumptions and floating-point behavior.
2. **Rendering:** Vulkan/SDL assumptions, shader compilation and RHI feature
   coverage. This is likely a major porting task, not a simple backend rename.
3. **Memory:** PS2 emulated RAM arena, game heap partitions, texture/model assets,
   render targets and frame queues must fit the Vita's constrained memory.
4. **Threads/timing:** host fibers, pthreads, timers and frame pacing must map
   to supported VitaSDK facilities without changing game scheduling semantics.
5. **Input/audio/filesystem:** replace SDL and POSIX-specific services with Vita
   controller, audio and filesystem integrations.
6. **Legal/data constraints:** retain the upstream notices and only support
   user-supplied game data; do not add copyrighted disc content.

## Work plan and gates

### Gate 0 — Preserve upstream and establish reproducibility

- Keep Vita work isolated on a feature branch.
- Record upstream commit and source/dependency audit findings.
- Keep the existing desktop/Android CI behavior unchanged.

### Gate 1 — Cross-compile probe

- Install the pinned VitaSDK container in CI and verify the required compiler,
  linker and packaging tools are present.
- Compile a tiny ARMv7 object and two existing portable production modules
  (`port/math/ps2float.c` and `port/audio/adpcm.c`) with warnings treated as errors.
  Use VitaSDK's documented CMake helpers to create a minimal SELF/VPK, check the
  archive structure, and retain the smoke-test package as a short-lived CI artifact.
- This smoke-test VPK contains only a `main` that returns immediately; it does
  not compile ICO sources or demonstrate graphics, input, audio or game startup.
  Only after the probe passes should an experimental repository toolchain and
  explicit Vita source allowlist be introduced.

### Gate 2 — Game-code compile audit

- Create an explicit Vita source/dependency allowlist rather than compiling all
  desktop platform sources.
- Cross-compile the platform-independent game code and resolve errors one class
  at a time.
- Preserve the source-order and generated-file checks.
- Do not stub required game behavior merely to make a build green.

### Gate 3 — Platform services

Implement and test Vita replacements for lifecycle, timing, scheduler/threading,
memory, filesystem, input and audio. Add unit tests for logic that can run on
the host, and small on-device smoke tests for Vita-specific calls.

### Gate 4 — Rendering prototype

- Inventory the exact RHI features and shader formats the game uses.
- Build a Vita GPU capability matrix and prototype one simple textured triangle
  or equivalent render test using Vita-supported APIs.
- Choose a renderer only after a real device/API probe. Do not assume Vulkan is
  available on Vita.
- Then implement the minimum RHI needed by the game's renderer, with explicit
  fallbacks for unsupported formats/features.

### Gate 5 — Boot and gameplay

- Produce a VPK, install it, and capture logs from a real Vita or compatible
  emulator.
- Verify boot, controller input, scene rendering, audio, save behavior, and
  frame pacing separately.
- Publish performance and compatibility results with the exact device/build;
  do not claim full playability based on a successful link alone.

## Immediate next action

Extend the VitaSDK-only CI probe from the confirmed minimal VPK build to cross-compile selected portable production modules with strict warnings. Once that passes, add a Vita-specific CMake toolchain/preset without changing desktop/Android workflows, then expand the source allowlist incrementally. A green smoke-test VPK is not an ICO game build.
