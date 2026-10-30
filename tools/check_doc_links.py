#!/usr/bin/env python3
"""Every relative link in the tracked Markdown must resolve.

Documentation rots in a specific way: a file is renamed, and a dozen documents keep pointing at the old
name. Nothing fails, and readers quietly stop trusting the docs. This gate resolves every relative link and
image path in the Markdown files that are tracked, and reports the ones that do not exist.

External links (http/https) are not fetched -- a gate that needs the network is a gate that fails for the
wrong reason. Anchors (`#section`) are not resolved either: GitHub's slug rules are not worth re-implementing,
and a wrong anchor is a small defect next to a broken file path.

    python tools/check_doc_links.py
    python tools/check_doc_links.py --self-test
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# [text](target) and [text]: target, plus the image form ![alt](target). The target stops at whitespace or a
# title, and a target containing a space must be wrapped in <> -- both are CommonMark rules.
LINK = re.compile(r"!?\[[^\]]*\]\(\s*<?([^)\s>]+)>?[^)]*\)")
REFERENCE = re.compile(r"^\s*\[[^\]]+\]:\s*<?([^\s>]+)>?", re.M)
SKIP_PREFIXES = ("http://", "https://", "mailto:", "ftp://")


def markdown_files(root: Path = ROOT) -> list[Path]:
    """Every Markdown file git knows about, tracked or not-but-not-ignored.

    Tracked-only would under-scan exactly when it matters: a new document's links would be checked for the
    first time in CI, after the commit. Ignored files stay out, so a scratch note under .tmp/ is not the
    gate's business.
    """
    try:
        listed = subprocess.run(["git", "-C", str(root), "ls-files", "--cached", "--others",
                                 "--exclude-standard", "*.md"],
                                capture_output=True, text=True, check=True).stdout
        files = [root / line for line in listed.splitlines() if line.strip()]
        if files:
            return files
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return sorted(path for path in root.rglob("*.md") if ".tmp" not in path.parts)


def ignored_by_git(root: Path, path: Path) -> bool:
    """True when git ignores the path, i.e. it is a generated file a clean clone does not contain.

    MEASURED on the Linux board replay: two documents link to `python/cpp_kana2/_native/name.txt` and
    `csharp/CppKana2/version.props`, both generated and gitignored, so the gate failed on a perfectly
    consistent tree. A link to something the reader is told to generate is not a broken link; it is reported
    as a NOTE instead.
    """
    try:
        result = subprocess.run(["git", "-C", str(root), "check-ignore", "-q", str(path)],
                                capture_output=True)
    except FileNotFoundError:
        return False
    return result.returncode == 0


# Where generated files live. This exists because `git check-ignore` cannot answer in an exported tree -- a
# `git archive` checkout has no .git, so check-ignore exits 128 and every generated target would be reported as
# broken. MEASURED on the board replay: that is exactly what happened after the first version of this fix.
GENERATED_MARKERS = (
    "/_native/",
    "_native/name.txt",
    "/version.props",
    "/BuildInfo.g.cs",
    "/_version.py",
    "/.tmp/",
    "/dist/",
    "/bin/",
    "/obj/",
)


def is_generated(root: Path, path: Path) -> bool:
    try:
        relative = "/" + path.relative_to(root).as_posix()
    except ValueError:
        return False
    return any(marker in relative for marker in GENERATED_MARKERS)


def check(root: Path = ROOT) -> list[str]:
    problems = []
    for document in markdown_files(root):
        if not document.is_file():
            continue  # listed in the index but not checked out; nothing to read
        text = document.read_text(encoding="utf-8", errors="replace")
        relative_document = document.relative_to(root).as_posix()
        for target in LINK.findall(text) + REFERENCE.findall(text):
            if target.startswith(SKIP_PREFIXES) or target.startswith("#"):
                continue
            path_part = target.split("#", 1)[0]
            if not path_part:
                continue
            resolved = (document.parent / path_part).resolve()
            if resolved.exists():
                continue
            if ignored_by_git(root, resolved) or is_generated(root, resolved):
                print(f"NOTE: {relative_document} links to generated (gitignored) '{target}'; "
                      "not a broken link")
                continue
            problems.append(f"{relative_document}: '{target}' does not exist "
                            f"(resolved to {resolved})")
    return problems


def self_test() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "docs").mkdir()
        (root / "present.md").write_text("# here\n", encoding="utf-8")
        (root / "docs" / "good.md").write_text(
            "# good\n\n[present](../present.md) and [external](https://example.invalid/) and [anchor](#x)\n",
            encoding="utf-8")
        (root / "docs" / "bad.md").write_text("# bad\n\n[missing](../absent.md)\n", encoding="utf-8")

        problems = check(root)
        joined = " ".join(problems)
        if "absent.md" not in joined:
            print(f"SELF-TEST FAIL: a missing target was not reported: {problems}", file=sys.stderr)
            return 1
        if "present.md" in joined:
            print(f"SELF-TEST FAIL: an existing target was reported: {problems}", file=sys.stderr)
            return 1
    print("doc links: self-test passed (a missing relative target is reported, existing ones are not)")
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
    print(f"doc links: {len(markdown_files())} Markdown files, every relative target resolves")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
