# Verification record

What was actually measured, where, and with which toolchain. Numbers here are observations, not targets: the
targets live in [CONTRACT.md](CONTRACT.md).

## Windows development machine (the primary loop)

| Item | Value |
|---|---|
| Toolchain | MinGW g++ 16.1.0, CMake 3.30.4, Ninja, Python 3.13.9, .NET SDK 7.0.302 / 10.0.402 |
| `ctest --preset default` | 16/16 — `Smoke`, `CapiSmoke`, `ConformanceFixture`, nine gate tests, `GateSelfTests`, `Gate_ExportGate` (+self-test), `Gate_InstallConsumer` |
| `ctest --preset static` | 13/13 (the export gate is shared-only, the SONAME gate is ELF-only, by design) |
| `python -m pytest python/tests -q` | 18 passed |
| `python csharp/tools/run_tests.py` | "all C# checks passed" on net10.0 **and** net7.0 |
| `python tools/run_all_gates.py` | 9/9 at the time of that replay (the tenth gate, `check_doc_citations`, came later), each preceded by its own `--self-test` |
| `pip wheel ./python` | `cpp_kana2-2.0.0-py3-none-win_amd64.whl` (not `py3-none-any`); installed into a target directory and answered `がっこう -> gakkou` |
| `dotnet pack` | `lib/net7.0`, `lib/net10.0`, `runtimes/win-x64/native/libcpp-kana2.dll` |
| exports (PE, measured with `objdump -p`) | 25 = 9 `cppk2_*` + 16 C++ public names, no `std::` internals |

## Linux replay on the development board (firefly.local)

Round 3 of the replay, on the `v2` branch just before the 2.0.0 tag, from a plain `git archive v2` tree with no repair applied.

| Item | Value |
|---|---|
| Container | `quay.io/pypa/manylinux_2_28_aarch64` (digest `sha256:a3e02308…`) |
| Toolchain | GCC 14.2.1 (Red Hat gcc-toolset-14), CMake 4.4.4, Ninja 1.12.1, Python 3.12.16, binutils 2.41 |
| Host | `Linux firefly 4.4.194 aarch64` |
| `ctest` | **17/17 passed**, 0 skipped, 0 not run — `100% tests passed out of 17` |
| SONAME | `Library soname: [libcpp-kana2.so.2]`; symlinks `.so -> .so.2 -> .so.2.0.0` |
| exports (ELF, measured with `nm -D`) | **26** = 9 `cppk2_*` + 12 `_ZN8cpp_kana2v2*` + 2 `_ZNK8cpp_kana2v2*` + `_ZTI`/`_ZTS`/`_ZTV` of `ErrorException`; `std::` internals **0** |
| consumer probe | an independent program links against the installed package and prints `CONSUMER=rendered=kana` |
| exception probe | `PROBE=CAUGHT_BY_TYPE code=2 what=invalid UTF-8 …`, typeinfo `N8cpp_kana2v214ErrorExceptionE` |

Why 26 on ELF and 25 on PE: the two export mechanisms are different (a version script that lists mangled
patterns vs. `dllexport`), and `ErrorException`'s typeinfo/name/vtable are exported on ELF because a consumer's
`catch (cpp_kana::v2::ErrorException)` needs them to match. Both numbers are "the C ABI plus the C++ public
names and nothing else"; neither is a target.

## What the replay found that Windows could not

Three defects, all in the verification layer rather than the converter, and all fixed:

1. `Gate_SonameGate` compared against `libcpp-kana2-tests.so.` — `tests/CMakeLists.txt` declares its own
   `project(cpp-kana2-tests ...)`, so `${PROJECT_NAME}` was the test project. The library's SONAME was right
   the whole time.
2. `-fvisibility=hidden` was not enough on ELF: 8 standard-library internals were exported from templates
   instantiated inside a default-visibility class. Fixed with `cmake/exports.map`.
3. The first version of that map used `;;` comments (GNU ld only takes `/* */`) and omitted `_ZNK` — const
   member functions — which broke the link of the conformance tool and of a consumer. Both fixed.

A fourth, smaller one: `check_doc_links` relied on `git check-ignore`, which cannot answer in an exported tree
without `.git`; it now also recognises generated paths by shape.

## Clang, on the same Windows machine

A second compiler is worth more than a second platform for catching undefined behaviour, and this machine has
Clang 22.1.4 — but it targets `x86_64-pc-windows-msvc`, and no MSVC toolset is installed. Clang can be pointed
at the MinGW runtime instead:

```
clang --target=x86_64-w64-windows-gnu -idirafter <mingw>/x86_64-w64-mingw32/include
```

`-idirafter`, not `-isystem`: with `-isystem` the `#include_next <stdlib.h>` inside libstdc++'s `<cstdlib>`
fails to resolve and every translation unit stops at the first `#include <string>`. MEASURED before and after.

| Item | Value |
|---|---|
| `ctest` (Clang 22.1.4, MinGW target) | **16/16** — same set as the GCC leg |
| `python tools/verify_all.py` | 6/6 legs green: `mingw-shared`, `mingw-static`, `clang`, `python`, `csharp`, `gates` |

This leg found a real defect in a gate: `Gate_InstallConsumer` forwarded the outer build's compiler but not its
`CMAKE_CXX_FLAGS`, so the consumer project was configured for the MSVC target and failed to link — reporting a
broken installation that was not broken. The gate now forwards `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`.

## Not measured

- MSVC: no `cl.exe` on this machine (the Visual Studio install has no C++ toolset), so the `/Brepro`, `/utf-8`
  and `dumpbin` paths are written but unexercised. The PE export gate has only been run against MinGW output.
- macOS: no host. The Apple branch of the CMake (`.dylib`, `MACOSX_DEPLOYMENT_TARGET`) is written but unrun.
- Reproducible-build byte equality: the flags are set (`-ffile-prefix-map`, `/Brepro`, deterministic `ar`), but
  no gate compares two builds.
- Performance: nothing in this repository claims a figure, so nothing measures one.
