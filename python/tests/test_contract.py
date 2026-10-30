"""The shell's transcription must match the header, field for field and name for name.

Two drifts this test exists for:

* a function added to ``c_api.h`` and not to ``_ffi.py`` (or the reverse) -- the shell would be quietly
  incomplete;
* a structure whose fields were reordered -- every field would still be readable, and every value would be
  wrong, which no behavioural test would attribute to the layout.

It parses the header rather than importing a second copy of the truth, because a copy of the truth is what
the drift would live in.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

import cpp_kana2
from cpp_kana2 import _ffi

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "include" / "cpp-kana2" / "ext" / "c_api.h"


@pytest.fixture(scope="module")
def header_text() -> str:
    if not HEADER.is_file():
        pytest.exit(f"{HEADER} is missing", returncode=2)
    return HEADER.read_text(encoding="utf-8")


def _declared_functions(header: str) -> set[str]:
    return set(re.findall(r"CPPK2_API\s+[A-Za-z0-9_ *]+?\b(cppk2_[a-z0-9_]+)\s*\(", header))


def _struct_fields(header: str, struct_name: str) -> list[str]:
    match = re.search(r"typedef struct " + struct_name + r"\s*\{(.*?)\}\s*" + struct_name + r"\s*;", header, re.S)
    if not match:
        raise AssertionError(f"structure {struct_name} not found in c_api.h")
    body = re.sub(r"//[^\n]*", "", match.group(1))  # strip line comments
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)  # and block comments
    fields = []
    for line in body.split(";"):
        line = line.strip()
        if not line:
            continue
        # "int32_t kind" / "char *text" / "cppk2_mora *moras" -> last identifier
        identifiers = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", line)
        if identifiers:
            fields.append(identifiers[-1])
    return fields


def _header_constants(header: str) -> dict[str, int]:
    return {name: int(value) for name, value in re.findall(r"\b(CPPK2_[A-Z0-9_]+)\s*=\s*(\d+)", header)}


def test_every_declared_function_is_bound(header_text: str) -> None:
    declared = _declared_functions(header_text)
    assert declared, "the header parser found no CPPK2_API declarations -- the regex or the header changed"
    assert declared == set(_ffi.C_API_FUNCTIONS), (
        "c_api.h and _ffi.py disagree about the C surface: "
        f"only in header {sorted(declared - set(_ffi.C_API_FUNCTIONS))}, "
        f"only in _ffi {sorted(set(_ffi.C_API_FUNCTIONS) - declared)}"
    )


def test_every_declared_function_exists_in_the_library() -> None:
    for name in _ffi.C_API_FUNCTIONS:
        assert hasattr(cpp_kana2._library, name), f"{name} is declared and bound but missing from the library"


def test_struct_layouts_match_the_header(header_text: str) -> None:
    pairs = {
        "cppk2_mora": _ffi.MoraStruct,
        "cppk2_conversion": _ffi.ConversionStruct,
        "cppk2_error": _ffi.ErrorStruct,
        "cppk2_romaji_options": _ffi.RomajiOptionsStruct,
        "cppk2_kana_options": _ffi.KanaOptionsStruct,
    }
    for struct_name, structure in pairs.items():
        assert [name for name, _type in structure._fields_] == _struct_fields(header_text, struct_name), (
            f"{struct_name} field order differs between c_api.h and _ffi.py"
        )


def test_status_constants_match_the_header(header_text: str) -> None:
    constants = _header_constants(header_text)
    assert constants.get("CPPK2_OK") == _ffi.OK
    assert constants.get("CPPK2_ERR_INVALID_UTF8") == _ffi.ERR_INVALID_UTF8
    assert constants.get("CPPK2_ERR_INTERNAL") == _ffi.ERR_INTERNAL
    assert constants.get("CPPK2_MORA_SOKUON") == _ffi.MORA_SOKUON
    assert constants.get("CPPK2_UNKNOWN_FAIL") == _ffi.UNKNOWN_FAIL


def test_errors_cover_every_status_code() -> None:
    from cpp_kana2 import _errors

    known = {_ffi.ERR_INVALID_ARGUMENT, _ffi.ERR_INVALID_UTF8, _ffi.ERR_UNSUPPORTED_CHARACTER,
             _ffi.ERR_NOT_READY, _ffi.ERR_DATA_CORRUPT, _ffi.ERR_NOT_IMPLEMENTED, _ffi.ERR_INTERNAL}
    assert set(_errors._BY_CODE) == known
