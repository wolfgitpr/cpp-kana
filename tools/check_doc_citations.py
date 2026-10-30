#!/usr/bin/env python3
"""Documentation cites `file:line`; this gate checks those citations still point at something.

Every `[text](path#Lnn)` and bare `path:nn` in the docs is a promise that line nn of that file is the thing
being described. A cleanup pass that deletes a function or reformats a file silently makes those promises
false, and a reader who follows one lands somewhere unrelated and stops trusting the rest.

The check is deliberately shallow: the file must exist and the line must be within it. It does not verify
that the line SAYS what the sentence claims -- that needs a person, and pretending otherwise would be worse
than the honest boundary. What it catches is drift by deletion, which is the common case.

    python tools/check_doc_citations.py
    python tools/check_doc_citations.py --self-test
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# [text](../src/x.cpp#L12) / [text](../src/x.cpp#L12-L30)
MARKDOWN_LINK = re.compile(r"\]\(([^)\s#]+)#L(\d+)(?:-L(\d+))?\)")
# `src/x.cpp:12` inside prose, in backticks or not
BARE_CITATION = re.compile(r"\b([A-Za-z0-9_./-]+\.(?:h|cpp|c|cs|py|cmake|map|txt|json|tsv)):(\d+)\b")


def markdown_files(root: Path = ROOT) -> list[Path]:
    try:
        import subprocess

        listed = subprocess.run(["git", "-C", str(root), "ls-files", "--cached", "--others",
                                 "--exclude-standard", "*.md"],
                                capture_output=True, text=True, check=True).stdout
        files = [root / line for line in listed.splitlines() if line.strip()]
        if files:
            return files
    except Exception:  # noqa: BLE001 - a tree without git still has docs
        pass
    return sorted(path for path in root.rglob("*.md") if ".tmp" not in path.parts)


def check(root: Path = ROOT) -> list[str]:
    problems = []
    for document in markdown_files(root):
        if not document.is_file():
            continue
        text = document.read_text(encoding="utf-8", errors="replace")
        relative_document = document.relative_to(root).as_posix()
        citations: list[tuple[str, int]] = []
        for match in MARKDOWN_LINK.finditer(text):
            target, first, last = match.group(1), int(match.group(2)), match.group(3)
            citations.append((target, max(first, int(last) if last else first)))
        for match in BARE_CITATION.finditer(text):
            citations.append((match.group(1), int(match.group(2))))

        for target, line in citations:
            resolved = (document.parent / target).resolve()
            if not resolved.is_file():
                try:
                    resolved = (root / target).resolve()
                except OSError:
                    continue
            if not resolved.is_file():
                continue  # a missing file is check_doc_links.py's job, not this one's
            try:
                total = len(resolved.read_text(encoding="utf-8", errors="replace").splitlines())
            except OSError:
                continue
            if line > total:
                problems.append(f"{relative_document}: cites {target}:{line}, but that file has {total} lines")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "docs").mkdir()
        (root / "src").mkdir()
        source = root / "src" / "thing.cpp"
        source.write_text("line1\nline2\nline3\n", encoding="utf-8", newline="\n")
        document = root / "docs" / "note.md"
        document.write_text("# note\n\nSee [x](../src/thing.cpp#L3) and `src/thing.cpp:2`.\n",
                            encoding="utf-8", newline="\n")
        if check(root):
            print(f"SELF-TEST FAIL: a valid citation was reported: {check(root)}", file=sys.stderr)
            return 1

        document.write_text("# note\n\nSee [x](../src/thing.cpp#L9).\n", encoding="utf-8", newline="\n")
        found = " ".join(check(root))
        if "has 3 lines" not in found:
            print(f"SELF-TEST FAIL: an out-of-range citation was not reported: {found!r}", file=sys.stderr)
            return 1

        document.write_text("# note\n\nSee `src/thing.cpp:7`.\n", encoding="utf-8", newline="\n")
        if "has 3 lines" not in " ".join(check(root)):
            print("SELF-TEST FAIL: an out-of-range bare citation was not reported", file=sys.stderr)
            return 1
    print("doc citations: self-test passed (out-of-range link and bare citations are both reported)")
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
    print(f"doc citations: every file:line reference in {len(markdown_files())} documents is in range")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
