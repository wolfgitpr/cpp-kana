# Quick start

Three tracks: C++, C, and the Python/C# bindings. All of them use the same build: a C++17 library named
`cpp-kana2`, version `2.0.0`, whose CMake project builds the library target `cpp-kana2` and the alias
`cpp-kana2::cpp-kana2`.

| Track | Needs |
|---|---|
| C++ | CMake 3.17 or newer (3.21+ to use the presets), a C++17 compiler |
| C | the same build, plus a C99 compiler |
| Python / C# | Python 3, plus the `dotnet` SDK for the C# suite |

## C++ track

### Configure and build

The repository ships three presets (`default`, `static`, `sanitize`); `default` is a Release build with
Ninja, `BUILD_SHARED_LIBS=ON`, tests and install on:

```bash
cmake --preset default
cmake --build --preset default
```

The same thing without presets:

```bash
cmake -S . -B .tmp/build-default -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .tmp/build-default
```

`sanitize` is `RelWithDebInfo` with ASan+UBSan and refuses to configure when the toolchain cannot build
with those runtimes; `static` is the same Release build with `BUILD_SHARED_LIBS=OFF`.

### Link the target

In your own CMake project, link the exported target:

```cmake
find_package(cpp-kana2 2.0 CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE cpp-kana2::cpp-kana2)
```

`find_package` with a major-version request other than 2 (for example `3.0`) is refused by the installed
version file (`COMPATIBILITY SameMajorVersion`). The target carries C++17 (`cxx_std_17`) and, for a static
build, the public definition `CPP_KANA2_STATIC`. A Makefile/pkg-config consumer gets the same facts from
`cpp-kana2.pc`, whose `Cflags` spells out `-DCPP_KANA2_STATIC` for a static build because pkg-config has no
channel for compile definitions.

### Minimal program

```cpp
#include <cstdio>

#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Types.h>

int main() {
    using namespace cpp_kana::v2;

    const auto result = kanaToRomaji("こんにちは");
    if (!result.ok()) {                                        // a failure is a value, not an exception
        std::printf("failed: %s\n", codeText(result.error().code));
        return 1;
    }
    std::printf("%s\n", result.value().rendered().c_str());     // konnichiha
    return 0;
}
```

`result.value().moras()` gives the same conversion per unit (`こ → ko`, `ん → n`, …) with a `MoraKind` and a
per-unit `Code`; see [API.md](API.md).

### Example target

Configure with `-DCPP_KANA2_BUILD_EXAMPLES=ON` and the tree builds `cpp-kana2-example-minimal` from
`examples/minimal_usage.cpp`, which calls all three conversions:

```bash
cmake -S . -B .tmp/build-examples -G Ninja -DCMAKE_BUILD_TYPE=Release -DCPP_KANA2_BUILD_EXAMPLES=ON
cmake --build .tmp/build-examples
```

### What the build produces

What each configuration puts on disk; paths follow CMake's output variables (`<build>/bin` for runtimes,
`<build>/lib` for libraries, set in the root `CMakeLists.txt`).

| Build | Product |
|---|---|
| `BUILD_SHARED_LIBS=ON` (default, preset `default`) | one shared library named `cpp-kana2`, with `VERSION 2.0.0` and `SOVERSION 2`. ELF: SONAME `libcpp-kana2.so.2`, with `libcpp-kana2.so` as the unversioned symlink. Windows: `cpp-kana2.dll` (MSVC, with import library `cpp-kana2.lib`) or `libcpp-kana2.dll` (MinGW). macOS: `libcpp-kana2.dylib` (versioned file plus symlinks, as on ELF). The MSVC Debug configuration appends the postfix `d`. |
| `BUILD_SHARED_LIBS=OFF` (preset `static`) | static archive (`cpp-kana2.lib` / `libcpp-kana2.a`) built with `POSITION_INDEPENDENT_CODE ON`, and the public compile definition `CPP_KANA2_STATIC` |
| `CPP_KANA2_BUILD_TESTS=ON` (default preset) | the test executables and `ctest` entries described below |
| `CPP_KANA2_INSTALL=ON` (default for a top-level build) | install rules: headers under `include/cpp-kana2/`, the library, `cpp-kana2Config.cmake` + `cpp-kana2Targets.cmake`, `cpp-kana2.pc`, and `data/moras.tsv` under `share/cpp-kana2/` |

Tests and gates:

```bash
ctest --preset default
```

With a Python 3 interpreter present this registers the smoke tests (`Smoke`, `CapiSmoke`), the fixture check
(`ConformanceFixture`), one ctest entry per gate script under `tools/`, the export gate (shared builds), the
SONAME gate (ELF only) and the install-consumer gate (`CPP_KANA2_INSTALL=ON`). Without Python 3 the gate
entries are not registered and the compiler-only tests still run.

## C track

The C ABI is a first-class surface: `<cpp-kana2/ext/c_api.h>` compiles as C99, and the library exports the
`cppk2_*` symbols whether it was built as a shared library or an archive.

### Install, then compile a `.c` file against it

```bash
cmake --preset default
cmake --build --preset default
cmake --install .tmp/build-default --prefix "$HOME/cpp-kana2"
```

GCC/Clang, against the installed tree:

```bash
cc -std=c99 -I"$HOME/cpp-kana2/include" app.c -L"$HOME/cpp-kana2/lib" -lcpp-kana2 -o app
```

MSVC, against the same prefix (the DLL is installed into `bin`):

```bat
cl /nologo /W4 /I"%PREFIX%\include" app.c "%PREFIX%\lib\cpp-kana2.lib" /Fe:app.exe
```

For a static install, pass `-DCPP_KANA2_STATIC` to the compiler (that definition selects the "no dllimport"
branch of both headers); the CMake target and the `.pc` file do this for you.

### Minimal program

```c
#include <stdio.h>
#include <string.h>

#include <cpp-kana2/ext/c_api.h>

int main(void) {
    cppk2_conversion *conversion = NULL;
    cppk2_error error;
    memset(&error, 0, sizeof(error));

    if (cppk2_kana_to_romaji("きって", NULL, &conversion, &error) != CPPK2_OK) {
        printf("%s: %s\n", cppk2_code_text(error.code), error.detail);
        cppk2_string_free(error.detail);   /* the library frees its own allocations */
        return 1;
    }
    printf("%s (%d morae)\n", conversion->rendered, conversion->mora_count);  /* kitte (3 morae) */
    cppk2_conversion_free(conversion);
    return 0;
}
```

Two rules that matter more than anything else in this header: pass `struct_size` (set it to
`sizeof(your_struct)`) to `cppk2_romaji_options_init` / `cppk2_kana_options_init` before using an options
struct, and never release a `cppk2_*` pointer with the C runtime's `free()` — use `cppk2_conversion_free` /
`cppk2_string_free`. [API.md](API.md#c-abi) states both contracts in full.

At run time the shared library must be findable: put `<prefix>/lib` on `LD_LIBRARY_PATH` (Linux),
`DYLD_LIBRARY_PATH` (macOS), or `<prefix>\bin` on `PATH` (Windows).

## Bindings track

Both bindings sit on the C ABI: the Python package loads the shared library with `ctypes`, the C# assembly
calls it with P/Invoke. Neither one re-implements conversion.

### Python

```bash
python python/tools/build_native.py
```

That script configures and builds the library into `.tmp/py-native` with `BUILD_SHARED_LIBS=ON`,
`CPP_KANA2_BUILD_TESTS=OFF` and `CPP_KANA2_INSTALL=OFF`, writes `python/cpp_kana2/_version.py` from
`project(cpp-kana2 VERSION ...)` in the root `CMakeLists.txt`, copies the built library into
`python/cpp_kana2/_native/`, records its real file name in `_native/name.txt`, and finishes by importing the
package and checking that `きって` renders as `kitte`. On Windows it also links the C/C++ runtime statically,
so the staged DLL has no MinGW runtime dependency to satisfy at load time.

```bash
cd python
python -c "import cpp_kana2; print(cpp_kana2.kana_to_romaji('きって').rendered)"   # kitte
```

`import cpp_kana2` needs `python/` on `sys.path` (or an install from `python/`), and it raises
`NativeLibraryError` if no native library has been staged. The suite is pytest:

```bash
python -m pytest python/tests -q
```

Object model: `kana_to_romaji` / `romaji_to_kana` / `convert_script` / `version` / `code_text` return or take
`Conversion`, `Mora`, `RomajiOptions` and `KanaOptions`, and raise one exception class per status code
(`CppKanaError` and its subclasses). The name mapping is in
[API.md](API.md#python-and-c).

### C#

```bash
python csharp/tools/run_tests.py
```

It needs `dotnet`. It locates the native library (`--native-dir <build>/bin`, or the one the Python step
staged in `python/cpp_kana2/_native`), writes `csharp/CppKana2/BuildInfo.g.cs` with that file's real name and
the version from the root `CMakeLists.txt`, then runs the test project with `dotnet run`, per framework, with
the library's directory passed in so the assembly and the native file end up in the same output folder. The
default frameworks are `net10.0` and `net7.0`, limited to the SDKs `dotnet --list-sdks` reports.

```bash
python csharp/tools/run_tests.py --native-dir .tmp/build-default/bin --framework net10.0
```

The C# surface is `Kana.KanaToRomaji`, `Kana.RomajiToKana`, `Kana.ConvertScript`, `Kana.Version`,
`Kana.CodeText`, with the `RomajiOptions` / `KanaOptions` classes, `Conversion` / `Mora`, and
`CppKana2Exception` plus one subclass per status code.
