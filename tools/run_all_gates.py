#!/usr/bin/env python3
"""Runs every gate, and proves each one can still fail before trusting it.

A gate that only ever prints OK is worse than no gate: it costs maintenance and buys false confidence. So
every check here has a `--self-test` that drives a synthetic failure through it, and this runner runs the
self-test **before** the real check. If a self-test fails, the run stops -- there is no point reading the
verdict of a checker that cannot detect the thing it exists to detect.

    python tools/run_all_gates.py              # every gate, self-test then real run
    python tools/run_all_gates.py --no-self-test
    python tools/run_all_gates.py --only check_layers,check_bindings
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"

# The registry. check_gate_inventory.py reads this list, so a gate added here and not registered in ctest
# (or the reverse) is red. Order matters only for reading the output: cheap gates first.
GATES = [
    "check_source_list",
    "check_layers",
    "check_public_surface",
    "check_bindings",
    "check_version_tokens",
    "check_data_tables",
    "check_conformance",
    "check_wheel",
    "check_doc_links",
    "check_doc_citations",
    "check_gate_inventory",
]


def run(script: Path, *arguments: str) -> tuple[int, str]:
    started = time.monotonic()
    result = subprocess.run([sys.executable, str(script), *arguments], capture_output=True, text=True)
    elapsed = time.monotonic() - started
    output = (result.stdout + result.stderr).strip()
    return result.returncode, f"{elapsed:5.2f}s  {output}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-self-test", action="store_true", help="skip the self-tests (faster, weaker)")
    parser.add_argument("--only", default="", help="comma separated gate names")
    args = parser.parse_args()

    wanted = [name.strip() for name in args.only.split(",") if name.strip()] or GATES
    unknown = [name for name in wanted if name not in GATES]
    if unknown:
        print(f"unknown gate(s): {', '.join(unknown)}; known: {', '.join(GATES)}", file=sys.stderr)
        return 2

    failures = 0
    for name in wanted:
        script = TOOLS / f"{name}.py"
        if not script.is_file():
            print(f"FAIL {name}: {script} does not exist", file=sys.stderr)
            failures += 1
            continue

        if not args.no_self_test:
            code, output = run(script, "--self-test")
            print(f"self {'PASS' if code == 0 else 'FAIL'}  {name}  {output}")
            if code != 0:
                failures += 1
                print(f"stopping: {name} cannot detect its own failure case", file=sys.stderr)
                break

        code, output = run(script)
        print(f"     {'PASS' if code == 0 else 'FAIL'}  {name}  {output}")
        if code != 0:
            failures += 1

    print(f"\n{len(wanted) - failures}/{len(wanted)} gates green" if failures == 0
          else f"\n{failures} gate(s) failed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
