# Vita renderer decision: vitaGL

Status: **selected backend direction; ICO RHI integration not yet implemented**.

The PS Vita renderer for this port will use [vitaGL](https://github.com/Rinnegatamante/vitaGL), not Vulkan or D3D12. The existing desktop backends remain unchanged. The selected dependency is pinned to commit `dca4b9d143290d78ec043131a19be36c78cdc7b5` for reproducible feasibility builds.

## What is already verified

The `vitagl-renderer-probe` CI job builds and installs the pinned `libvitaGL.a`, builds the project's `port/render/vita/vita_gl_probe.c`, and packages a VPK that exercises context initialization, a native Cg shader path, a small RGBA texture, depth testing, and alpha blending. CI proves compile/link/package success only. The probe has not been verified on Vita hardware or a compatible emulator, and requires the device's `libshacccg.suprx` runtime shader compiler.

The current `ico_pc` Vita configuration is still headless and links `port/null/gfx_null.c`; it does not use vitaGL and does not render ICO. Do not describe the current executable or probe VPK as a working ICO port.

## Integration boundary

Keep the existing `port/render/rd_*` renderer and its GS-semantic behavior. Implement a Vita-specific backend under `port/rhi/vitagl/` and register it through the existing `RhiBackendTable` dispatcher. Do not fork the game renderer or replace the desktop Vulkan/D3D12 paths.

The backend needs to cover the RHI operations in `port/rhi/rhi_backend.h`. Before it can be selected by the application, it must have a real Vita display/context lifecycle, backbuffer acquisition/presentation, buffers and textures, shader/program and pipeline management, binding state, render passes, copies/readback where required, depth/stencil, and deterministic resource cleanup. Unsupported operations must fail explicitly or use a tested fallback; they must not silently succeed.

## Shader strategy

Do not feed the existing SPIR-V or DXIL blobs to vitaGL. Add a Vita-native shader route using vitaGL's supported shader path (including native Cg extensions where appropriate), then migrate and validate shader families incrementally. Runtime Cg compilation depends on `libshacccg.suprx`; cache compiled GXP programs in writable Vita storage to avoid repeated startup compilation.

The first shader slices should be the clear/present path and a simple textured sprite. After that, tackle the VU mesh/skinning shaders, texture sampling and fog, then post-processing. Keep shader sources and generated output specific to the Vita path so the desktop shader pipeline remains unchanged.

## Known semantic blockers

- `SV_VertexID`-generated geometry needs explicit attributes or equivalent Vita shader logic.
- `Texture2D<uint4>`, integer `.Load()`, and `RGBA8_UINT` exact-blend targets have no assumed direct mapping. Preserve the GS integer blend semantics with a tested implementation or a clearly documented fallback.
- `SV_Depth`, reversed-Z, depth sampling/copies, stencil wrap, and target transitions need capability tests.
- Dual-source output has an existing alternate path, but the Vita backend must verify which variant is usable.
- HLSL `precise` arithmetic must preserve intended operation order; compare against existing CPU/reference tests.
- Vita memory and fill-rate are constrained; render-target count, dimensions, intermediate copies, and shader cache memory need explicit budgets.

## Required gates

1. **Probe runtime:** run the project-owned probe on hardware or a compatible emulator; capture device/firmware details, logs, and visible output. CI packaging alone is not enough.
2. **Backend skeleton:** add a Vita-only CMake selection that links vitaGL and the Vita display lifecycle, without pulling SDL/Vulkan/D3D12 into the Vita target.
3. **Minimal RHI slice:** create a context, clear/present, draw a textured quad, and destroy/recreate the display safely. Add a VPK smoke test.
4. **Shader slice:** compile/cache Vita-native sprite shaders and render a texture through the actual ICO RHI path.
5. **Capability tests:** test depth, stencil, blend, copies, texture formats, and resource lifetime before enabling the matching ICO render passes.
6. **Incremental scene integration:** bring up UI/sprites, then opaque meshes, skinning, shadows and post-processing. Report each verified slice separately.
7. **Game validation:** only claim ICO scene rendering or gameplay after a real game VPK has been run and observed.

## Guardrails

- Work only on `vita/feasibility-audit`; never modify or merge to `main` without explicit authorization.
- Keep desktop and Android builds unchanged.
- Do not commit game disc images, extracted assets, proprietary data, compiled VPKs/ELFs, credentials, or shader cache binaries.
- Keep CI/runtime evidence distinct: source compile, backend link, VPK packaging, probe runtime, ICO scene rendering, and gameplay are separate milestones.
