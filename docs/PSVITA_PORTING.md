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
- **Native controller adapter (new):** `port/input/input_vita.c` maps Vita `sceCtrl` samples into the existing virtual-pad interface and explicitly clears the virtual pad on sampling errors to avoid stuck buttons. It is opt-in with `ICO_INPUT_VITA=ON`; the VitaSDK CI cross-compiles it and a host mock test checks mapping and failure handling. It currently uses a fixed physical-button mapping, not the desktop configurable bindings, and is not yet wired into a complete Vita main loop.
- **Audio:** `port/audio/CMakeLists.txt` separates the software SPU2 and
  ADPCM/mixing logic from the SDL output sink (`out_sdl.c`). The new
  `out_vita.c` sink uses `sceAudioOut` with fixed 1024-frame submissions and
  accumulates the host loop's variable-size blocks; CMake selects it with
  `-DICO_AUDIO_VITA=ON`. It is compile-gated in VitaSDK CI, but hardware audio
  and latency still need testing.
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
- **Renderer compile audit (CI runs 38021850043, 38024007273 and
  38024257423):** VitaSDK GCC compiled 23 CPU-side renderer/support translation
  units with strict warnings: six software VU1 references, `xxh3.c`,
  `texpack_name.c`, and 15 renderer core, texture, presentation, and
  post-processing units (`rd_frame.c`, `rd_interp.c`, `rd_mesh.c`,
  `rd_perf.c`, `rd_pipeline.c`, `rd_tex.c`, `rd_core.c`, `rd_dump.c`,
  `rd_post.c`, `rd_shadow.c`, `rd_water.c`, `rd_present.c`, `rd_blur.c`,
  `rd_crt.c`, `rd_png.c`). All 23 objects passed ELF32/ARM checks. This is
  compile-only: the objects are not linked into an app and do not prove GPU
  functionality.
- **ARMv7 render-target guard fix (CI run 38024257423):** compiling `rd_core.c`
  exposed `-Werror=type-limits` on the old `id >= RD_TARGET_COUNT` comparison
  in `rd_target`. GCC's target-specific enum range analysis treated that
  comparison as impossible. The guard now converts the ID and sentinel to
  `unsigned` before comparing, which rejects negative and sentinel/out-of-range
  IDs without the ARM warning. Two regression assertions for negative and
  sentinel IDs were added to `rd_state_test.c`; that host test was not executed
  by the Vita-only workflow.
- **Latest verified CI baseline before the diagnostics-audit commits (run 38030131579):** all three jobs passed: the VitaSDK probe, bit-exact host softdouble regression, and the full 214-unit game-source audit. The SDK job compiled the 23 renderer/support units, ten portable input units including input_config.c, and selected platform units, and structurally validated a minimal probe VPK. The VPK contains only a trivial main; it is not an ICO build. The later diagnostics-audit and documentation commits have not been verified by a retrieved CI result in this session.
- **Vita memory/FP platform fixes:** arena.c now uses a Vita user-RW kernel memblock instead of unavailable sys/mman.h, checks allocation/base-address errors, and zeroes the allocation. This compiles in VitaSDK CI, but allocating the full 41 MiB block on hardware remains unverified. fpenv.c uses ARM VFP FPSCR controls for rounding/flush-to-zero; hardware FP exception trapping is not provided by the Cortex-A9 VFP path, so ICO_FPTRAP is rejected rather than silently emulated. Runtime FP semantics still need device tests.
- **Vita arena failure-path regression (new CI coverage):** `port/platform/test/arena_vita_test.c` compiles the Vita-specific `sceKernelAllocMemBlock` / `sceKernelGetMemBlockBase` path against a host mock. It exercises successful allocation, one-shot allocation failure and retry, base-address failure and cleanup/retry, 1 MiB alignment, zero-fill, and EE-address mapping. This checks the C branch and its recovery logic, not Vita kernel behavior. The test was added to the workflow after the last retrieved passing baseline; its result must be inspected in a newer run before calling it verified.
- **Native graphics candidate:** [vitaGL](https://github.com/Rinnegatamante/vitaGL) is an existing OpenGL wrapper over the Vita's sceGxm API, so it is a concrete candidate for a first textured-triangle prototype rather than assuming Vulkan exists. Its README states that modern builds require the console's `libshacccg.suprx` runtime shader compiler and that its OpenGL support is a subset. This is only a candidate: the project's DXC/HLSL shaders need a separate GLSL/shader path, and the RHI's integer render target, blending, depth/stencil and binding semantics must be individually tested before committing to this backend.
- **RHI contract and shader pipeline:** `port/rhi/rhi.h` describes a vertex +
  fragment pipeline (no compute), two frames in flight, storage-read buffers
  for bones/particles, and explicit render-target/state transitions. The shader
  list has 51 direct registrations plus four no-dual variants generated by a
  CMake loop, spanning 14 unique HLSL files. `cmake/IcoShaders.cmake` invokes
  DXC for shader-model 6.0 and emits Vulkan 1.2 SPIR-V plus optional D3D12 DXIL;
  there is no Vita shader-output route. The Vulkan backend requires Vulkan 1.2
  facilities and an SDL-created surface/swapchain, neither of which establishes
  a Vita implementation.
- **Important renderer capability gate:** the existing Vulkan backend requires
  RGBA8 (blendable), RGBA8_UINT, R8, D32F, and either D32F_S8 or D24S8. The
  renderer uses RGBA8_UINT for its exact integer-blend pass; stencil wrap is a
  required RHI capability, while dual-source blending has a two-pass fallback
  and block-compressed BC textures are optional. A Vita backend must prove or
  deliberately emulate these semantics; compiling CPU renderer files is not
  evidence that the GPU formats, blending, or shader bindings are supported.
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

**Earlier CI milestone (run 38027247903, commit 102da244):** all three jobs passed: the VitaSDK probe, bit-exact host softdouble regression, and the full 214-unit game-source audit. Later commits extended selected platform-service coverage, including `fiber.c` and `sched.c`; consult the latest workflow run rather than treating this historical run as current.

### Gate 2 — Game-code compile audit

- The compile-only audit currently covers all 214 game C translation units and
  checks its source list against the canonical link-order inventory.
- Preserve source-order, generated-file, include-order and semantic-flag checks.
- Next extend the audit to the platform-independent port libraries and classify
  any unsupported target APIs separately from the game sources.
- Do not stub required game behavior merely to make a build green.


- **Fiber/scheduler cross-compile gate (added in the current branch):** `fiber.c` selects minicoro's ARM EABI assembly context switch, but its guard-page allocator depends on POSIX virtual-memory APIs that are not part of Vita's userland contract. On Vita, the implementation now keeps the ARM assembly switch and explicitly uses its existing zeroed `calloc`/`free` stack allocator; desktop POSIX and Windows retain guarded stacks. This avoids compiling or linking unsupported VM calls, but intentionally gives up the guard page on Vita. The CI toolchain job now compiles `fiber.c` (with `ICO_VITA_TARGET=1` to exercise this branch and a compile-time assertion that the POSIX guard allocator stays disabled), `sched.c`, `kernel_host.c`, `window_lifecycle.c`, and `assert_host.c` under strict warnings; this covers the ARM context-switch assembly, scheduler, EE kernel-call bridge, platform-independent lifecycle table, and assertion bridge at object level. Passing that compile gate will establish source/ABI compatibility only; scheduler switching and stack behavior still require runtime tests on the Vita or a suitable emulator.

- **Native audio sink implemented (VitaSDK compile passed; host regression added):** `port/audio/out_vita.c` uses Vita's `sceAudioOut` main port at 48 kHz stereo S16. Because Vita output submissions use a fixed block length, it accumulates the game's variable 800/801-frame NTSC and 960-frame PAL-ish vsync blocks and submits complete 1024-frame chunks. Output volume is applied in software; the SNDN2/SPU2 simulation continues if the output port cannot be opened. `ICO_AUDIO_VITA=ON` explicitly selects this sink in CMake, leaving the desktop SDL path unchanged. A host-side mock regression checks fixed-block chunking, sample ordering, volume scaling, and output-error cleanup; VitaSDK CI compiles the real API calls. This is not proof of audible runtime playback.

- **Next platform blocker — diagnostics:** `port/platform/diag_host.c` is not in the Vita compile audit and remains a desktop implementation. Source inspection finds dependencies on POSIX signals/alternate signal stacks, `ucontext_t`, pthreads (including `pthread_getattr_np` and `pthread_kill`), `/proc/self/status`, `/proc/self/exe`, `dladdr`, and POSIX file descriptors. The root platform target also requires `Threads::Threads`. A native Vita build needs a separate, explicitly scoped diagnostic/threading implementation and Vita-specific CMake source selection; the successful compile of selected services is not evidence that `ico_platform` can link as-is. The `diagnostics-portability-audit` CI job now compiles this host file unchanged and expects a failure specifically tied to a known desktop-only dependency; this is a blocker detector, not a passing Vita implementation. Inspect the workflow result for the latest branch tip before treating this audit as validated; a successful baseline run does not validate the newly added blocker detector.

### Gate 3 — Platform services

Implement and test Vita replacements for lifecycle, timing, scheduler/threading,
memory, filesystem, input and audio. Add unit tests for logic that can run on
the host, and small on-device smoke tests for Vita-specific calls.

### Gate 4 — Rendering prototype

- Build a Vita GPU capability matrix against the RHI's required formats and
  semantics, starting with RGBA8_UINT integer blending, stencil wrap, depth
  sampling/copies, storage-read data, and shader bindings.
- Prototype a textured triangle with the actual Vita-supported graphics API,
  then add targeted tests for the features above before implementing the game
  renderer.
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

The latest completed VitaSDK run before the native audio-sink addition passed all
three jobs, including the 214/214 game-source audit. The fiber/scheduler ARMv7
compile gate is now in place and passed; it checks source compatibility only.
The native Vita audio sink and host-side buffer regression have been added; the current CI run is checking the real VitaSDK API and integration compile. Next, resolve the diagnostics/platform source-selection blocker, then
prototype a Vita-supported textured triangle and validate the renderer's
integer target, depth/stencil, and shader requirements. The controller adapter,
native application entry point, full-game link, and runtime tests remain
outstanding. The existing smoke VPK is not an ICO game build.
