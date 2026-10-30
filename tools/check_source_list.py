#!/usr/bin/env python3
"""The CMake source list must describe the tree, in both directions.

Why this exists: CMakeLists.txt lists its sources explicitly (no GLOB), which is the right default -- a glob
decides the build's content from whatever happens to be on disk. The cost of that choice is that a file can
be added and not listed (it silently does not build) or deleted and still listed (the build fails somewhere
unrelated). This gate pays that cost once per run.

    python tools/check_source_list.py             # check this tree
    python tools/check_source_list.py --self-test # prove the check can fail
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LISTED_VARIABLES = ("CPP_KANA2_SOURCES", "CPP_KANA2_HEADERS")
TREE_DIRECTORIES = ("src", "include")


def parse_cmake_lists(cmake_text: str) -> dict[str, list[str]]:
    lists: dict[str, list[str]] = {}
    for variable in LISTED_VARIABLES:
        match = re.search(r"set\(" + variable + r"\s+(.*?)\)\s*\n", cmake_text, re.S)
        if not match:
            raise SystemExit(f"{variable} not found in CMakeLists.txt")
        lists[variable] = [line.strip() for line in match.group(1).splitlines() if line.strip()]
    return lists


def tree_files(root: Path) -> set[str]:
    found = set()
    for directory in TREE_DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file():
                found.add(path.relative_to(root).as_posix())
    return found


def problems(root: Path) -> list[str]:
    cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    listed = parse_cmake_lists(cmake_text)
    flat_listed = {entry for entries in listed.values() for entry in entries}
    on_disk = tree_files(root)

    found = []
    if re.search(r"file\s*\(\s*GLOB", cmake_text):
        found.append("CMakeLists.txt uses file(GLOB ...); the source set must be explicit")
    for variable, entries in listed.items():
        missing = sorted(entry for entry in entries if entry not in on_disk)
        if missing:
            found.append(f"{variable} names {len(missing)} file(s) that do not exist: {', '.join(missing)}")
    unlisted = sorted(on_disk - flat_listed)
    if unlisted:
        found.append(f"{len(unlisted)} file(s) under src/ or include/ are not in any CMake list: "
                     + ", ".join(unlisted))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true", help="run against synthetic fixtures")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    found = problems(ROOT)
    if found:
        for problem in found:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print(f"source list: CMake and the tree agree ({len(tree_files(ROOT))} files)")
    return 0


def self_test() -> int:
    """One fixture that must fail and one that must pass -- a gate nobody has seen fail is not a gate."""
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "src").mkdir()
        (root / "src" / "kept.cpp").write_text("// kept\n", encoding="utf-8")
        (root / "src" / "forgotten.cpp").write_text("// forgotten\n", encoding="utf-8")
        (root / "CMakeLists.txt").write_text(
            "set(CPP_KANA2_SOURCES\n    src/kept.cpp\n)\nset(CPP_KANA2_HEADERS\n)\n", encoding="utf-8")
        found = problems(root)
        if not found or "forgotten.cpp" not in " ".join(found):
            print("SELF-TEST FAIL: an unlisted source file was not reported", file=sys.stderr)
            return 1

        (root / "CMakeLists.txt").write_text(
            "set(CPP_KANA2_SOURCES\n    src/kept.cpp\n    src/forgotten.cpp\n)\nset(CPP_KANA2_HEADERS\n)\n",
            encoding="utf-8")
        if problems(root):
            print(f"SELF-TEST FAIL: a consistent tree was reported: {problems(root)}", file=sys.stderr)
            return 1

        (root / "CMakeLists.txt").write_text(
            "file(GLOB SOURCES src/*.cpp)\nset(CPP_KANA2_SOURCES\n)\nset(CPP_KANA2_HEADERS\n)\n", encoding="utf-8")
        if not any("GLOB" in problem for problem in problems(root)):
            print("SELF-TEST FAIL: file(GLOB ...) was not reported", file=sys.stderr)
            return 1
    print("source list: self-test passed (unlisted file, orphan entry, and GLOB are all reported)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
