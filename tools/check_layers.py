#!/usr/bin/env python3
"""Layers may only depend downwards, and every file must belong to a layer.

The table below is the authority. Two failure modes it is designed against, both of which happened in the
sibling project this one is modelled on:

* a layer that stops being scanned (files moved to a new directory) while the gate keeps saying OK -- hence
  "a file under src/ that is in no layer is an error", not "unknown files are skipped";
* a rule that is written as prose in a document and enforced nowhere -- hence this file, and the --self-test
  that proves each kind of violation is reported.

Order: infra < data < mora < core < c_api. Public headers (<cpp-kana2/...>) are the installed surface and may
be included from anywhere; `../src/`-style escapes out of a layer are not allowed from public headers.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAYERS = {
    "infra": [],
    "data": ["infra"],
    "mora": ["data", "infra"],
    "core": ["mora", "data", "infra"],
    "c_api": ["core", "mora", "data", "infra"],
}
PUBLIC_PREFIX = "cpp-kana2/"
INCLUDE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.M)


def layer_of(relative: str) -> str | None:
    parts = relative.split("/")
    if len(parts) >= 3 and parts[0] == "src":
        return parts[1] if parts[1] in LAYERS else None
    return None


def check(root: Path) -> list[str]:
    problems = []
    for path in sorted((root / "src").rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        layer = layer_of(relative)
        if layer is None:
            problems.append(f"{relative}: not in any layer (add it to a layer in tools/check_layers.py)")
            continue
        for include in INCLUDE.findall(path.read_text(encoding="utf-8")):
            if include.startswith(PUBLIC_PREFIX) or "/" not in include:
                continue  # public header or standard/system header
            target = include
            if target.startswith("../"):
                # "../data/Tables.h" from src/mora/ resolves to src/data/Tables.h
                base = path.parent.relative_to(root).as_posix()
                parts = base.split("/")
                for segment in target.split("/"):
                    if segment == "..":
                        parts = parts[:-1]
                    else:
                        parts.append(segment)
                target = "/".join(parts)
            dependency = layer_of(target)
            if dependency is None or dependency == layer:
                continue
            if dependency not in LAYERS[layer]:
                problems.append(f"{relative}: {layer} may not include {dependency} ({include})")
    return problems


def strip_public_escapes(root: Path) -> list[str]:
    problems = []
    for path in sorted((root / "include").rglob("*.h")):
        relative = path.relative_to(root).as_posix()
        for include in INCLUDE.findall(path.read_text(encoding="utf-8")):
            if include.startswith("../"):
                problems.append(f"{relative}: a public header must not include {include} (it is installed alone)")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    found = check(ROOT) + strip_public_escapes(ROOT)
    if found:
        for problem in found:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print(f"layers: {len(LAYERS)} layers, no upward include, every src file classified")
    return 0


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "src" / "infra").mkdir(parents=True)
        (root / "src" / "data").mkdir()
        (root / "include").mkdir()

        # A downward edge is fine.
        (root / "src" / "data" / "ok.cpp").write_text('#include "../infra/thing.h"\n', encoding="utf-8")
        if check(root):
            print(f"SELF-TEST FAIL: a legal downward include was reported: {check(root)}", file=sys.stderr)
            return 1

        # An upward edge must be reported.
        (root / "src" / "infra" / "bad.cpp").write_text('#include "../data/thing.h"\n', encoding="utf-8")
        if not any("infra may not include data" in problem for problem in check(root)):
            print("SELF-TEST FAIL: an upward include was not reported", file=sys.stderr)
            return 1
        (root / "src" / "infra" / "bad.cpp").unlink()

        # A file in no layer must be reported (this is the "layer silently stopped being scanned" case).
        (root / "src" / "stray.cpp").write_text("// nobody's\n", encoding="utf-8")
        if not any("not in any layer" in problem for problem in check(root)):
            print("SELF-TEST FAIL: an unclassified file was not reported", file=sys.stderr)
            return 1
        (root / "src" / "stray.cpp").unlink()

        # A public header escaping into src/ must be reported.
        (root / "include" / "bad.h").write_text('#include "../src/infra/thing.h"\n', encoding="utf-8")
        if not strip_public_escapes(root):
            print("SELF-TEST FAIL: a public header including ../src was not reported", file=sys.stderr)
            return 1
    print("layers: self-test passed (upward edge, unclassified file, public-header escape)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
