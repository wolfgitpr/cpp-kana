#!/usr/bin/env python3
"""One version, projected everywhere, and this gate is what makes "everywhere" true.

The authority is `project(cpp-kana2 VERSION x.y.z)` in the root CMakeLists.txt. Everything else is a
projection of it: the C++ macros in `include/cpp-kana2/core/Version.h`, the Python `_version.py` and the
C# `version.props` that the build scripts generate, and the CHANGELOG's top heading.

The generated files are checked only when they exist: a fresh clone has never run the binding builds, and
"missing" is not drift. A projection that exists and disagrees is drift, and that is what fails here.

    python tools/check_version_tokens.py
    python tools/check_version_tokens.py --self-test
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "CMakeLists.txt"
VERSION_HEADER = ROOT / "include" / "cpp-kana2" / "core" / "Version.h"
PYTHON_VERSION = ROOT / "python" / "cpp_kana2" / "_version.py"
SHARP_VERSION = ROOT / "csharp" / "CppKana2" / "version.props"
CHANGELOG = ROOT / "CHANGELOG.md"

PROJECT_SIZE = 64


def cmake_version(root: Path = ROOT) -> str:
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(\s*cpp-kana2\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    if not match:
        raise SystemExit("CMakeLists.txt has no project(cpp-kana2 VERSION x.y.z)")
    return match.group(1)


def header_version(path: Path = VERSION_HEADER) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    values = {}
    for key in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(r"#define\s+CPP_KANA2_VERSION_" + key + r"\s+(\d+)", text)
        if match:
            values[key.lower()] = match.group(1)
    match = re.search(r'#define\s+CPP_KANA2_VERSION_STRING\s+"([^"]+)"', text)
    if match:
        values["string"] = match.group(1)
    return values


def check(root: Path = ROOT) -> list[str]:
    expected = cmake_version(root)
    major, minor, patch = expected.split(".")
    problems = []

    values = header_version(root / "include" / "cpp-kana2" / "core" / "Version.h")
    for key, want in (("major", major), ("minor", minor), ("patch", patch), ("string", expected)):
        if key not in values:
            problems.append(f"Version.h: CPP_KANA2_VERSION_{key.upper()} is missing")
        elif values[key] != want:
            problems.append(f"Version.h: CPP_KANA2_VERSION_{key.upper()} is {values[key]}, CMakeLists says {want}")

    python_file = root / "python" / "cpp_kana2" / "_version.py"
    if python_file.is_file():
        match = re.search(r'__version__\s*=\s*"([^"]+)"', python_file.read_text(encoding="utf-8"))
        if not match:
            problems.append("python/cpp_kana2/_version.py: no __version__ assignment")
        elif match.group(1) != expected:
            problems.append(f"python/cpp_kana2/_version.py: {match.group(1)} != {expected} (regenerate it)")

    sharp_file = root / "csharp" / "CppKana2" / "version.props"
    if sharp_file.is_file():
        match = re.search(r"<Version>([^<]+)</Version>", sharp_file.read_text(encoding="utf-8"))
        if not match:
            problems.append("csharp/CppKana2/version.props: no <Version> element")
        elif match.group(1) != expected:
            problems.append(f"csharp/CppKana2/version.props: {match.group(1)} != {expected} (regenerate it)")

    changelog = root / "CHANGELOG.md"
    if changelog.is_file():
        text = changelog.read_text(encoding="utf-8")
        headings = re.findall(r"^##\s*\[?([0-9]+\.[0-9]+\.[0-9]+)", text, re.M)
        if headings and headings[0] != expected:
            problems.append(f"CHANGELOG.md: top entry is {headings[0]}, CMakeLists says {expected}")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "CMakeLists.txt").write_text("project(cpp-kana2 VERSION 3.1.4 LANGUAGES CXX)\n", encoding="utf-8")
        if cmake_version(root) != "3.1.4":
            print("SELF-TEST FAIL: the project() version was not read", file=sys.stderr)
            return 1

        header = root / "include" / "cpp-kana2" / "core"
        header.mkdir(parents=True)
        (header / "Version.h").write_text(
            "#define CPP_KANA2_VERSION_MAJOR 3\n#define CPP_KANA2_VERSION_MINOR 1\n"
            "#define CPP_KANA2_VERSION_PATCH 4\n#define CPP_KANA2_VERSION_STRING \"3.1.4\"\n", encoding="utf-8")
        if check(root):
            print(f"SELF-TEST FAIL: a consistent projection was reported: {check(root)}", file=sys.stderr)
            return 1

        (header / "Version.h").write_text(
            "#define CPP_KANA2_VERSION_MAJOR 3\n#define CPP_KANA2_VERSION_MINOR 0\n"
            "#define CPP_KANA2_VERSION_PATCH 0\n#define CPP_KANA2_VERSION_STRING \"3.0.0\"\n", encoding="utf-8")
        found = " ".join(check(root))
        if "CPP_KANA2_VERSION_STRING" not in found or "CPP_KANA2_VERSION_MINOR" not in found:
            print(f"SELF-TEST FAIL: drift was not reported: {found!r}", file=sys.stderr)
            return 1
    print("version tokens: self-test passed (a stale projection is reported)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    problems = check()
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print(f"version tokens: {cmake_version()} agrees with Version.h and every generated projection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
