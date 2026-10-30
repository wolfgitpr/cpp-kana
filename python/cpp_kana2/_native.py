"""Locating the native library, without ever hard-coding its file name.

The name differs per platform and per build layout (`cpp-kana2.dll`, `libcpp-kana2.so`,
`libcpp-kana2.dylib`, with or without a version suffix), so ``tools/build_native.py`` writes the name it
actually staged into ``_native/name.txt`` and this module believes it. Only when that file is missing --
a source checkout nobody staged -- does it fall back to scanning, and then the scan must find exactly one
candidate: guessing between two is how a wheel ends up loading the wrong build.
"""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

_DIR = Path(__file__).resolve().parent
_NATIVE_DIR = _DIR / "_native"
_SUFFIXES = (".dll", ".so", ".dylib")
_STEM = "cpp-kana2"


class NativeLibraryError(RuntimeError):
    """The native library could not be found or loaded."""


def _candidates() -> list[Path]:
    found = []
    for entry in _NATIVE_DIR.glob("*"):
        if entry.is_file() and entry.suffix in _SUFFIXES and _STEM in entry.name:
            found.append(entry)
    return sorted(found)


def _resolve() -> Path:
    recorded = _NATIVE_DIR / "name.txt"
    if recorded.is_file():
        name = recorded.read_text(encoding="utf-8").strip()
        path = _NATIVE_DIR / name
        if not path.is_file():
            raise NativeLibraryError(
                f"_native/name.txt says {name!r}, but that file is not next to it. The package is "
                f"inconsistent (was it copied by hand?); re-run python/tools/build_native.py."
            )
        return path

    candidates = _candidates()
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise NativeLibraryError(
            f"no native library in {_NATIVE_DIR}. Build one and stage it with:\n"
            f"    python tools/build_native.py\n"
            f"(looked for *{_STEM}*.dll|.so|.dylib)"
        )
    names = ", ".join(candidate.name for candidate in candidates)
    raise NativeLibraryError(f"more than one native library in {_NATIVE_DIR} ({names}); remove the stale one")


def load() -> ctypes.CDLL:
    path = _resolve()

    # On Windows, a DLL that cannot find one of ITS dependencies reports "the specified module could not
    # be found", which reads like "your file is missing". Making the attempt's real path part of the
    # message is what turns that into a diagnosable error.
    if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
        try:
            os.add_dll_directory(str(path.parent))
        except (OSError, AttributeError):  # pragma: no cover - platform detail
            pass

    try:
        return ctypes.CDLL(str(path))
    except OSError as error:  # pragma: no cover - depends on the machine
        raise NativeLibraryError(
            f"could not load {path}: {error}. On Windows this usually means a dependent DLL of the "
            f"library itself is missing, not that {path.name} is."
        ) from error
