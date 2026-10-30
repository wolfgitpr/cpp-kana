"""The ctypes transcription of ``include/cpp-kana2/ext/c_api.h`` -- and nothing else.

Every entry point gets explicit ``argtypes`` and ``restype``. Leaving ``restype`` unset is the classic
ctypes bug: the default is ``c_int``, so a pointer returned on a 64-bit platform is silently truncated.

``tests/test_contract.py`` compares this file against the header: the set of declared functions, the
structure field order, and the status constants. A binding that drifts from the header has to be edited in
two places, and the test is what makes that visible.
"""

from __future__ import annotations

import ctypes
from typing import List

# --- status codes, mirroring cpp_kana2::v2::Code and the CPPK2_ERR_* enum ---------------------------

OK = 0
ERR_INVALID_ARGUMENT = 1
ERR_INVALID_UTF8 = 2
ERR_UNSUPPORTED_CHARACTER = 3
ERR_NOT_READY = 4
ERR_DATA_CORRUPT = 5
ERR_NOT_IMPLEMENTED = 6
ERR_INTERNAL = 7

SCRIPT_HIRAGANA = 0
SCRIPT_KATAKANA = 1
SCRIPT_AS_IS = 2

SOKUON_GEMINATE = 0
SOKUON_DROP = 1

HATSUON_APOSTROPHE = 0
HATSUON_PLAIN = 1

LONG_VOWEL_REPEAT = 0
LONG_VOWEL_DROP = 1
LONG_VOWEL_KATAKANA_DASH = 2

CASE_LOWER = 0
CASE_UPPER = 1
CASE_CAPITALIZE = 2

SEPARATOR_SPACE = 0
SEPARATOR_NONE = 1

UNKNOWN_KEEP = 0
UNKNOWN_IGNORE = 1
UNKNOWN_FAIL = 2

MORA_SYLLABLE = 0
MORA_SOKUON = 1
MORA_HATSUON = 2
MORA_LONG_VOWEL = 3
MORA_OTHER = 4
MORA_ARCHAIC = 5
MORA_ROMAJI = 6

# --- structures -------------------------------------------------------------------------------------


class MoraStruct(ctypes.Structure):
    _fields_ = [
        ("text", ctypes.c_char_p),
        ("rendered", ctypes.c_char_p),
        ("kind", ctypes.c_int32),
        ("code", ctypes.c_int32),
    ]


class ConversionStruct(ctypes.Structure):
    _fields_ = [
        ("text", ctypes.c_char_p),
        ("rendered", ctypes.c_char_p),
        ("moras", ctypes.POINTER(MoraStruct)),
        ("mora_count", ctypes.c_int32),
        ("ok", ctypes.c_int32),
    ]


class ErrorStruct(ctypes.Structure):
    _fields_ = [
        ("code", ctypes.c_int32),
        ("reserved_0", ctypes.c_int32),
        ("detail", ctypes.c_void_p),  # char*, freed with cppk2_string_free
    ]


SIZE_T = ctypes.c_size_t


class RomajiOptionsStruct(ctypes.Structure):
    _fields_ = [
        ("struct_size", SIZE_T),
        ("sokuon", ctypes.c_int32),
        ("hatsuon", ctypes.c_int32),
        ("long_vowel", ctypes.c_int32),
        ("letter_case", ctypes.c_int32),
        ("separator", ctypes.c_int32),
        ("unknown", ctypes.c_int32),
        ("reserved_1", ctypes.c_int32),
        # Two reserved fields, not one: with one, the 4 bytes of tail padding on a 64-bit target would absorb
        # a newly added int32 and sizeof would not change, defeating the *_options struct_size handshake.
        ("reserved_2", ctypes.c_int32),
    ]


class KanaOptionsStruct(ctypes.Structure):
    _fields_ = [
        ("struct_size", SIZE_T),
        ("script", ctypes.c_int32),
        ("unknown", ctypes.c_int32),
        ("reserved_1", ctypes.c_int32),
        # Two reserved fields, not one: with one, the 4 bytes of tail padding on a 64-bit target would absorb
        # a newly added int32 and sizeof would not change, defeating the *_options struct_size handshake.
        ("reserved_2", ctypes.c_int32),
    ]


C_API_FUNCTIONS = [
    "cppk2_version",
    "cppk2_code_text",
    "cppk2_romaji_options_init",
    "cppk2_kana_options_init",
    "cppk2_kana_to_romaji",
    "cppk2_romaji_to_kana",
    "cppk2_convert_script",
    "cppk2_conversion_free",
    "cppk2_string_free",
]


def declare(library: ctypes.CDLL) -> ctypes.CDLL:
    """Binds every entry point with its real signature."""
    ctypes_version = ctypes.c_char_p

    library.cppk2_version.argtypes = []
    library.cppk2_version.restype = ctypes_version

    library.cppk2_code_text.argtypes = [ctypes.c_int32]
    library.cppk2_code_text.restype = ctypes_version

    library.cppk2_romaji_options_init.argtypes = [ctypes.POINTER(RomajiOptionsStruct), SIZE_T]
    library.cppk2_romaji_options_init.restype = ctypes.c_int32

    library.cppk2_kana_options_init.argtypes = [ctypes.POINTER(KanaOptionsStruct), SIZE_T]
    library.cppk2_kana_options_init.restype = ctypes.c_int32

    for name in ("cppk2_kana_to_romaji", "cppk2_romaji_to_kana"):
        function = getattr(library, name)
        function.argtypes = [
            ctypes.c_char_p,
            ctypes.c_void_p,  # const options*, NULL for defaults
            ctypes.POINTER(ctypes.POINTER(ConversionStruct)),
            ctypes.POINTER(ErrorStruct),
        ]
        function.restype = ctypes.c_int32

    library.cppk2_convert_script.argtypes = [
        ctypes.c_char_p,
        ctypes.c_int32,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.POINTER(ConversionStruct)),
        ctypes.POINTER(ErrorStruct),
    ]
    library.cppk2_convert_script.restype = ctypes.c_int32

    library.cppk2_conversion_free.argtypes = [ctypes.POINTER(ConversionStruct)]
    library.cppk2_conversion_free.restype = None

    library.cppk2_string_free.argtypes = [ctypes.c_void_p]
    library.cppk2_string_free.restype = None

    return library


def take_detail(library: ctypes.CDLL, error: ErrorStruct) -> str:
    """Decodes ``error.detail`` and releases it. One place, because forgetting the free is the usual leak."""
    if not error.detail:
        return ""
    try:
        raw = ctypes.cast(error.detail, ctypes.c_char_p).value or b""
        return raw.decode("utf-8", "replace")
    finally:
        library.cppk2_string_free(error.detail)
        error.detail = None


def moras_of(conversion: ConversionStruct) -> List["MoraView"]:
    views = []
    for index in range(conversion.mora_count):
        mora = conversion.moras[index]
        views.append(
            MoraView(
                text=(mora.text or b"").decode("utf-8", "replace"),
                rendered=(mora.rendered or b"").decode("utf-8", "replace"),
                kind=mora.kind,
                code=mora.code,
            )
        )
    return views


class MoraView:
    """A detached copy of one ``cppk2_mora``: the memory it came from is freed with the conversion."""

    __slots__ = ("text", "rendered", "kind", "code")

    def __init__(self, text: str, rendered: str, kind: int, code: int) -> None:
        self.text = text
        self.rendered = rendered
        self.kind = kind
        self.code = code

    def __repr__(self) -> str:
        return f"Mora(text={self.text!r}, rendered={self.rendered!r}, kind={self.kind}, code={self.code})"

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MoraView):
            return NotImplemented
        return (self.text, self.rendered, self.kind, self.code) == (
            other.text,
            other.rendered,
            other.kind,
            other.code,
        )
