#!/usr/bin/env python3
"""Cross-compile the host game's C translation units without linking the game.

This deliberately reuses cmake/IcoSources.cmake and the host build's source
semantics. It is a compile audit only: it does not prove that platform services
or the renderer link or run on Vita.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


PROGRAMMERS = ("common", "fumi", "ito", "omori", "script", "seki", "sugipon")
INCLUDE_ORDER = ("sugipon", "omori", "common", "ito", "fumi", "seki", "script")
SEMANTIC_FLAGS = (
    "-std=gnu11",
    "-fno-strict-aliasing",
    "-fwrapv",
    "-ffp-contract=off",
    "-fno-fast-math",
    "-fsigned-char",
    "-fno-common",
    "-fgnu89-inline",
)
WARNING_FLAGS = (
    "-Wreturn-type",
    "-Wimplicit-function-declaration",
    "-Wstrict-prototypes",
    "-Wno-error=implicit-function-declaration",
    "-Wno-error=implicit-int",
    "-Wno-error=int-conversion",
    "-Wno-error=incompatible-pointer-types",
    "-Wno-error=return-mismatch",
    "-Wno-error=declaration-missing-parameter-type",
)


def source_lists(path: Path) -> dict[str, list[str]]:
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"set\((ICO_SOURCES_[A-Za-z0-9_]+|ICO_EE_ONLY_SOURCES)[ \t]*([\s\S]*?)\n\)"
    )
    result: dict[str, list[str]] = {}
    for match in pattern.finditer(text):
        name, body = match.groups()
        result[name] = [
            token for line in body.splitlines()
            if (token := line.strip()) and not token.startswith("#")
        ]
    missing = [f"ICO_SOURCES_{name}" for name in PROGRAMMERS if f"ICO_SOURCES_{name}" not in result]
    if missing:
        raise ValueError(f"missing source-list sections: {', '.join(missing)}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--cc", required=True, help="Vita ARM C compiler")
    parser.add_argument("--readelf", required=True, help="Vita ARM readelf for validating every object")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    outdir = args.output_dir.resolve()
    outdir.mkdir(parents=True, exist_ok=True)
    lists = source_lists(root / "cmake" / "IcoSources.cmake")
    sources = [s for prog in PROGRAMMERS for s in lists[f"ICO_SOURCES_{prog}"]]
    ee_only = lists.get("ICO_EE_ONLY_SOURCES", [])

    # Cross-check the parsed lists against the canonical link-order inventory.
    # This catches parser/list drift before a partial audit can look complete.
    inventory = []
    for line in (root / "config" / "link_order.pal.txt").read_text(encoding="utf-8").splitlines():
        fields = line.split("#", 1)[0].strip().split()
        if fields and fields[0].startswith("ico2/") and fields[0].endswith(".c"):
            inventory.append(fields[0])
    expected_game = [path for path in inventory if not path.startswith("ico2/ito/mpeg/")]
    expected_ee_only = [path for path in inventory if path.startswith("ico2/ito/mpeg/")]
    if len(sources) != len(set(sources)) or set(sources) != set(expected_game):
        missing = sorted(set(expected_game) - set(sources))
        extra = sorted(set(sources) - set(expected_game))
        raise ValueError(f"source-list mismatch: missing={missing}, extra={extra}")
    if set(ee_only) != set(expected_ee_only):
        raise ValueError("ICO_EE_ONLY_SOURCES does not match the canonical link-order inventory")
    generated_check = subprocess.run(
        [sys.executable, str(root / "tools" / "gen_sources.py"), "--check"],
        cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
    )
    if generated_check.returncode:
        raise ValueError("generated source lists are stale: " + generated_check.stdout.strip())
    missing_files = [s for s in sources if not (root / s).is_file()]
    if missing_files:
        raise FileNotFoundError("source list contains missing files: " + ", ".join(missing_files))

    # Probe warning names first: GCC only warns for unknown -Wno-error= flags
    # when another diagnostic is emitted, so don't assume every host GCC flag
    # exists in every VitaSDK toolchain release.
    help_result = subprocess.run(
        [args.cc, "-Q", "--help=warnings"], text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    known_warning_text = help_result.stdout
    optional_flags = []
    for flag in WARNING_FLAGS[3:]:
        warning_name = flag.removeprefix("-Wno-error=")
        if re.search(r"^\s+-W" + re.escape(warning_name) + r"\s", known_warning_text, re.MULTILINE):
            optional_flags.append(flag)
    common_flags = [
        *SEMANTIC_FLAGS, *WARNING_FLAGS[:3], *optional_flags,
        "-DICO_HOST=1", "-DICO_HEADLESS=1",
    ]

    results = []
    for index, rel in enumerate(sources, 1):
        source = root / rel
        prog = rel.split("/")[1]
        include_dirs = [prog, *INCLUDE_ORDER]
        unique_dirs = []
        for item in include_dirs:
            if item not in unique_dirs:
                unique_dirs.append(item)
        cmd = [args.cc, *common_flags]
        for item in unique_dirs:
            include = root / "ico2" / item / "include"
            if include.is_dir():
                cmd.extend(["-I", str(include)])
        cmd.extend(["-I", str(root / "port" / "compat")])
        # Mirror include paths added to these object libraries by port/game
        # and port/ui; those target-specific paths are not in ico_include_dirs.
        if prog in ("common", "fumi", "script", "sugipon"):
            cmd.extend(["-I", str(root / "port" / "include")])
        if prog == "common":
            cmd.extend(["-I", str(root / "port" / "ui")])
        cmd.append("-O2")
        cmd.append(f"-fmacro-prefix-map={root}/ico2/{prog}/=")
        if rel == "ico2/common/src/main.c":
            cmd.append("-Dmain=ico_game_main")
        obj = outdir / f"{index:03d}_{source.stem}.o"
        cmd.extend(["-c", str(source), "-o", str(obj)])
        run = subprocess.run(cmd, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, check=False)
        compile_ok = run.returncode == 0
        architecture_output = ""
        architecture_ok = False
        if compile_ok:
            header = subprocess.run(
                [args.readelf, "-h", str(obj)], text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, check=False,
            )
            architecture_output = header.stdout
            architecture_ok = (
                header.returncode == 0
                and re.search(r"^\\s*Class:\\s+ELF32\\s*$", header.stdout, re.MULTILINE) is not None
                and re.search(r"^\\s*Machine:\\s+ARM\\s*$", header.stdout, re.MULTILINE) is not None
            )
            if not architecture_ok and header.returncode == 0:
                architecture_output += "\\nExpected an ELF32 object for ARM."
        ok = compile_ok and architecture_ok
        failure_output = run.stdout if not compile_ok else architecture_output
        results.append({
            "source": rel,
            "ok": ok,
            "compile_returncode": run.returncode,
            "architecture_ok": architecture_ok,
            "output": "" if ok else failure_output[-6000:],
        })
        print(f"[{index:03d}/{len(sources):03d}] {'OK' if ok else 'FAIL'} {rel}", flush=True)
        if not ok:
            print(failure_output[-3000:], file=sys.stderr, flush=True)

    report = {
        "compiler": args.cc,
        "readelf": args.readelf,
        "object_validation": "ELF32/ARM required for every successful translation unit",
        "source_count": len(sources),
        "ee_only_excluded_count": len(ee_only),
        "passed": sum(item["ok"] for item in results),
        "failed": sum(not item["ok"] for item in results),
        "semantic_flags": list(SEMANTIC_FLAGS),
        "warning_flags": [*WARNING_FLAGS[:3], *optional_flags],
        "results": results,
        "scope": "C translation-unit compile audit only; no game link or runtime validation",
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Vita source audit: {report['passed']}/{report['source_count']} passed; "
          f"{report['failed']} failed; report={args.report}", flush=True)
    return 1 if report["failed"] else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as exc:
        print(f"vita_source_audit: {exc}", file=sys.stderr)
        sys.exit(2)
