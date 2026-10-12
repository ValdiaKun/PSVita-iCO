# THUG Vita Reference Audit

Date: 2026-10-12
Working branch: `vita/feasibility-audit`. Do not modify `main`.

## Decision

Keep https://github.com/iwannagooutside/thug-vita-port a required technical reference. Do not copy its source files unless an applicable license or explicit permission authorizes that reuse. Independently implement ICO-compatible functionality where reuse is not authorized.

## Findings

- THUG Vita's README describes a native PS Vita port using vitaGL and VitaSDK/CMake.
- Its Vita CMake configuration includes platform/runtime code, links vitaGL and VitaSDK libraries, and applies selective per-file optimization based on measured hotspots.
- A request for the repository-root `LICENSE` file returned 404 through GitHub's contents API. The inspected README does not state a general license for THUG Vita's own source. This is not permission to copy; inspect file headers, upstream provenance (`kisak-thug`), submodules, and dependency licenses separately.
- vitaGL has its own licensing terms and must be evaluated independently if linked or distributed.

## ICO-specific compatibility findings

Inspection of the ICO branch's `port/rhi/rhi_backend.h`, `port/rhi/rhi_backend.c`, `port/rhi/CMakeLists.txt`, `port/render/rd.h`, `port/shaders/CMakeLists.txt`, and `cmake/IcoShaders.cmake` narrows the work:

- The RHI is already designed for multiple backends. A backend registers an `RhiBackendTable`; the dispatcher currently compiles Vulkan and optionally D3D12 only. An ICO-owned Vita backend should follow this table/dispatch contract rather than bypassing the RHI.
- The existing renderer is not a thin wrapper around a window API. It replays 13 ordered GS-compatible command lists, maintains state leakage between lists, and uses explicit resource barriers. Backend correctness must preserve order and PS2-specific sampling/state semantics.
- Shader build currently embeds DXC-produced SPIR-V and DXIL with no runtime compiler. This cannot be passed directly to vitaGL as-is. A Vita shader path is required, with a per-shader capability audit and behavior-preserving translation or dedicated shader sources.
- The shader inventory includes generated-vertex (`SV_VertexID`) paths, integer `RGBA8_UINT` blend targets and integer loads, depth output (`SV_Depth`), and VU microprogram translations whose floating-point operation order matters. These are concrete compatibility blockers, not merely build-system chores.
- RHI formats include integer textures, R8/R16 formats, RGBA16F, depth/stencil, and block-compressed texture formats. Each must be supported, emulated, or rejected/fallbacked explicitly; do not silently map formats with different semantics.
- The Vita's 32-bit ARM target is not the project's existing Android ARM64 target. Platform services and ABI/compile settings need a Vita-specific target and tests.

## Reference areas to inspect for design guidance

- Vita application entry point, runtime setup, logging, and shutdown.
- vitaGL context and graphics initialization.
- GPU resource lifecycle, textures, framebuffers, and state management.
- Shader compilation and runtime requirements.
- Vita controller/touch input and platform services.
- VitaSDK CMake configuration and VPK packaging.
- Real-device profiling, log capture, screenshot comparison, and regression loops.
- Selective per-file optimization backed by measurements rather than global flags.

These are architectural and engineering patterns to investigate, not evidence that THUG's engine-specific code can be dropped into ICO.

## Reuse decision matrix

| Component | Current decision |
|---|---|
| THUG-specific source | No direct copying until license/permission is confirmed |
| Architecture and observable behavior | Technical reference; implement compatible ICO-side equivalents |
| vitaGL | Evaluate as a separate dependency and meet its applicable terms |
| VitaSDK/CMake packaging ideas | Recreate in ICO's own build system and verify |
| Upstream `kisak-thug` code | Trace provenance and license before any reuse |

## Implementation plan

1. Keep all work on `vita/feasibility-audit`; never commit directly to `main`.
2. Add a Vita backend to the RHI dispatcher and build system without enabling desktop Vulkan/D3D12 on Vita.
3. Implement the native display/frame loop and a minimal clear/present test before connecting ICO's full renderer.
4. Build a shader coverage table: each HLSL entry point, current semantics, Vita equivalent, and test/reference image. Start with the simplest blit/sprite paths; separately plan integer blending and VU shaders.
5. Add focused tests for resource lifetime, texture formats, framebuffer/render-target behavior, shader coverage, input mapping, and present/swap behavior wherever testable without hardware.
6. Build a native VPK and validate it on hardware before claiming a native-rendering milestone. Record device logs, screenshots, and failures.
7. Profile before optimizing. Use measured hotspots and regression checks; do not copy THUG's compiler flags blindly because ICO's engine and ABI constraints differ.

## Acceptance criteria

Record provenance and license status for every reused component. Native-rendering milestone requires the ICO-owned Vita backend to initialize on device and present a verified frame. A playable-port claim requires booting ICO's own game code with user-supplied game data and verifying controls and gameplay; a headless link or placeholder VPK is insufficient.

## References

- THUG Vita: https://github.com/iwannagooutside/thug-vita-port
- vitaGL: https://github.com/Rinnegatamante/vitaGL
- recvx-vita: https://github.com/Rinnegatamante/recvx-vita
- GitHub licensing guidance: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
