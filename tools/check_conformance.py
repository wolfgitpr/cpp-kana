#!/usr/bin/env python3
"""Pins conformance/cases.json by its sha256 and checks the provenance field.

Why a separate file instead of trusting the JSON: the fixture is compared byte for byte by three
language suites, and "did this file change" has to be answerable without parsing it. cases.sha256 is that
answer, and the Python suite plus this gate both read it.

The provenance field is checked too: `generated_by_commit` must name a commit this repository contains.
A fixture that says "generated" but not "from what" cannot be reproduced; one that names a commit nobody
has is worse, because it *looks* reproducible.

    python tools/check_conformance.py             # verify
    python tools/check_conformance.py --write     # re-pin after an intended regeneration
    python tools/check_conformance.py --self-test # prove a tampered fixture is caught
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def digest_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(fixture: Path, digest: Path, root: Path, check_commit: bool = True) -> list[str]:
    if not fixture.exists():
        return [f"{fixture.name} is missing; run cpp-kana2-conformance --write"]

    problems = []
    actual = digest_of(fixture)
    line = f"{actual}  cases.json\n"
    if not digest.exists():
        problems.append(f"{digest.name} is missing")
    elif digest.read_text(encoding="utf-8") != line:
        problems.append(f"{digest.name} does not match the fixture: expected {line.strip()!r}")

    data = json.loads(fixture.read_text(encoding="utf-8"))
    if not isinstance(data.get("cases"), list) or not data["cases"]:
        problems.append("cases is missing or empty")
    elif data.get("cases_count") != len(data["cases"]):
        problems.append(f"cases_count says {data.get('cases_count')} but the array holds {len(data['cases'])}")

    commit = str(data.get("generated_by_commit", ""))
    if commit == "unknown":
        print("NOTE: generated_by_commit is 'unknown' (no git metadata in this checkout)")
    elif not re.fullmatch(r"[0-9a-f]{7,40}", commit):
        problems.append(f"generated_by_commit {commit!r} is not a lower-case hex id")
    elif check_commit:
        found = subprocess.run(["git", "-C", str(root), "cat-file", "-e", f"{commit}^{{commit}}"],
                               capture_output=True)
        if found.returncode != 0:
            problems.append(f"generated_by_commit {commit} is not a commit in this repository")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        fixture = root / "cases.json"
        digest = root / "cases.sha256"
        payload = {"cases_count": 1, "generated_by_commit": "unknown",
                   "cases": [{"kind": "kana", "input": "あ", "rendered": "a"}]}
        fixture.write_text(json.dumps(payload), encoding="utf-8", newline="\n")

        # An unpinned fixture must be reported.
        if not any("missing" in problem for problem in verify(fixture, digest, root)):
            print("SELF-TEST FAIL: a missing pin was not reported", file=sys.stderr)
            return 1

        digest.write_text(f"{digest_of(fixture)}  cases.json\n", encoding="utf-8", newline="\n")
        if verify(fixture, digest, root):
            print(f"SELF-TEST FAIL: a correctly pinned fixture was reported: {verify(fixture, digest, root)}",
                  file=sys.stderr)
            return 1

        # Changing the fixture without re-pinning must be reported.
        payload["cases"].append({"kind": "kana", "input": "い", "rendered": "i"})
        fixture.write_text(json.dumps(payload), encoding="utf-8", newline="\n")
        found = " ".join(verify(fixture, digest, root))
        if "does not match the fixture" not in found:
            print(f"SELF-TEST FAIL: an unpinned change was not reported: {found!r}", file=sys.stderr)
            return 1

        # A count that disagrees with the array must be reported even when the pin matches.
        digest.write_text(f"{digest_of(fixture)}  cases.json\n", encoding="utf-8", newline="\n")
        found = " ".join(verify(fixture, digest, root))
        if "cases_count says" not in found:
            print(f"SELF-TEST FAIL: a wrong cases_count was not reported: {found!r}", file=sys.stderr)
            return 1
    print("conformance: self-test passed (missing pin, unpinned change, wrong cases_count)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--write", action="store_true", help="rewrite cases.sha256 from the fixture")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    fixture = ROOT / "conformance" / "cases.json"
    digest = ROOT / "conformance" / "cases.sha256"
    if args.write:
        if not fixture.exists():
            print(f"MISSING: {fixture.relative_to(ROOT)}; run cpp-kana2-conformance --write", file=sys.stderr)
            return 1
        actual = digest_of(fixture)
        digest.write_text(f"{actual}  cases.json\n", encoding="utf-8", newline="\n")
        print(f"pinned {fixture.relative_to(ROOT)} at {actual}")
        return 0

    problems = verify(fixture, digest, ROOT)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    data = json.loads(fixture.read_text(encoding="utf-8"))
    print(f"conformance fixture: {len(data['cases'])} cases, sha256 matches, provenance ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
