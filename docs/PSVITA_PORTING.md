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
- **Native controller adapter:** `port/input/input_vita.c` maps Vita `sceCtrl` samples into the existing virtual-pad interface and explicitly clears the virtual pad on sampling errors to avoid stuck buttons. With `ICO_INPUT_VITA=ON`, `port/platform/host_loop.c` initializes it before the game boot, samples it before each game's vblank handler/Main tick, and registers shutdown cleanup. `port/input/CMakeLists.txt` links `ico_input` into `ico_pc`, which also supplies the game's `scePadRead` bridge. CI checks the CMake compile definitions and generated executable link graph, and the host mock test checks mapping and failure handling. The adapter still uses a fixed physical-button mapping rather than desktop configurable bindings; this integration is headless and does not provide a display loop or render ICO. Physical input remains untested.
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
- **Latest verified CI run (run 38081797560, commit `ef89a82785d58a5dd9e9d94c9941813a74f48b5f`):** all six jobs passed: VitaSDK cross-compilation, the 214-unit game-source audit, the portable softdouble/audio/controller regressions, the Vita arena mock regression, the diagnostics portability blocker check, and the independent vitaGL renderer feasibility build. The SDK job also compiled the selected 23 CPU-side renderer units, 14 platform-service units, and Vita input/audio adapters, and built/validated the trivial SDK smoke VPK. None of those compile-only checks links the ICO game.
- **Vita memory/FP platform fixes:** arena.c now uses a Vita user-RW kernel memblock instead of unavailable sys/mman.h, checks allocation/base-address errors, and zeroes the allocation. This compiles in VitaSDK CI, but allocating the full 41 MiB block on hardware remains unverified. fpenv.c uses ARM VFP FPSCR controls for rounding/flush-to-zero; hardware FP exception trapping is not provided by the Cortex-A9 VFP path, so ICO_FPTRAP is rejected rather than silently emulated. Runtime FP semantics still need device tests.
- **Vita arena failure-path regression (verified in run 38081797560):** `port/platform/test/arena_vita_test.c` compiles the Vita-specific `sceKernelAllocMemBlock` / `sceKernelGetMemBlockBase` path against a host mock. It exercises successful allocation, one-shot allocation failure and retry, base-address failure and cleanup/retry, 1 MiB alignment, zero-fill, and EE-address mapping. This checks the C branch and its recovery logic, not Vita kernel behavior.
- **vitaGL renderer feasibility spike (verified in run 38081797560):** CI fetched vitaGL at pinned commit `dca4b9d143290d78ec043131a19be36c78cdc7b5`, built and installed `libvitaGL.a`, then compiled, linked, packaged, and structurally inspected its `samples/immediate_mode` colored-quad VPK. The package SHA-256 is `a3e3dd301874222942f35f2a2c4126ff344cf652205b64c6bd846b5809579ce7`; the CI artifact is `vitaGL-immediate-mode-probe-vpk`. This proves the pinned library and sample can be built with VitaSDK; it does **not** prove that the sample runs or visibly renders on hardware/emulator, and it is not an ICO build. vitaGL requires the console's `libshacccg.suprx` runtime shader compiler. The actual renderer integration remains unimplemented: `rhi_backend.h` defines 68 backend operations, while `embed_shaders.py` emits only SPIR-V and DXIL blobs. ICO's HLSL includes `StructuredBuffer<float4>`, integer `Texture2D<uint4>` plus `.Load`, `SV_VertexID`, precise floating-point sequences, integer render targets, and dual-source blend outputs. A dedicated Vita backend and a compatible shader conversion/reimplementation path are still required; vitaGL is not a drop-in Vulkan replacement.
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

- **Diagnostics/platform source-selection blocker — resolved at object/library level:** `port/platform/diag_host.c` remains desktop-only (POSIX signals/alternate signal stacks, `ucontext_t`, pthread APIs, `/proc`, `dladdr`, and file descriptors). With `ICO_INPUT_VITA=ON`, CMake now selects `diag_vita.c` and avoids linking `Threads::Threads`; the Vita diagnostics backend compiles with VitaSDK and a host mock regression checks its supported behavior and explicit unsupported-capability reporting. CI also confirms that `diag_host.c` fails to compile unchanged on Vita for the expected `ucontext.h` dependency, and the actual `ico_platform` static library now builds successfully with the Vita selection. This closes the previous platform-target compilation blocker, but it does not provide the missing crash handler, stack unwinding, watchdog, or fatal UI integration.

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

Completed CI run `38105477329` at commit `e2baa3c8b28115432383316393875a968c241fab`: all six jobs passed. VitaSDK CI compiled the native Vita controller adapter and `host_loop.c` with both `ICO_HOST_VITA=1` and `ICO_INPUT_VITA=1`, built `ico_input`, `ico_platform`, and `ico_sndn2`, and verified that the generated `ico_pc` link graph includes `libico_input.a`. It also built the complete CMake `ico_game` static archive. That build exposed a target ABI mismatch: `LayoutClothDef.attr` is an EE 32-bit enum field, but the Vita toolchain selected short enum storage; adding `-fno-short-enums` to the game/data layout options fixed the static assertion. The source audit then passed all 214 game C translation units and verified each successful object is ELF32/ARM (214/214, zero failures). The run also passed the softdouble regression, arena mock regression, diagnostics portability audit, and project-owned vitaGL triangle VPK build/package checks. The probe VPK is a graphics test only; no visible rendering has been verified on hardware or emulator. The next gate is a full headless `ico_pc` executable link; even a successful link would not prove a Vita display loop, ICO rendering, or playable gameplay.


## Vita diagnostics backend (2026-10)

When `ICO_INPUT_VITA=ON`, CMake selects `port/platform/diag_vita.c` instead of the desktop `diag_host.c`; the option is declared at the root so the platform subdirectory sees it before target source selection. The Vita backend provides monotonic timestamps, append-only logging, milestone context, progress and float-fault counters, and explicit unknown results for process-memory sampling. It logs that crash handlers, stack unwinding and the background watchdog are not implemented; a watchdog request is never represented as running. The VitaSDK CI cross-compiles this backend, and a host mock regression checks timestamp conversion, counters, deduplication, milestone context and truthful reporting of unsupported capabilities.

This is a platform-layer milestone, not a full application build. The aggregate platform target and native runtime remain subject to separate compatibility checks.

## Vita host storage paths (2026-10)

With `ICO_INPUT_VITA=ON`, the executable target now defines `ICO_HOST_VITA`: the packaged/read-only program directory is `app0:`, while writable settings, logs, relative ISO paths, and default memory-card data live under `ux0:data/ico-pc`. Known Vita mount prefixes (`app0:`, `ux0:`, `uma0:`, `imc0:`, and `ur0:`) are recognized as absolute paths. Startup creates the writable data directory and fails early if it cannot be created; layered settings and save-path lookup avoid creating directories on demand. The native host regression covers these path rules and the read-only lookup behavior.

CI compiles `main_host.c`, `trace_host.c`, `host_config.c`, and `host_loop.c` in a VitaSDK headless configuration. This is only a compile-compatibility check: the existing desktop SDL event/window path is not a Vita display loop. The CMake-selected `ico_pc` target can now link in a headless/null-graphics VitaSDK configuration, but that executable is not a complete native Vita application: no Vita display loop or ICO RHI backend is linked.


## Latest verification and reference audit (2026-10-11)

- **Full headless executable link (CI run 38107731661, commit `172959289dd1c66b0eabd95cc241bfd9d912b1ee`):** all six workflow jobs passed. The CMake-selected `ico_pc` target linked successfully and `file` identified the result as an ELF32 ARM EABI5 statically linked executable. The workflow verified the generated link graph contains `libico_input.a`, `-lSceCtrl_stub`, and `-lSceAudio_stub`. It also verified the Vita enum compile flag is present in every CMake compile-database entry. This is stronger than the earlier compile-only source audit, but it is still a **headless/null-graphics executable link**, not a playable game build.
- **ABI warnings remain visible:** the successful link emitted 371 ARM linker enum-size metadata warnings. `readelf -A` reported `Tag_ABI_enum_size: int` for the representative `main_host.c` object and `Tag_ABI_enum_size: small` for the final executable. The workflow no longer incorrectly fails solely on those warnings, but their source and ABI implications are not yet fully resolved. Do not suppress these warnings without identifying the responsible inputs and proving enum values do not cross an incompatible ABI boundary.
- **Artifacts from run 38107731661:** `ico-project-owned-vita-gl-probe` (artifact ID 11689994595) and `vitaGL-immediate-mode-probe-vpk` (artifact ID 11690239362) are graphics feasibility packages; `vita-sdk-smoke-probe-172959289dd1c66b0eabd95cc241bfd9d912b1ee` (artifact ID 11689939951) is a minimal SDK packaging probe. None contains a playable ICO build, and no hardware/emulator rendering result has been verified.
- **RHI link-path finding:** `port/rhi/CMakeLists.txt` returns without creating the RHI dispatcher when neither Vulkan nor D3D12 is available. On the VitaSDK probe configuration, the successful `ico_pc` link uses `port/null/gfx_null.c` and does not link `ico_rhi`, `ico_render`, or `ico_shaders`. The current link therefore validates native game/platform code and device stubs, not the graphics stack. A Vita backend must be explicitly added to the RHI selection/build graph before the main renderer can use it.
- **THUG Vita reference audit:** `iwannagooutside/thug-vita-port` has a game-specific Vita renderer in `Code/Gfx/Vita/` and uses vitaGL calls for model rendering. Its implementation is useful for learning the practical GL state, texture, geometry, and performance work involved in a native Vita backend. It is not a drop-in replacement for ICO's explicit RHI or its HLSL shader model.
- **recvx-vita reference audit:** `Rinnegatamante/recvx-vita` implements a PS2 GS-oriented layer in `platform/gs/gs_draw.c`. Its design treats GS frame buffers as GL render targets, maintains a texture cache for GS memory, and handles uploads/readbacks for memory regions that overlap render targets. That is a valuable reference for PS2 graphics semantics, but ICO's current code is not a PS2 GS emulator; only transferable concepts should be reused.
- **PS2Recomp reference audit:** `ran-j/PS2Recomp` provides an experimental PS2 ELF-to-C++ path and a Vita runtime build script with a PVR/gl4es/SDL2/raylib dependency stack. Its own Vita script disables building the recompiler/analyzer for the Vita runtime build and warns when game registration code is absent. This is a research alternative, not evidence that ICO's original PS2 ELF can be recompiled or that its runtime supplies ICO's required renderer.
- **Next engineering gate:** define a Vita RHI backend selection path and document an operation/capability mapping against `RhiBackendTable`; separately classify HLSL features that need a Vita shader rewrite or a different implementation. Start with a minimal backend test that creates a context, clears a target, draws textured geometry, and presents a frame. Only then integrate the ICO renderer incrementally. A triangle or quad test proves only that specific graphics path, not ICO scene rendering.
