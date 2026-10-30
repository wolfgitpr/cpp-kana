#!/usr/bin/env python3
"""The mora table is derived, so the gate runs the derivation and then audits the result.

Two independent things can go wrong and they need different checks:

* the checked-in `src/data/TableData.inc` drifts from `data/moras.tsv` -- that is what `gen_moras.py
  --check` exists for, and this gate runs it rather than re-implementing it;
* `data/moras.tsv` itself is wrong -- a duplicate kana, an ambiguous alias, a row whose `secondary` flag
  lies about sharing its romaji, a stray carriage return. Those are properties of the data, not of the
  generation, and they are checked here.

One rule needs spelling out, because it looks like a defect and is not: two kana may share a canonical
romaji (ぢ/じ both spell "ji", うぉ/を both spell "wo"), but only when the later row sets `secondary` = 1 --
that flag is what tells the reverse direction which spelling to return.

    python tools/check_data_tables.py
    python tools/check_data_tables.py --self-test
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TSV = ROOT / "data" / "moras.tsv"
GENERATOR = ROOT / "tools" / "gen_moras.py"
COLUMNS = 5


def read_rows(path: Path) -> list[list[str]]:
    """Data rows only: the file's header is a comment block, so `#` lines are not data."""
    text = path.read_bytes().decode("utf-8")
    lines = [line for line in text.split("\n") if line.strip() and not line.lstrip().startswith("#")]
    return [line.split("\t") for line in lines]


def audit(path: Path = TSV) -> list[str]:
    problems = []
    seen_kana: dict[str, int] = {}
    canonical: dict[str, str] = {}
    aliases: dict[str, str] = {}
    for index, row in enumerate(read_rows(path), start=1):
        if len(row) != COLUMNS:
            problems.append(f"{path.name}: data row {index} has {len(row)} columns, expected {COLUMNS}")
            continue
        kana, romaji, alias_field, kind, secondary = row
        if not kana:
            problems.append(f"{path.name}: data row {index} has no kana")
        if kana in seen_kana:
            problems.append(f"{path.name}: data row {index}: kana {kana!r} already appears in data row {seen_kana[kana]}")
        seen_kana[kana] = index
        if not kind.isdigit():
            problems.append(f"{path.name}: data row {index}: kind {kind!r} is not a number")
        if secondary not in {"0", "1"}:
            problems.append(f"{path.name}: data row {index}: secondary {secondary!r} is not 0 or 1")

        if romaji in canonical:
            if secondary != "1":
                problems.append(f"{path.name}: data row {index}: romaji {romaji!r} is already the canonical "
                                f"spelling of {canonical[romaji]!r} and this row does not declare itself secondary")
        else:
            canonical[romaji] = kana
            if secondary == "1":
                problems.append(f"{path.name}: data row {index}: {kana!r} is marked secondary but no earlier "
                                f"row spells {romaji!r}")

        for alias in [entry for entry in alias_field.split(",") if entry]:
            if alias in aliases:
                problems.append(f"{path.name}: data row {index}: alias {alias!r} already belongs to {aliases[alias]!r}")
            aliases[alias] = kana
            if alias in canonical and canonical[alias] != kana:
                problems.append(f"{path.name}: data row {index}: alias {alias!r} of {kana!r} is also the canonical "
                                f"spelling of {canonical[alias]!r}; the reverse direction cannot decide")

    if path.read_bytes().count(b"\r") != 0:
        problems.append(f"{path.name}: contains CRLF line endings (the table is pinned to LF)")
    return problems


def generator_check() -> list[str]:
    if not GENERATOR.is_file():
        return [f"{GENERATOR} is missing"]
    result = subprocess.run([sys.executable, str(GENERATOR), "--check"], capture_output=True, text=True)
    if result.returncode != 0:
        return [f"gen_moras.py --check failed:\n{result.stdout}{result.stderr}".strip()]
    return []


def self_test() -> int:
    header = "# a comment header\n"
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "moras.tsv"

        path.write_text(header + "あ\ta\t\t0\t0\nい\ti\t\t0\t0\n", encoding="utf-8", newline="\n")
        if audit(path):
            print(f"SELF-TEST FAIL: a clean table was reported: {audit(path)}", file=sys.stderr)
            return 1

        path.write_text(header + "じ\tji\t\t0\t0\nぢ\tji\tdji\t0\t1\n", encoding="utf-8", newline="\n")
        if audit(path):
            print(f"SELF-TEST FAIL: a declared secondary sharing a romaji was reported: {audit(path)}", file=sys.stderr)
            return 1

        path.write_text(header + "じ\tji\t\t0\t0\nぢ\tji\tdji\t0\t0\n", encoding="utf-8", newline="\n")
        if not any("does not declare itself secondary" in problem for problem in audit(path)):
            print("SELF-TEST FAIL: an undeclared duplicate romaji was not reported", file=sys.stderr)
            return 1

        path.write_text(header + "あ\ta\t\t0\t0\nあ\ti\t\t0\t0\n", encoding="utf-8", newline="\n")
        if not any("already appears" in problem for problem in audit(path)):
            print("SELF-TEST FAIL: a duplicate kana was not reported", file=sys.stderr)
            return 1

        path.write_text(header + "あ\ta\t\t0\t0\nい\ti\ta\t0\t0\n", encoding="utf-8", newline="\n")
        if not any("is also the canonical spelling" in problem for problem in audit(path)):
            print("SELF-TEST FAIL: an ambiguous alias was not reported", file=sys.stderr)
            return 1

        path.write_bytes("# header\r\nあ\ta\t\t0\t0\r\n".encode("utf-8"))
        if not any("CRLF" in problem for problem in audit(path)):
            print("SELF-TEST FAIL: CRLF was not reported", file=sys.stderr)
            return 1
    print("data tables: self-test passed (secondary romaji, duplicate kana, ambiguous alias, CRLF)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()

    problems = audit() + generator_check()
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print(f"data tables: {len(read_rows(TSV))} rows, no duplicate kana, no ambiguous alias, generator in sync")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
