#!/usr/bin/env python3
"""The public surface is frozen in three lists, and the lists are the authority.

`tests/abi/expected_public_headers.txt`, `expected_c_api_symbols.txt` and
`expected_public_names.txt` are checked in. This gate compares them with the tree in BOTH directions: a
name added without touching the list is red, and a name in the list the headers no longer declare is red.
The point is not ceremony -- the Python and C# shells transcribe this surface, and a silent addition is a
binding that is quietly incomplete.

Freezing is a deliberate act; the diff is the review:

    python tools/check_public_surface.py --freeze

    python tools/check_public_surface.py
    python tools/check_public_surface.py --self-test
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ABI_DIR = ROOT / "tests" / "abi"

HEADERS_LIST = ABI_DIR / "expected_public_headers.txt"
SYMBOLS_LIST = ABI_DIR / "expected_c_api_symbols.txt"
NAMES_LIST = ABI_DIR / "expected_public_names.txt"

COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

# A type declared at namespace level: column 4 in these headers, class members are at column 8.
TYPE_DECLARATION = re.compile(
    r"^ {4}(?! )(?:template[ \t]*<[^>\n]*>[ \t]*)?"
    r"(?:class|struct|enum[ \t]+class|enum)[ \t]+(?:CPP_KANA2_EXPORT[ \t]+)?(\w+)",
    re.M,
)

# A function declaration, possibly split across lines. A continuation line is indented deeper than column 4;
# that guard is what stops the match from running into the next declaration.
FUNCTION_STATEMENT = re.compile(
    r"^ {4}(?! )(?:CPP_KANA2_(?:EXPORT|NODISCARD)[ \t]+)*"
    r"(?:[\w:<>, \t*&]|\n(?! {4}[^ ]))*?[ \t*&](\w+)[ \t]*\([^;]*\)[^;{}]*?;",
    re.M,
)

C_SYMBOL = re.compile(r"CPPK2_API[^\n]*?\b(cppk2_[a-z0-9_]+)[ \t]*\(", re.S)

# Names the extractor must keep seeing. Without this, an extractor regression would produce a shorter list
# that compares clean against itself -- the "empty scan is not a pass" rule, made specific.
REQUIRED_NAMES = {
    "Code", "Error", "ErrorException", "Outcome", "Mora", "MoraKind", "Conversion",
    "RomajiOptions", "KanaOptions", "Script", "kanaToRomaji", "romajiToKana", "convertScript",
    "utf8ToCodepoints", "codepointsToUtf8", "isValidUtf8", "codeText", "version",
}


def public_headers(root: Path = ROOT) -> list[str]:
    return sorted(path.relative_to(root).as_posix() for path in (root / "include").rglob("*.h"))


def names_in(text: str) -> set[str]:
    """Namespace-level names in one header (types and free functions)."""
    names = set(TYPE_DECLARATION.findall(text))
    names.update(match.group(1) for match in FUNCTION_STATEMENT.finditer(text))
    return names


def public_names() -> list[str]:
    names: set[str] = set()
    for header in public_headers():
        if header.endswith("ext/c_api.h"):
            continue  # a C header; its symbols are frozen separately
        names |= names_in(COMMENT.sub("", (ROOT / header).read_text(encoding="utf-8")))
    return sorted(name for name in names if name != "detail" and not name.isupper())


def c_symbols() -> list[str]:
    text = COMMENT.sub("", (ROOT / "include" / "cpp-kana2" / "ext" / "c_api.h").read_text(encoding="utf-8"))
    return sorted(set(C_SYMBOL.findall(text)))


def read_list(path: Path) -> list[str]:
    if not path.is_file():
        return []
    return [line.strip() for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.startswith("#")]


def compare(actual: list[str], expected_path: Path, what: str) -> list[str]:
    expected = read_list(expected_path)
    found = []
    if not expected:
        found.append(f"{what}: {expected_path.name} is empty or missing (an empty list freezes nothing)")
    added = sorted(set(actual) - set(expected))
    removed = sorted(set(expected) - set(actual))
    if added:
        found.append(f"{what}: {len(added)} declared but not frozen: {', '.join(added)}")
    if removed:
        found.append(f"{what}: {len(removed)} frozen but no longer declared: {', '.join(removed)}")
    return found


def check() -> list[str]:
    names = public_names()
    missing = sorted(REQUIRED_NAMES - set(names))
    found = []
    if missing:
        found.append("public names: the extractor no longer sees " + ", ".join(missing)
                     + " -- fix the extractor before trusting the list")
    return (
        found
        + compare(public_headers(), HEADERS_LIST, "public headers")
        + compare(c_symbols(), SYMBOLS_LIST, "C ABI symbols")
        + compare(names, NAMES_LIST, "public names")
    )


def freeze() -> None:
    ABI_DIR.mkdir(parents=True, exist_ok=True)
    HEADERS_LIST.write_text(
        "# The public headers, frozen. Edit in the same commit as the header that moves (K4/K5).\n"
        + "\n".join(public_headers()) + "\n", encoding="utf-8", newline="\n")
    SYMBOLS_LIST.write_text(
        "# The C ABI symbols, frozen. Every cppk2_* entry point the library exports.\n"
        + "\n".join(c_symbols()) + "\n", encoding="utf-8", newline="\n")
    NAMES_LIST.write_text(
        "# Namespace-level names of cpp_kana::v2 in the public headers (types and free functions).\n"
        "# Names containing detail/impl/internal are not part of the surface and are not listed here.\n"
        + "\n".join(public_names()) + "\n", encoding="utf-8", newline="\n")


def self_test() -> int:
    declarations = (
        "namespace x\n{\n"
        "    enum class Kind : int {\n"
        "        One = 0,\n"
        "    };\n"
        "\n"
        "    class Thing {\n"
        "        bool ok() const noexcept;\n"
        "    };\n"
        "\n"
        "    void declared(int value);\n"
        "\n"
        "    Outcome<Thing> splitAcrossLines(std::string_view first,\n"
        "                                    std::string_view second);\n"
        "}\n"
    )
    names = names_in(declarations)
    expected = {"Kind", "Thing", "declared", "splitAcrossLines"}
    if names != expected:
        print(f"SELF-TEST FAIL: extractor returned {sorted(names)}, expected {sorted(expected)}", file=sys.stderr)
        return 1
    if "ok" in names:
        print("SELF-TEST FAIL: a class member was reported as a namespace-level name", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as directory:
        expected_path = Path(directory) / "expected_public_names.txt"
        expected_path.write_text("Thing\nGone\n", encoding="utf-8")
        found = " ".join(compare(["Kind", "Thing", "declared"], expected_path, "public names"))
        if "Kind" not in found or "Gone" not in found:
            print(f"SELF-TEST FAIL: expected an added and a removed name, got {found!r}", file=sys.stderr)
            return 1
    print("public surface: self-test passed (types, multi-line declarations, members excluded, drift reported)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--freeze", action="store_true", help="rewrite the three frozen lists")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.freeze:
        freeze()
        print(f"froze {len(public_headers())} headers, {len(c_symbols())} C symbols, {len(public_names())} names")
        return 0
    if args.self_test:
        return self_test()

    found = check()
    if found:
        for problem in found:
            print(f"FAIL: {problem}", file=sys.stderr)
        print("if the change is intended, run: python tools/check_public_surface.py --freeze", file=sys.stderr)
        return 1
    print(f"public surface: {len(public_headers())} headers, {len(c_symbols())} C symbols, "
          f"{len(public_names())} names, all frozen")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
