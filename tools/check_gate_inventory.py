#!/usr/bin/env python3
"""Every gate must be run by something, and the runner must run every gate.

This is the gate about gates. It compares three sources and fails when any two disagree:

* the `GATES` registry in `tools/run_all_gates.py`;
* the scripts on disk (`tools/check_*.py`);
* the `CPP_KANA2_GATES` list in `tests/CMakeLists.txt`, which the ctest `foreach` turns into one test per
  gate (`add_test(NAME Gate_${gate} ...)`).

The failure it prevents is the quiet one: a new gate script nobody wires up looks like coverage in a
directory listing and runs in no CI job.

    python tools/check_gate_inventory.py
    python tools/check_gate_inventory.py --self-test
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def registered_gates(root: Path = ROOT) -> list[str]:
    text = (root / "tools" / "run_all_gates.py").read_text(encoding="utf-8")
    match = re.search(r"^GATES\s*=\s*\[(.*?)\]", text, re.S | re.M)
    if not match:
        raise SystemExit("tools/run_all_gates.py has no GATES list")
    return re.findall(r'"(\w+)"', match.group(1))


def script_gates(root: Path = ROOT) -> list[str]:
    return sorted(path.stem for path in (root / "tools").glob("check_*.py"))


def ctest_gates(root: Path = ROOT) -> tuple[list[str], bool]:
    """The gate names listed for ctest, and whether the file registers them with a loop."""
    text = (root / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"set\(CPP_KANA2_GATES(.*?)\)", text, re.S)
    names = re.findall(r"[a-z_]+[\w]*", match.group(1)) if match else []
    return names, "add_test(NAME Gate_${gate}" in text


# The gates that are not Python scripts: they are CMake scripts driven by ctest, so CPP_KANA2_GATES cannot
# see them. Naming them here is what keeps a deleted add_test from going unnoticed.
REQUIRED_CTEST = [
    "ConformanceFixture",
    "GateSelfTests",
    "Gate_ExportGate",
    "Gate_ExportGateSelfTest",
    "Gate_SonameGate",
    "Gate_InstallConsumer",
]


def check(root: Path = ROOT) -> list[str]:
    problems = []
    runner = registered_gates(root)
    scripts = script_gates(root)
    ctest, loops = ctest_gates(root)

    for name in scripts:
        if name not in runner:
            problems.append(f"{name}.py exists but is not in run_all_gates.GATES (nobody runs it)")
    for name in runner:
        if name not in scripts:
            problems.append(f"run_all_gates lists {name}, but tools/{name}.py does not exist")
        if name not in ctest:
            problems.append(f"{name} is not in tests/CMakeLists.txt's CPP_KANA2_GATES (ctest never runs it)")
    text = (root / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
    for name in REQUIRED_CTEST:
        if f"add_test(NAME {name}" not in text:
            problems.append(f"{name} is not registered by name in tests/CMakeLists.txt (the C-side gates are "
                            "not covered by CPP_KANA2_GATES)")
    if runner and not loops:
        problems.append("tests/CMakeLists.txt does not register the gates with add_test(NAME Gate_${gate} ...)")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "tools").mkdir()
        (root / "tests").mkdir()
        (root / "tools" / "run_all_gates.py").write_text('GATES = [\n    "check_one",\n]\n', encoding="utf-8")
        (root / "tools" / "check_one.py").write_text("", encoding="utf-8")
        (root / "tools" / "check_orphan.py").write_text("", encoding="utf-8")
        (root / "tests" / "CMakeLists.txt").write_text(
            "set(CPP_KANA2_GATES\n        check_one)\n"
            "add_test(NAME Gate_${gate} COMMAND x)\n", encoding="utf-8")

        found = " ".join(check(root))
        if "check_orphan" not in found:
            print(f"SELF-TEST FAIL: an unwired gate was not reported: {found!r}", file=sys.stderr)
            return 1
        if "check_one is not in tests" in found:
            print(f"SELF-TEST FAIL: a wired gate was reported as unregistered: {found!r}", file=sys.stderr)
            return 1

        (root / "tests" / "CMakeLists.txt").write_text(
            "set(CPP_KANA2_GATES\n)\nadd_test(NAME Gate_${gate} COMMAND x)\n", encoding="utf-8")
        found = " ".join(check(root))
        if "is not in tests/CMakeLists.txt" not in found:
            print(f"SELF-TEST FAIL: an unregistered gate was not reported: {found!r}", file=sys.stderr)
            return 1

        (root / "tests" / "CMakeLists.txt").write_text("set(CPP_KANA2_GATES\n        check_one)\n", encoding="utf-8")
        found = " ".join(check(root))
        if "does not register the gates" not in found:
            print(f"SELF-TEST FAIL: a missing add_test loop was not reported: {found!r}", file=sys.stderr)
            return 1
    print("gate inventory: self-test passed (unwired gate, unregistered gate, missing ctest loop)")
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
    ctest, _loops = ctest_gates()
    print(f"gate inventory: {len(registered_gates())} gates registered, present, and run by ctest "
          f"({len(ctest)} in CPP_KANA2_GATES)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
