"""Packaging for the Python shell.

The package is NOT pure Python: it ships a compiled library in ``cpp_kana2/_native/``. That has two
consequences this file enforces deliberately:

* ``root_is_pure = False`` and a ``py3-none-<platform>`` tag. A wheel tagged ``py3-none-any`` would install
  on every machine and then fail to load on all but the one it was built for. The tag is computed by
  ``_platform_tag()`` rather than left to setuptools, because setuptools would derive it from the
  interpreter's platform rather than from the library that is actually inside.
* the version comes from ``cpp_kana2/_version.py``, which ``tools/build_native.py`` generates from the
  root ``CMakeLists.txt``. No version literal appears here (docs/STABILITY.md section 1: one authority).

Measured recipe:

    python tools/build_native.py
    python -m pip wheel . --no-build-isolation --no-deps -w dist
"""

from __future__ import annotations

import sys
from pathlib import Path

from setuptools import setup
from setuptools.command.bdist_wheel import bdist_wheel


def _version() -> str:
    namespace: dict[str, str] = {}
    version_file = Path(__file__).parent / "cpp_kana2" / "_version.py"
    if not version_file.is_file():
        raise SystemExit(
            "cpp_kana2/_version.py is missing. Run: python tools/build_native.py "
            "(it reads the version from the root CMakeLists.txt and stages the native library)"
        )
    exec(version_file.read_text(encoding="utf-8"), namespace)  # noqa: S102 - generated, one assignment
    return namespace["__version__"]


def _platform_tag() -> str:
    if sys.platform == "win32":
        return "win_amd64" if sys.maxsize > 2**32 else "win32"
    if sys.platform == "darwin":
        import platform

        machine = platform.machine().lower()
        return "macosx_11_0_arm64" if machine in {"arm64", "aarch64"} else "macosx_10_15_x86_64"
    machine = "aarch64" if sys.maxsize > 2**32 and sys.platform.startswith("linux") else "x86_64"
    import platform

    if platform.machine().lower() in {"aarch64", "arm64"}:
        machine = "aarch64"
    # manylinux_2_28 is deliberately conservative: the CI leg builds inside a manylinux_2_28 container, and
    # claiming a lower floor than the toolchain emits is how a wheel installs and then refuses to start.
    return f"manylinux_2_28_{machine}"


class PlatformWheel(bdist_wheel):
    def finalize_options(self) -> None:
        super().finalize_options()
        self.root_is_pure = False

    def get_tag(self) -> tuple[str, str, str]:
        _python, _abi, platform = super().get_tag()
        return "py3", "none", _platform_tag()


def _package_data() -> dict[str, list[str]]:
    native = Path(__file__).parent / "cpp_kana2" / "_native"
    if not native.is_dir() or not any(native.iterdir()):
        raise SystemExit("cpp_kana2/_native/ is empty. Run: python tools/build_native.py")
    return {"cpp_kana2": ["_native/*"]}


setup(
    name="cpp-kana2",
    version=_version(),
    description="Japanese kana <-> Hepburn romaji, over the cpp-kana2 C ABI",
    author="wolfgitpr",
    keywords="japanese kana hiragana katakana romaji hepburn nlp",
    long_description=(Path(__file__).parent.parent / "README.md").read_text(encoding="utf-8")
    if (Path(__file__).parent.parent / "README.md").is_file()
    else "",
    long_description_content_type="text/markdown",
    license="Apache-2.0",
    # The wheel carries the licence text too: a package installed into a virtualenv is often the only copy a
    # user has, and "Apache-2.0" in METADATA is not the licence.
    license_files=["LICENSE"],
    python_requires=">=3.9",
    # PyPI shows these as the project's sidebar. Without them the page has no link to the source or the docs,
    # which for a binding (where the API reference is the whole point) is the difference between usable and not.
    url="https://github.com/wolfgitpr/cpp-kana",
    project_urls={
        "Documentation": "https://github.com/wolfgitpr/cpp-kana/blob/v2/docs/README.md",
        "API reference": "https://github.com/wolfgitpr/cpp-kana/blob/v2/docs/API.md",
        "Source": "https://github.com/wolfgitpr/cpp-kana/tree/v2",
        "Changelog": "https://github.com/wolfgitpr/cpp-kana/blob/v2/CHANGELOG.md",
        "Issues": "https://github.com/wolfgitpr/cpp-kana/issues",
    },
    packages=["cpp_kana2"],
    package_data=_package_data(),
    include_package_data=True,
    zip_safe=False,
    cmdclass={"bdist_wheel": PlatformWheel},
    classifiers=[
        # Beta rather than Stable: 2.0.0 is the first release of this line, and claiming Stable on a first
        # release is a promise the issue tracker has not had a chance to test.
        "Development Status :: 4 - Beta",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: C++",
        "License :: OSI Approved :: Apache Software License",
        "Natural Language :: Japanese",
        "Intended Audience :: Developers",
        "Topic :: Text Processing :: Linguistic",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Operating System :: MacOS",
    ],
)
