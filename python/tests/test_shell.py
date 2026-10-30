"""Shell behaviour: the parts that are this module's own, not the library's.

The conformance suite proves the bytes agree with C++. What is left to prove here is the layer Python adds:
option validation, the exception mapping, memory ownership, and the promises the API makes (NUL rejection,
empty input, thread-safe concurrent reads).
"""

from __future__ import annotations

import ctypes
import threading

import pytest

import cpp_kana2


def test_version_chain() -> None:
    assert cpp_kana2.version() == cpp_kana2.__version__
    assert cpp_kana2.version().startswith("2.")


def test_code_text_comes_from_the_library() -> None:
    assert cpp_kana2.code_text(0) == "ok"
    assert cpp_kana2.code_text(2) == "invalid UTF-8"
    assert cpp_kana2.code_text(9999) == "unknown error"


def test_kana_to_romaji_breaks_down_morae() -> None:
    conversion = cpp_kana2.kana_to_romaji("きって", cpp_kana2.RomajiOptions(separator="none"))
    assert conversion.rendered == "kitte"
    assert [mora.rendered for mora in conversion.moras] == ["ki", "t", "te"]
    assert conversion.moras[1].kind == cpp_kana2.MoraKind.SOKUON
    assert conversion.ok


def test_unknown_character_is_marked_not_guessed() -> None:
    kept = cpp_kana2.kana_to_romaji("こん好にちは")
    assert kept.rendered == "kon好nichiha"
    assert not kept.ok
    assert any(mora.code == 3 for mora in kept.moras)  # 3 = CPPK2_ERR_UNSUPPORTED_CHARACTER

    ignored = cpp_kana2.kana_to_romaji("こん好にちは", cpp_kana2.RomajiOptions(unknown="ignore"))
    assert ignored.rendered == "konnichiha"

    with pytest.raises(cpp_kana2.UnsupportedCharacterError):
        cpp_kana2.kana_to_romaji("こん好にちは", cpp_kana2.RomajiOptions(unknown="fail"))


def test_embedded_nul_is_refused() -> None:
    with pytest.raises(cpp_kana2.InvalidArgumentError):
        cpp_kana2.kana_to_romaji("あ\x00い")


def test_non_string_input_is_refused() -> None:
    with pytest.raises(cpp_kana2.InvalidArgumentError):
        cpp_kana2.kana_to_romaji("あ".encode("utf-8"))


def test_unknown_option_value_is_refused() -> None:
    with pytest.raises(cpp_kana2.InvalidArgumentError):
        cpp_kana2.kana_to_romaji("あ", cpp_kana2.RomajiOptions(sokuon="sometimes"))


def test_empty_input_is_an_empty_result_not_an_error() -> None:
    conversion = cpp_kana2.kana_to_romaji("")
    assert conversion.rendered == ""
    assert conversion.moras == ()
    assert conversion.ok


def test_invalid_utf8_reaches_the_shell_as_a_code() -> None:
    # The Python API takes str, so it cannot produce invalid UTF-8; this calls the C ABI directly to prove
    # the failure path decodes and does not leak the detail string.
    from cpp_kana2 import _ffi

    out = ctypes.POINTER(_ffi.ConversionStruct)()
    error = _ffi.ErrorStruct()
    status = cpp_kana2._library.cppk2_kana_to_romaji(b"\xE3\x81", None, out, error)
    assert status == _ffi.ERR_INVALID_UTF8
    detail = _ffi.take_detail(cpp_kana2._library, error)
    assert "byte offset" in detail


def test_conversions_are_independent_across_threads() -> None:
    # The library holds no mutable state, so concurrent conversions must not interfere. This is a smoke
    # check of that promise, not a race detector.
    results: list[str] = []
    lock = threading.Lock()

    def worker() -> None:
        for _ in range(200):
            rendered = cpp_kana2.kana_to_romaji("がっこう").rendered
            with lock:
                results.append(rendered)

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    assert set(results) == {"gakkou"}


def test_repeated_calls_do_not_leak_the_conversion() -> None:
    # A leak would not fail here, but a double free or a use-after-free would: the objects are rebuilt from
    # native memory that is released in a finally block, so a stale pointer surfaces as garbage bytes.
    for _ in range(500):
        conversion = cpp_kana2.kana_to_romaji("こんにちは、せかい")
        assert conversion.rendered.startswith("konnichiha")
        assert all(mora.rendered is not None for mora in conversion.moras)
