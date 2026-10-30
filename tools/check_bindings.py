#!/usr/bin/env python3
"""The bindings must transcribe the C ABI, not approximate it.

The C++ side has compile-time checks (`tests/c_api_smoke.c` is compiled as C against the header). A binding
has none: a missing entry point is a runtime AttributeError, and a reordered structure is silently wrong
data. So the transcription is compared here, mechanically:

* the Python `_ffi.C_API_FUNCTIONS` list and the C# `NativeMethods` extern declarations must equal
  `tests/abi/expected_c_api_symbols.txt`;
* every structure in `c_api.h` must appear in both bindings with the same number of fields in the same
  order (names compared case- and underscore-insensitively, because the two languages spell them
  differently on purpose: `letter_case` in C, `LetterCase` in C#).

    python tools/check_bindings.py
    python tools/check_bindings.py --self-test
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
STRUCT = re.compile(r"typedef\s+struct\s+(\w+)\s*\{(.*?)\}\s*\1\s*;", re.S)
CS_FIELD = re.compile(r"^\s*internal\s+(?:static\s+extern\s+)?[\w<>\[\]]+\s+(cppk2_\w+)\s*\(", re.M)
CS_STRUCT_FIELD = re.compile(r"^\s*public\s+[\w.<>\[\]]+\s+(\w+);", re.M)
PY_STRUCT = re.compile(r"class (\w+)\(ctypes\.Structure\):(.*?)\n\n", re.S)

# The five structures in c_api.h, and the Python/C# class that transcribes each one.
STRUCT_CLASSES = {
    "cppk2_mora": ("MoraStruct", "MoraNative"),
    "cppk2_conversion": ("ConversionStruct", "ConversionNative"),
    "cppk2_error": ("ErrorStruct", "ErrorNative"),
    "cppk2_romaji_options": ("RomajiOptionsStruct", "RomajiOptionsNative"),
    "cppk2_kana_options": ("KanaOptionsStruct", "KanaOptionsNative"),
}


def norm(name: str) -> str:
    return name.replace("_", "").lower()


def header_structs(header: Path) -> dict[str, list[str]]:
    text = COMMENT.sub("", header.read_text(encoding="utf-8"))
    structs = {}
    for name, body in STRUCT.findall(text):
        fields = []
        for statement in body.split(";"):
            identifiers = re.findall(r"[A-Za-z_]\w*", statement)
            if identifiers:
                fields.append(identifiers[-1])
        structs[name] = fields
    return structs


def header_symbols(header: Path) -> list[str]:
    text = COMMENT.sub("", header.read_text(encoding="utf-8"))
    return sorted(set(re.findall(r"CPPK2_API[^\n]*?\b(cppk2_[a-z0-9_]+)\s*\(", text)))


def python_functions(path: Path) -> list[str]:
    return sorted(set(re.findall(r'"(cppk2_[a-z0-9_]+)"', path.read_text(encoding="utf-8"))))


def python_structs(path: Path) -> dict[str, list[str]]:
    text = path.read_text(encoding="utf-8")
    classes = {name: body for name, body in PY_STRUCT.findall(text)}
    found = {}
    for c_name, (python_class, _cs_class) in STRUCT_CLASSES.items():
        if python_class in classes:
            found[c_name] = re.findall(r'\("(\w+)",', classes[python_class])
    return found


def csharp_functions(path: Path) -> list[str]:
    return sorted(set(CS_FIELD.findall(path.read_text(encoding="utf-8"))))


def csharp_structs(path: Path) -> dict[str, list[str]]:
    text = path.read_text(encoding="utf-8")
    found = {}
    for c_name, (_python_class, cs_class) in STRUCT_CLASSES.items():
        match = re.search(r"struct " + cs_class + r"\s*\{(.*?)\n        \}", text, re.S)
        if match:
            found[c_name] = CS_STRUCT_FIELD.findall(match.group(1))
    return found


def paths(root: Path) -> dict[str, Path]:
    return {
        "header": root / "include" / "cpp-kana2" / "ext" / "c_api.h",
        "symbols": root / "tests" / "abi" / "expected_c_api_symbols.txt",
        "python": root / "python" / "cpp_kana2" / "_ffi.py",
        "csharp": root / "csharp" / "CppKana2" / "NativeMethods.cs",
    }


def check(root: Path = ROOT) -> list[str]:
    where = paths(root)
    problems = []
    frozen = [line.strip() for line in where["symbols"].read_text(encoding="utf-8").splitlines()
              if line.strip() and not line.startswith("#")]

    for label, found in (("python/cpp_kana2/_ffi.py", python_functions(where["python"])),
                         ("csharp/CppKana2/NativeMethods.cs", csharp_functions(where["csharp"]))):
        if found != sorted(set(frozen)):
            problems.append(f"{label}: symbol set differs from the frozen list "
                            f"(only in the binding: {sorted(set(found) - set(frozen))}, "
                            f"only in the list: {sorted(set(frozen) - set(found))})")

    declared = header_symbols(where["header"])
    if declared != sorted(set(frozen)):
        problems.append(f"c_api.h declares {len(declared)} entry points but the frozen list has {len(frozen)}: "
                        f"only in the header {sorted(set(declared) - set(frozen))}, "
                        f"only in the list {sorted(set(frozen) - set(declared))}")

    structs = header_structs(where["header"])
    if not structs:
        problems.append("c_api.h: no typedef struct found; the comparison would compare nothing")
    for binding, found in (("python", python_structs(where["python"])),
                           ("csharp", csharp_structs(where["csharp"]))):
        for c_name, fields in structs.items():
            if c_name not in found:
                problems.append(f"{binding}: structure {c_name} is not transcribed")
                continue
            expected = [norm(field) for field in fields]
            actual = [norm(field) for field in found[c_name]]
            if expected != actual:
                problems.append(f"{binding}: {c_name} fields are {actual}, header says {expected}")
    return problems


def self_test() -> int:
    """Builds a small but complete fixture tree and drives `check()` itself, not just its helpers.

    This distinction matters: an earlier version of this self-test called the parse helpers directly, so
    deleting the symbol comparison inside `check()` left the self-test green.
    """
    header = (
        "CPPK2_API const char *cppk2_version(void);\n"
        "CPPK2_API int32_t cppk2_thing(int32_t a, cppk2_romaji_options *out);\n"
        "typedef struct cppk2_romaji_options {\n"
        "    size_t struct_size;\n"
        "    int32_t first;\n"
        "    int32_t second;\n"
        "} cppk2_romaji_options;\n"
    )
    python_ffi = (
        'C_API_FUNCTIONS = ["cppk2_version", "cppk2_thing"]\n\n'
        'class RomajiOptionsStruct(ctypes.Structure):\n'
        '    _fields_ = [\n'
        '        ("struct_size", SIZE_T),\n'
        '        ("first", ctypes.c_int32),\n'
        '        ("second", ctypes.c_int32),\n'
        '    ]\n\n'
    )
    csharp_native = (
        "    internal static class NativeMethods\n"
        "    {\n"
        "        [StructLayout(LayoutKind.Sequential)]\n"
        "        internal struct RomajiOptionsNative\n"
        "        {\n"
        "            public UIntPtr StructSize;\n"
        "            public int First;\n"
        "            public int Second;\n"
        "        }\n"
        "        [DllImport(\"x\")]\n"
        "        internal static extern IntPtr cppk2_version();\n"
        "        [DllImport(\"x\")]\n"
        "        internal static extern int cppk2_thing(int a, IntPtr opts);\n"
        "    }\n"
    )

    def build(root: Path, symbols: str, csharp: str = csharp_native) -> None:
        (root / "include" / "cpp-kana2" / "ext").mkdir(parents=True, exist_ok=True)
        (root / "tests" / "abi").mkdir(parents=True, exist_ok=True)
        (root / "python" / "cpp_kana2").mkdir(parents=True, exist_ok=True)
        (root / "csharp" / "CppKana2").mkdir(parents=True, exist_ok=True)
        (root / "include" / "cpp-kana2" / "ext" / "c_api.h").write_text(header, encoding="utf-8")
        (root / "tests" / "abi" / "expected_c_api_symbols.txt").write_text(symbols, encoding="utf-8",
                                                                          newline="\n")
        (root / "python" / "cpp_kana2" / "_ffi.py").write_text(python_ffi, encoding="utf-8")
        (root / "csharp" / "CppKana2" / "NativeMethods.cs").write_text(csharp, encoding="utf-8")

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        build(root, "cppk2_version\ncppk2_thing\n")
        clean = check(root)
        if clean:
            print(f"SELF-TEST FAIL: a consistent fixture was reported: {clean}", file=sys.stderr)
            return 1

        # A symbol frozen but not transcribed must be reported (this is the comparison the old self-test
        # never executed).
        build(root, "cppk2_version\ncppk2_thing\ncppk2_absent\n")
        found = " ".join(check(root))
        if "cppk2_absent" not in found:
            print(f"SELF-TEST FAIL: a missing binding symbol was not reported: {found!r}", file=sys.stderr)
            return 1

        # A reordered structure must be reported.
        reordered = csharp_native.replace("            public int First;\n            public int Second;\n",
                                          "            public int Second;\n            public int First;\n")
        build(root, "cppk2_version\ncppk2_thing\n", csharp=reordered)
        found = " ".join(check(root))
        if "fields are" not in found:
            print(f"SELF-TEST FAIL: a reordered structure was not reported: {found!r}", file=sys.stderr)
            return 1
    print("bindings: self-test passed (check() reports a missing symbol and a reordered structure)")
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
    print("bindings: Python and C# transcribe every C entry point and every structure, field for field")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
