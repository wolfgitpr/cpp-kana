#!/usr/bin/env python3
"""Builds and runs the C# suite against a native library.

The steps, in order, because each one depends on the previous:

1. find the native library (``--native-dir``, or the one the Python shell staged -- there is only one
   correct answer and it is the file that was just built);
2. write ``BuildInfo.g.cs`` with that file's real name and the version from the root CMakeLists.txt;
3. ``dotnet run`` the test project with the library's directory passed in, so the assembly and the native
   file end up in the same output folder;
4. exit with the runner's own status.

    python csharp/tools/run_tests.py
    python csharp/tools/run_tests.py --native-dir .tmp/build-default/bin --framework net10.0
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "csharp" / "CppKana2.Tests" / "CppKana2.Tests.csproj"
LIBRARY_SUFFIXES = (".dll", ".so", ".dylib")
DEFAULT_FRAMEWORKS = ("net10.0", "net7.0")


def find_native(directory: Path) -> Path:
    candidates = [
        entry
        for entry in sorted(directory.iterdir())
        if entry.is_file() and entry.suffix in LIBRARY_SUFFIXES and "cpp-kana2" in entry.name
    ]
    if not candidates:
        raise SystemExit(f"no *cpp-kana2*.dll|.so|.dylib in {directory}")
    candidates.sort(key=lambda path: (len(path.name), path.name))
    return candidates[0]


def default_native_dir() -> Path:
    staged = ROOT / "python" / "cpp_kana2" / "_native"
    if staged.is_dir() and any(staged.iterdir()):
        return staged
    raise SystemExit(
        "no --native-dir given and the Python shell has not staged one either.\n"
        "Run: python python/tools/build_native.py   (or pass --native-dir <build>/bin)"
    )


def installed_frameworks() -> set[str]:
    result = subprocess.run(["dotnet", "--list-sdks"], capture_output=True, text=True)
    versions = set()
    for line in result.stdout.splitlines():
        version = line.split()[0] if line.split() else ""
        if version.startswith("10."):
            versions.add("net10.0")
        elif version.startswith("7."):
            versions.add("net7.0")
    return versions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--native-dir", type=Path)
    parser.add_argument("--framework", action="append", help="repeatable; defaults to every installed target")
    args = parser.parse_args()

    native = find_native(args.native_dir or default_native_dir())
    print(f"native: {native}")

    emit = [
        sys.executable,
        str(ROOT / "csharp" / "tools" / "emit_build_info.py"),
        "--native",
        str(native),
    ]
    print("+ " + " ".join(emit))
    subprocess.run(emit, check=True)

    frameworks = args.framework or [f for f in DEFAULT_FRAMEWORKS if f in installed_frameworks()]
    if not frameworks:
        raise SystemExit("none of net10.0/net7.0 is installed (dotnet --list-sdks)")

    native_dir = native.parent
    for framework in frameworks:
        command = [
            "dotnet",
            "run",
            "--project",
            str(PROJECT),
            "--framework",
            framework,
            "-c",
            "Release",
            f"-p:CppKana2NativeDir={native_dir}",
            "--nologo",
        ]
        print("+ " + " ".join(command))
        result = subprocess.run(command)
        if result.returncode != 0:
            print(f"C# suite failed on {framework}", file=sys.stderr)
            return result.returncode
    print(f"C# suite passed on {', '.join(frameworks)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
