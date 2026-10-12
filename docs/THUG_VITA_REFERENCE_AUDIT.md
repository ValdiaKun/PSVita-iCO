# THUG Vita Reference Audit

Date: 2026-10-12
Working branch: `vita/feasibility-audit`. Do not modify `main`.

## Decision

Keep https://github.com/iwannagooutside/thug-vita-port as a required technical reference. Do not copy its source files unless an applicable license or explicit permission authorizes that reuse. Independently implement ICO-compatible functionality where reuse is not authorized.

## Findings

- THUG Vita's README describes a native PS Vita port using vitaGL and VitaSDK/CMake.
- Its Vita CMake configuration includes platform/runtime code, links vitaGL and VitaSDK libraries, and applies selective per-file optimization based on measured hotspots.
- A request for the repository-root `LICENSE` file returned 404 through GitHub's contents API. The inspected README does not state a general license for THUG Vita's own source. This is not permission to copy; inspect file headers, upstream provenance (`kisak-thug`), submodules, and dependency licenses separately.
- vitaGL has its own licensing terms and must be evaluated independently if linked or distributed.

## ICO audit targets

1. Compare THUG's platform startup, logging, vitaGL initialization, resource lifecycle, shader path, input handling, packaging, and real-device test loop with ICO's `port/rhi`, `port/render`, `port/shaders`, and platform code.
2. Use those findings as engineering requirements; do not import THUG engine-specific source into ICO without authorization.
3. Implement an ICO-owned Vita RHI backend and native display/frame loop on this branch.
4. Add focused tests for texture formats, render targets, shader coverage, input mapping, and presentation where feasible.
5. Build and test an ICO VPK on actual hardware before claiming a native-rendering milestone. A headless link or placeholder VPK is not a playable build.
6. Profile before optimizing; do not blindly copy THUG compiler flags because ICO's code and ABI constraints differ.

## Reuse decision matrix

| Component | Current decision |
|---|---|
| THUG-specific source | No direct copying until license/permission is confirmed |
| Architecture and observable behavior | Technical reference; implement compatible ICO-side equivalents |
| vitaGL | Evaluate as a separate dependency and meet its applicable terms |
| VitaSDK/CMake packaging ideas | Recreate in ICO's own build system and verify |
| Upstream `kisak-thug` code | Trace provenance and license before any reuse |

## Acceptance criteria

Record provenance and license status for every reused component. Native-rendering milestone requires the ICO-owned Vita backend to initialize on device and present a verified frame. A playable-port claim requires booting ICO game code with user-supplied game data and testing controls/gameplay.

## References

- THUG Vita: https://github.com/iwannagooutside/thug-vita-port
- vitaGL: https://github.com/Rinnegatamante/vitaGL
- recvx-vita: https://github.com/Rinnegatamante/recvx-vita
- GitHub licensing guidance: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
