#!/usr/bin/env python3
"""Inventory HLSL features that need an explicit PS Vita shader implementation.

This is a compatibility audit, not a shader translator and not proof that a
feature is unsupported by every possible Vita implementation. The required
checks preserve a list of known incompatibilities that a future Vita shader
backend must address or deliberately implement.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

SHADER_DIR = Path(__file__).resolve().parents[1]
EXPECTED_FEATURES = {
    "vertex_id": ("blit.hlsl", "SV_VertexID"),
    "fragment_depth": ("blit.hlsl", "SV_Depth"),
    "integer_texture": ("blend_int.hlsl", "Texture2D<uint4>"),
    "integer_load": ("blend_int.hlsl", ".Load("),
    "integer_target": ("blend_int.hlsl", "uint4 blend_int_ps"),
    "precise_math": ("vu_skin.hlsl", "precise"),
    "dual_source_output": ("common.hlsli", "SV_Target1"),
}
FEATURE_PATTERNS = {
    "vertex_id": r"\bSV_VertexID\b",
    "fragment_depth": r"\bSV_Depth\b",
    "integer_texture": r"\bTexture\w*\s*<\s*(?:u?int\d*|uint\d+)\s*>",
    "integer_load": r"\.Load\s*\(",
    "integer_target": r"\buint\d*\s+\w+\s*\([^)]*\)\s*:\s*SV_Target\d*",
    "precise_math": r"\bprecise\b",
    "dual_source_output": r"\bSV_Target1\b",
    "storage_buffer": r"\b(?:StructuredBuffer|ByteAddressBuffer|RWStructuredBuffer)\s*<",
    "storage_texture": r"\bRWTexture\w*\s*<",
    "explicit_lod": r"\bSampleLevel\s*\(",
    "derivatives": r"\b(?:ddx|ddy|fwidth)\s*\(",
    "discard": r"\bdiscard\b",
    "integer_ops": r"\b(?:uint|int)\d*\b|\b(?:asuint|asint)\s*\(",
    "texture_sample": r"\.(?:Sample|SampleBias|SampleGrad|SampleCmp)\s*\(",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json-out", type=Path, help="write the feature inventory as JSON")
    args = parser.parse_args()

    shaders = sorted([*SHADER_DIR.glob("*.hlsl"), *SHADER_DIR.glob("*.hlsli")])
    if not shaders:
        raise SystemExit(f"no HLSL files found under {SHADER_DIR}")

    inventory = {}
    source_text = {}
    for path in shaders:
        text = path.read_text(encoding="utf-8")
        source_text[path.name] = text
        inventory[path.name] = sorted(
            feature for feature, pattern in FEATURE_PATTERNS.items()
            if re.search(pattern, text)
        )

    # Keep the per-entry-point inventory aligned with the actual DXC build
    # registration list, including the explicitly declared no-dual-source
    # variants. This makes the conversion workload auditable, not just a list
    # of HLSL files.
    cmake_text = (SHADER_DIR / "CMakeLists.txt").read_text(encoding="utf-8")
    entry_points = []
    loop_values = []
    for line in cmake_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("foreach(") and "_e " in stripped:
            loop_values = stripped.split("(", 1)[1].rstrip(")").split()[1:]
            continue
        if stripped.startswith("endforeach("):
            loop_values = []
            continue
        if not stripped.startswith("ico_add_shader("):
            continue
        args_text = stripped.split("(", 1)[1].split(")", 1)[0]
        tokens = args_text.split()
        if len(tokens) < 4:
            raise SystemExit(f"cannot parse shader registration: {stripped}")
        name, source, entry, stage = tokens[:4]
        suffix = tokens[tokens.index("SUFFIX") + 1] if "SUFFIX" in tokens else ""
        defines = []
        if "DEFINES" in tokens:
            end = tokens.index("SUFFIX") if "SUFFIX" in tokens else len(tokens)
            defines = tokens[tokens.index("DEFINES") + 1:end]
        if name == "${_e}" and entry == "${_e}":
            if not loop_values:
                raise SystemExit(f"shader variant has no parsed foreach values: {stripped}")
            for value in loop_values:
                entry_points.append({
                    "name": value + suffix,
                    "source": source,
                    "entry": value,
                    "stage": stage,
                    "defines": defines,
                    "features": inventory[source],
                })
        else:
            entry_points.append({
                "name": name + suffix,
                "source": source,
                "entry": entry,
                "stage": stage,
                "defines": defines,
                "features": inventory.get(source, []),
            })

    missing = []
    for feature, (filename, token) in EXPECTED_FEATURES.items():
        if filename not in source_text or token.lower() not in source_text[filename].lower():
            missing.append(f"{filename} no longer contains expected {feature} marker {token!r}")
        elif feature not in inventory[filename]:
            missing.append(f"{filename} contains {token!r}, but detector missed {feature}")

    if missing:
        raise SystemExit("Vita shader audit regression(s):\n- " + "\n- ".join(missing))

    expected_entries = {"blend_int_ps", "vu_skin_vs", "yuv_field_ps", "sprite_ps_nodual", "fog_lut_ps_nodual"}
    registered_names = {entry["name"] for entry in entry_points}
    missing_entries = sorted(expected_entries - registered_names)
    if missing_entries:
        raise SystemExit("Vita shader registration audit missing expected entries: " + ", ".join(missing_entries))
    if len(entry_points) < 50:
        raise SystemExit(f"unexpectedly small shader entry-point inventory: {len(entry_points)}")

    report = {
        "schema_version": 1,
        "purpose": "inventory-only; not proof of Vita shader compatibility",
        "shader_count": len(inventory),
        "entry_point_count": len(entry_points),
        "feature_patterns": FEATURE_PATTERNS,
        "shaders": inventory,
        "entry_points": entry_points,
    }
    counts = {
        feature: sum(feature in features for features in inventory.values())
        for feature in FEATURE_PATTERNS
    }
    report["feature_file_counts"] = counts

    print(f"Vita HLSL compatibility inventory: {len(inventory)} shader/include files, {len(entry_points)} registered entry points")
    for filename, features in inventory.items():
        print(f"  {filename}: {', '.join(features) if features else '(no matched features)'}")
    print("Feature counts:")
    for feature, count in counts.items():
        if count:
            print(f"  {feature}: {count} file(s)")
    print("Known blocker markers detected: " + ", ".join(EXPECTED_FEATURES))
    print("Entry-point registration checks passed: " + ", ".join(sorted(expected_entries)))
    print("NOTE: this is a static inventory; it does not compile or validate Vita shaders.")

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"JSON inventory written to {args.json_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
