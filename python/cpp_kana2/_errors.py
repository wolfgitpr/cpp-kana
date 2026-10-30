"""The Python exception hierarchy, mapped one-to-one onto the library's status codes.

One class per code, and a fallback that never lies: an unknown code becomes ``CppKanaError`` rather than
``KeyError`` (a status the library adds later must not crash a shell that compiles against an older
mapping).
"""

from __future__ import annotations

from . import _ffi


class CppKanaError(Exception):
    """Base class for every error the native library reports."""

    def __init__(self, code: int, detail: str = "") -> None:
        self.code = code
        self.detail = detail
        super().__init__(f"{_ffi_text(code)}" + (f": {detail}" if detail else ""))


class InvalidArgumentError(CppKanaError):
    pass


class InvalidUtf8Error(CppKanaError, ValueError):
    pass


class UnsupportedCharacterError(CppKanaError):
    pass


class NotReadyError(CppKanaError):
    pass


class DataCorruptError(CppKanaError):
    pass


class NotImplementedError_(CppKanaError):  # noqa: N801 - the trailing underscore avoids shadowing builtins
    pass


class InternalError(CppKanaError):
    pass


_BY_CODE = {
    _ffi.ERR_INVALID_ARGUMENT: InvalidArgumentError,
    _ffi.ERR_INVALID_UTF8: InvalidUtf8Error,
    _ffi.ERR_UNSUPPORTED_CHARACTER: UnsupportedCharacterError,
    _ffi.ERR_NOT_READY: NotReadyError,
    _ffi.ERR_DATA_CORRUPT: DataCorruptError,
    _ffi.ERR_NOT_IMPLEMENTED: NotImplementedError_,
    _ffi.ERR_INTERNAL: InternalError,
}


def _ffi_text(code: int) -> str:
    # The wording comes from the library (cppk2_code_text) so the shells cannot drift from it; the import
    # is deferred because _library imports this module while it is still initialising.
    from . import code_text

    return code_text(code)


def raise_for_status(code: int, detail: str = "") -> None:
    if code == _ffi.OK:
        return
    error_type = _BY_CODE.get(code)
    if error_type is None:
        # A code this version of the shell has never seen: report it as the base class, with the number,
        # rather than crashing in the mapping table.
        raise CppKanaError(code, detail)
    raise error_type(code, detail)
