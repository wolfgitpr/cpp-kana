#!/usr/bin/env python3
"""The wheel has to be checked as a FILE, not as a build that succeeded.

cpp-pinyin's lesson, which applies here unchanged: a wheel can build green and still be wrong in ways no test
sees, because the tests run against the staging directory while the user runs what is inside the archive. The
two failures that matter are (a) a wheel tagged `py3-none-any`, which pip installs on every platform and which
then fails at import everywhere but the platform it was built on, and (b) a wheel whose `_native/` directory is
empty, which installs cleanly and raises NativeLibraryError on first use.

    python tools/check_wheel.py                 # builds one from ./python and checks it
    python tools/check_wheel.py --wheel X.whl   # checks an existing archive
    python tools/check_wheel.py --self-test
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_RE = re.compile(r"^project\(cpp-kana2 VERSION ([0-9]+\.[0-9]+\.[0-9]+)", re.MULTILINE)


def expected_version() -> str:
    match = VERSION_RE.search((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not match:
        raise SystemExit("no `project(cpp-kana2 VERSION x.y.z)` in CMakeLists.txt")
    return match.group(1)


def build_wheel(destination: Path) -> Path:
    result = subprocess.run([sys.executable, "-m", "pip", "wheel", str(ROOT / "python"),
                             "--no-build-isolation", "--no-deps", "-w", str(destination)],
                            capture_output=True, text=True)
    wheels = sorted(destination.glob("cpp_kana2-*.whl"))
    if result.returncode != 0 or not wheels:
        raise SystemExit(f"wheel build failed:\n{result.stdout[-2000:]}\n{result.stderr[-2000:]}")
    return wheels[-1]


def check(wheel: Path) -> list[str]:
    problems: list[str] = []
    name = wheel.name

    # 1. The tag. `any` is the lie: it means "no native code", and this package is nothing but native code.
    tag = name.removesuffix(".whl").split("-")[-1]
    if tag == "any":
        problems.append(f"{name}: platform tag is 'any'; pip would install it on every OS and it can only work"
                        " on the one it was built for")
    if not tag.startswith("win_") and not tag.startswith("manylinux") and not tag.startswith("macosx"):
        problems.append(f"{name}: platform tag {tag!r} is not a platform this package is built for")
    if "-py3-none-" not in name:
        problems.append(f"{name}: expected a py3-none ABI tag (ctypes, no extension module), got {name}")

    # 2. The version, against the single authority.
    version = expected_version()
    if not name.startswith(f"cpp_kana2-{version}-"):
        problems.append(f"{name}: filename does not carry the version from CMakeLists.txt ({version})")

    with zipfile.ZipFile(wheel) as archive:
        entries = archive.namelist()

        # 3. The native library, by extension for the platform in the tag.
        family = tag.split("_")[0]
        suffix = {"win": ".dll", "manylinux": ".so", "macosx": ".dylib"}.get(family)
        candidates = [entry for entry in entries
                      if "/_native/" in entry and entry.endswith((".dll", ".so", ".dylib"))]
        wanted = [entry for entry in candidates if suffix is None or entry.endswith(suffix)]
        if not wanted:
            problems.append(f"{name}: no _native/*{suffix or '.dll/.so/.dylib'} inside;"
                            " the package would raise NativeLibraryError")
        for entry in candidates:
            if entry not in wanted:
                problems.append(f"{name}: {entry} is a foreign platform's library for a {tag} wheel")

        # 4. The licence, the metadata, and nothing that only exists on a build machine.
        metadata = [entry for entry in entries if entry.endswith(".dist-info/METADATA")]
        # PEP 639 moved the licence to dist-info/licenses/. Newer setuptools writes that layout, older ones wrote
        # dist-info/LICENSE; both are correct, and a check that only knows one of them fails on a runner whose
        # setuptools is newer than the machine the gate was written on. MEASURED: the first release run failed
        # exactly this way, on a wheel that did carry the licence.
        licence_entries = [entry for entry in entries
                           if entry.endswith((".dist-info/LICENSE", ".dist-info/licenses/LICENSE"))]
        if not licence_entries:
            problems.append(f"{name}: no LICENSE in dist-info (Apache-2.0 in METADATA is not the licence text)")
        if metadata:
            text = archive.read(metadata[0]).decode("utf-8", "replace")
            for field in ("Name: cpp-kana2", f"Version: {version}", "License: Apache-2.0"):
                if field not in text:
                    problems.append(f"{name}: METADATA is missing {field!r}")
        forbidden = [entry for entry in entries
                     if entry.endswith((".pyc", ".pdb", ".obj", ".egg-info"))
                     or "__pycache__" in entry or entry.startswith(("/", "D:", "C:"))]
        if forbidden:
            problems.append(f"{name}: build-machine leftovers inside: {forbidden[:3]}")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        fake = Path(directory) / "cpp_kana2-9.9.9-py3-none-any.whl"
        with zipfile.ZipFile(fake, "w") as archive:
            archive.writestr("cpp_kana2/__init__.py", "")
        found = " ".join(check(fake))
        for expected in ("platform tag is 'any'", f"no _native", "filename does not carry the version"):
            if expected not in found:
                print(f"SELF-TEST FAIL: {expected!r} not reported; got {found!r}", file=sys.stderr)
                return 1
        if "no LICENSE" not in found:
            print("SELF-TEST FAIL: a wheel without a licence was not reported", file=sys.stderr)
            return 1
    print("check_wheel: self-test passed (tag, native library, version and licence are all reported)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--wheel", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    if args.wheel:
        wheel = args.wheel
    else:
        # Without a wheel named, this gate builds one -- and building one requires the native library to have
        # been staged by python/tools/build_native.py. On a compiler-only machine (and in the ctest leg of CI,
        # which compiles the library but does not stage the Python package) there is nothing to stage from, and
        # a gate that fails for that reason would be reporting the environment, not the package. MEASURED: this
        # exact failure turned the first real CI run red with "metadata-generation-failed".
        staging = ROOT / "python" / "cpp_kana2" / "_native"
        if not staging.is_dir() or not any(staging.iterdir()):
            print("check_wheel: NOTE -- no staged native library (python/cpp_kana2/_native is empty); "
                  "nothing to package here, run python/tools/build_native.py to exercise this gate")
            return 0
        directory = ROOT / ".tmp" / "wheel-check"
        staging.mkdir(parents=True, exist_ok=True)
        for stale in staging.glob("*.whl"):
            stale.unlink()
        wheel = build_wheel(staging)

    problems = check(wheel)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print(f"check_wheel: {wheel.name} carries its native library, licence and version, and is not tagged 'any'")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
