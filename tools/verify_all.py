#!/usr/bin/env python3
"""Runs every verification leg this machine CAN run, and says which ones it cannot.

The point is the second half. A report that says "all tests pass" without naming what was not tested is how a
project convinces itself it is cross-platform when it is cross-compiled-on-one-machine. So this script prints
two tables: the legs it ran (with their real numbers) and the legs it could not run, each with the reason.

    python tools/verify_all.py                 # everything this machine can do (~2-3 minutes)
    python tools/verify_all.py --skip-clang    # when no Clang is installed
    python tools/verify_all.py --quick         # skip the reconfigure/built-from-scratch legs

The legs, and why each one is separate:

  mingw-shared   the primary development loop (GCC, shared library, the whole ctest suite)
  mingw-static   the archive a consumer links into its own shared object (PIC, no export table to measure)
  clang          a second compiler: MEASURED on this machine, Clang needs "--target=x86_64-w64-windows-gnu
                 -idirafter <sysroot>/include" to build against the MinGW runtime, and building with it proved
                 the install-consumer gate was not forwarding CMAKE_CXX_FLAGS
  python         the ctypes binding: stage the native library, then pytest
  csharp         the P/Invoke binding on every installed target framework
  gates          the ten gates, each preceded by its own --self-test
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def mingw_sysroot() -> Path | None:
    """Where the MinGW x86_64-w64-mingw32 headers live, for the Clang leg.

    Nothing here may be an absolute path from one machine: CONTRIBUTING.md tells contributors to run this
    script, so a path belonging to the author would silently turn the Clang leg into "skipped" everywhere else.
    Order: an explicit CPPKANA2_MINGW_SYSROOT, then whatever the Clang on PATH reports as its own system
    include directory for the windows-gnu target -- Clang is the one that has to consume the path, so asking it
    is the answer that cannot be wrong.
    """
    override = os.environ.get("CPPKANA2_MINGW_SYSROOT")
    if override:
        candidate = Path(override)
        return candidate if (candidate / "include").is_dir() else None

    compiler = shutil.which("clang++")
    if compiler is None:
        return None
    try:
        probe = subprocess.run([compiler, "--target=x86_64-w64-windows-gnu", "-v", "-E", "-x", "c++", os.devnull],
                               capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    for line in probe.stderr.splitlines():
        stripped = line.strip()
        if "x86_64-w64-mingw32" in stripped and stripped.endswith(os.sep + "include"):
            candidate = Path(stripped).parent
            return candidate if (candidate / "include").is_dir() else None
        if "x86_64-w64-mingw32" in stripped and stripped.rstrip("/\\").endswith("include"):
            candidate = Path(stripped.rstrip("/\\")).parent
            return candidate if (candidate / "include").is_dir() else None
    return None

def run(command: list[str], cwd: Path = ROOT) -> tuple[int, str]:
    started = time.monotonic()
    result = subprocess.run(command, cwd=str(cwd), capture_output=True, text=True)
    output = (result.stdout + result.stderr).strip().splitlines()
    tail = output[-1] if output else ""
    return result.returncode, f"{time.monotonic() - started:5.1f}s  {tail}"


def have(program: str) -> bool:
    return shutil.which(program) is not None


def configure(build: Path, extra: list[str]) -> int:
    command = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=Release", "-DCPP_KANA2_BUILD_TESTS=ON", "-DCPP_KANA2_INSTALL=ON", *extra]
    return subprocess.run(command, capture_output=True, text=True).returncode


def leg_cmake(name: str, build: Path, extra: list[str], rows: list) -> None:
    if build.exists():
        shutil.rmtree(build)
    status = configure(build, extra)
    if status != 0:
        rows.append((name, "FAIL", "configure failed"))
        return
    status, tail = run(["cmake", "--build", str(build)])
    if status != 0:
        rows.append((name, "FAIL", f"build: {tail}"))
        return
    status, tail = run(["ctest", "--test-dir", str(build)])
    rows.append((name, "PASS" if status == 0 else "FAIL", tail))


def native_rid() -> str:
    """The vcpkg/NuGet runtime identifier for this machine's Windows build."""
    return "win-x64" if sys.maxsize > 2**32 else "win-x86"


def staged_native_dir() -> str:
    """Where the Python staging step put the library -- the same file the C# suite links against."""
    return str(ROOT / "python" / "cpp_kana2" / "_native")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--skip-clang", action="store_true")
    parser.add_argument("--quick", action="store_true")
    args = parser.parse_args()

    rows: list[tuple[str, str, str]] = []
    skipped: list[tuple[str, str]] = []

    if args.quick:
        for preset in ("default", "static"):
            status, tail = run(["ctest", "--preset", preset])
            rows.append((f"mingw-{preset.split('-')[-1]}", "PASS" if status == 0 else "FAIL", tail))
    else:
        leg_cmake("mingw-shared", ROOT / ".tmp" / "verify-shared",
                  ["-DBUILD_SHARED_LIBS=ON"], rows)
        leg_cmake("mingw-static", ROOT / ".tmp" / "verify-static",
                  ["-DBUILD_SHARED_LIBS=OFF"], rows)

        if args.skip_clang or not have("clang++"):
            skipped.append(("clang", "no clang++ on PATH"))
        elif mingw_sysroot() is None:
            skipped.append(("clang", "no MinGW sysroot found (set CPPKANA2_MINGW_SYSROOT or put g++ on PATH)"))
        else:
            MINGW = mingw_sysroot()
            flags = f"--target=x86_64-w64-windows-gnu -idirafter {MINGW.as_posix()}/include"
            leg_cmake("clang", ROOT / ".tmp" / "verify-clang",
                      ["-DBUILD_SHARED_LIBS=ON",
                       "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
                       f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}",
                       f"-DCMAKE_EXE_LINKER_FLAGS=--target=x86_64-w64-windows-gnu -L{MINGW.as_posix()}/lib",
                       f"-DCMAKE_SHARED_LINKER_FLAGS=--target=x86_64-w64-windows-gnu -L{MINGW.as_posix()}/lib"],
                      rows)

    status, tail = run([sys.executable, "python/tools/build_native.py"])
    if status == 0:
        status, tail = run([sys.executable, "-m", "pytest", "python/tests", "-q"])
    rows.append(("python", "PASS" if status == 0 else "FAIL", tail))

    status, tail = run([sys.executable, "csharp/tools/run_tests.py"])
    rows.append(("csharp", "PASS" if status == 0 else "FAIL", tail))

    # Packaging is a leg of its own: both packages can build while being unusable, and each failure mode is
    # distinctive -- a wheel tagged py3-none-any (installs everywhere, loads nowhere) and a NuGet package whose
    # runtimes/<rid>/native/ directory is empty (restores fine, fails at the first P/Invoke).
    dist = ROOT / ".tmp" / "dist"
    if dist.exists():
        shutil.rmtree(dist)
    status, tail = run([sys.executable, "-m", "pip", "wheel", "./python", "--no-build-isolation",
                        "--no-deps", "-w", str(dist)])
    detail = tail
    if status == 0:
        wheels = list(dist.glob("*.whl"))
        if not wheels:
            status, detail = 1, "no wheel produced"
        elif wheels[0].name.endswith("-py3-none-any.whl"):
            status, detail = 1, f"{wheels[0].name} is tagged py3-none-any, which installs everywhere and loads nowhere"
        else:
            import zipfile

            names = zipfile.ZipFile(wheels[0]).namelist()
            native = [name for name in names if "_native" in name and name.endswith((".dll", ".so", ".dylib"))]
            if not native:
                status, detail = 1, f"{wheels[0].name} contains no native library"
            else:
                detail = f"{wheels[0].name} with {native[0].split('/')[-1]}"
    rows.append(("wheel", "PASS" if status == 0 else "FAIL", detail))

    if have("dotnet"):
        nupkg = ROOT / ".tmp" / "nupkg"
        if nupkg.exists():
            shutil.rmtree(nupkg)
        status, tail = run(["dotnet", "pack", "csharp/CppKana2/CppKana2.csproj", "-c", "Release",
                            f"-p:CppKana2Rid={native_rid()}", f"-p:CppKana2NativeDir={staged_native_dir()}",
                            "-o", str(nupkg)])
        detail = tail
        if status == 0:
            packages = list(nupkg.glob("*.nupkg"))
            import zipfile

            entries = zipfile.ZipFile(packages[0]).namelist() if packages else []
            native = [entry for entry in entries if entry.startswith("runtimes/") and "/native/" in entry]
            if not packages:
                status, detail = 1, "no .nupkg produced"
            elif not native:
                status, detail = 1, f"{packages[0].name} has no runtimes/<rid>/native/ entry"
            else:
                detail = f"{packages[0].name} with {native[0].split('/')[-1]}"
        rows.append(("nupkg", "PASS" if status == 0 else "FAIL", detail))
    else:
        skipped.append(("nupkg", "no dotnet on PATH"))

    status, tail = run([sys.executable, "tools/run_all_gates.py"])
    rows.append(("gates", "PASS" if status == 0 else "FAIL", tail))

    # The legs this machine cannot run. Naming them is the point of the script.
    skipped.append(("msvc", "no cl.exe / vcvars on this machine (Visual Studio has no C++ toolset installed)"))
    skipped.append(("macos", "no macOS host"))
    skipped.append(("linux", "verified by the firefly board replay (docs/BOARD_REPLAY.md), not here"))

    print(f"{'leg':<14} {'result':<6} detail")
    for name, result, detail in rows:
        print(f"{name:<14} {result:<6} {detail}")
    print()
    print("not run on this machine:")
    for name, reason in skipped:
        print(f"  {name:<12} {reason}")

    failures = [name for name, result, _detail in rows if result != "PASS"]
    print()
    print(f"{len(rows) - len(failures)}/{len(rows)} legs green" + ("" if not failures else f"; failed: {', '.join(failures)}"))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
