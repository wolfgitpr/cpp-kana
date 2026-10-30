"""Makes ``import cpp_kana2`` work from a source checkout, and fails loudly when it cannot.

The shell is tested against the staged native library (``python/tools/build_native.py``), not against an
installed wheel: that is the shape a developer and CI both work in. If the library was never staged, the
session exits with the command to run instead of twenty confusing collection errors.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

PYTHON_DIR = Path(__file__).resolve().parents[1]  # python/
ROOT = PYTHON_DIR.parent
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

import cpp_kana2  # noqa: E402


@pytest.fixture(scope="session")
def conformance_fixture() -> dict:
    import json

    path = ROOT / "conformance" / "cases.json"
    if not path.is_file():
        pytest.exit(f"{path} is missing; run cpp-kana2-conformance --write", returncode=2)
    return json.loads(path.read_text(encoding="utf-8"))


@pytest.fixture(scope="session")
def library():
    return cpp_kana2._library
