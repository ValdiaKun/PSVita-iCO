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
- **Verified ARMv7 compile scope (CI run 38018639427):** VitaSDK GCC 15.2 compiled
  `port/math/ps2float.c`; the portable SPU2 core (`adpcm.c`, `spu2.c`,
  `spu2_sd.c`, `spu2_tables.c`, `volume.c`, `mix_gain.c`); seven math units
  (`cloth.c`, `libvu0.c`, `matrix.c`, `matrix_drive.c`, `matrix_stack.c`,
  `quaternion.c`, `softdouble.c`); and nine portable input units (`bindings.c`,
  `input_record.c`, `mouse_camera.c`, `mouse_look.c`, `pad_host.c`,
  `pad_script.c`, `pointer.c`, `touch.c`, `vpad.c`) with strict warnings.
  Every object was verified as ELF32/ARM.
- **Full game translation-unit audit (CI run 38021283325):** the VitaSDK compiler
  successfully compiled all 214 host game C translation units selected from
  `config/link_order.pal.txt`, with the source inventory cross-checked against
  generated CMake lists and the host semantic flags/include order mirrored by
  `tools/vita_source_audit.py`. The run also passed the isolated softdouble
  regression and SDK smoke-package job. This is a compile-only audit: it does
  not link the game or compile/verify the host platform, RHI, shader, and
  window-specific dependency chain.
- **CPU-side render support compile audit (CI run 38021850043):** VitaSDK GCC
  compiled the six software VU1 reference translation units (`vu1_ref.c`,
  `vu1_normal.c`, `vu1_scissor.c`, `vu1_cluster.c`, `vu1_mesh.c`,
  `vu1_particle.c`) plus `xxh3.c` and `texpack_name.c` with strict warnings.
  All eight objects were checked as ELF32/ARM. These CPU-side helpers do not
  constitute a Vita GPU backend and were not linked into an application.
- **ARMv7 compatibility fix verified:** the expanded audit exposed that ARMv7
  GCC 15.2 rejects `unsigned __int128` in `softdouble.c`. Multiplication now
  uses explicit 32-bit limbs to form the 128-bit product, and division uses
  restoring quotient generation with a bounded 64-bit remainder. The host
  bit-exact softdouble oracle regression passes, and the updated `softdouble.c`
  compiles for ARMv7 with `-Wall -Wextra -Werror`.
- **Bounded game-layout fix verified (CI run 38021283325):** the source audit
  exposed a target-specific layout assertion in `ico2/fumi/src/boyact.c`.
  The current change permits the bounded `BoyInfo` layout on Vita, and the
  subsequent all-source compile job passed. This validates compilation only,
  not runtime memory safety or the correctness of the game behavior on Vita.

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
- Compile a tiny ARMv7 object and existing portable production modules with
  warnings treated as errors. Use VitaSDK's documented CMake helpers to create
  a minimal SELF/VPK, check the archive structure, and retain the smoke-test
  package as a short-lived CI artifact.
- This smoke-test VPK contains only a `main` that returns immediately; it does
  not compile ICO sources or demonstrate graphics, input, audio or game startup.

### Gate 2 — Game-code compile audit

- The compile-only audit currently covers all 214 game C translation units and
  checks its source list against the canonical link-order inventory.
- Preserve source-order, generated-file, include-order and semantic-flag checks.
- Next extend the audit to the platform-independent port libraries and classify
  any unsupported target APIs separately from the game sources.
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

The game-source compile audit and selected CPU-side render helpers now pass on
ARMv7, but the project still has no Vita platform entry/lifecycle, audio sink,
controller adapter, or native GPU backend. Next inventory the RHI capabilities
and shader formats actually used, then prototype a Vita-supported textured
triangle before attempting to map the renderer. The existing smoke VPK is not
an ICO game build.
