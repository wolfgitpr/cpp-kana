# cpp-kana2

`cpp-kana2` converts Japanese kana to Hepburn romaji and back. Kana input may be hiragana, katakana,
half-width katakana, or a kana followed by a dakuten/handakuten mark, precomposed or combining; romaji input
accepts the Hepburn spellings plus the common input aliases. The library is C++17 and has no third-party
dependencies: the mora table is compiled in, and the tests use an assertion harness that ships with the tree.
It offers three calling surfaces — the C++ API, the C ABI, and the Python and C# bindings, which sit on top of
the C ABI.

## Status

This is the `v2` branch. The package is `cpp-kana2`, the C++ namespace is `cpp_kana::v2`, the public headers
live under `<cpp-kana2/...>`, and the C surface carries its own tokens: `cppk2_` on symbols and typedefs,
`CPPK2_` on enum constants and macros. The version is `2.0.0`; this development line has not been released
yet, so that number is the contract for its first release rather than a bump of the v1 line.

It is a from-scratch rewrite that does not aim for behavioural compatibility with v1: v1's output is not a
contract here, and where v2 deliberately answers differently, the reason is recorded per case in
[docs/V1_DIFFERENCES.md](docs/V1_DIFFERENCES.md).

## Quick start

### C++

```cpp
#include <cstdio>

#include <cpp-kana2/core/Convert.h>

int main() {
    using namespace cpp_kana::v2;
    const auto romanised = kanaToRomaji("きって");
    if (!romanised.ok()) {
        std::printf("%s\n", codeText(romanised.error().code));
        return 1;
    }
    std::puts(romanised.value().rendered().c_str()); // kitte
    return 0;
}
```

A failure is a value (`Outcome<T>`), not an exception, so no `try`/`catch` is needed. `Conversion` also
exposes the per-unit breakdown through `moras()`. Linking is `target_link_libraries(app PRIVATE
cpp-kana2::cpp-kana2)`; the include directory is `include/`.

### C

```c
#include <stdio.h>

#include <cpp-kana2/ext/c_api.h>

int main(void) {
    cppk2_conversion *conversion = NULL;
    cppk2_error error = {0};
    const int32_t status = cppk2_kana_to_romaji("きって", NULL, &conversion, &error);
    if (status != CPPK2_OK) {
        printf("%s\n", error.detail != NULL ? error.detail : "(no detail)");
        cppk2_string_free(error.detail); /* ownership rule below */
        return 1;
    }
    printf("%s\n", conversion->rendered);
    cppk2_conversion_free(conversion);
    return 0;
}
```

Ownership rule: every allocation the library makes is freed by the library. A conversion (and every string
inside it) goes to `cppk2_conversion_free`; a detached string such as `cppk2_error.detail` goes to
`cppk2_string_free`. Calling the C runtime's `free()` on those pointers is heap corruption on Windows, not a
leak. Strings are NUL-terminated UTF-8 and failures are returned as status codes; no C++ exception crosses
the boundary.

### Python

```python
import cpp_kana2

cpp_kana2.kana_to_romaji("こんにちは").rendered
```

### C#

```csharp
using CppKana2;

var romaji = Kana.KanaToRomaji("こんにちは").Rendered;
```

### Build and test from source

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The `default` preset is a Release, shared-library build with tests and install enabled; `CMakePresets.json`
also defines `static` and `sanitize`. Using presets needs CMake 3.21 or newer, while the library itself only
needs 3.17. Build output goes under `.tmp/`.

Staging the Python package (builds the native library, writes `_version.py`, records the library's file
name):

```bash
python python/tools/build_native.py
python -m pytest python/tests -q
```

The C# suite (needs `dotnet`; it reuses the native library the Python step staged unless `--native-dir` is
given):

```bash
python csharp/tools/run_tests.py
```

## Behaviour

The romanisation is Hepburn. There is no "scheme" switch: what varies between the ways people write the same
text down is captured by the policy enums in
[include/cpp-kana2/core/Types.h](include/cpp-kana2/core/Types.h).

Default options for `kanaToRomaji` (`RomajiOptions`):

| Option | Default | Meaning |
|---|---|---|
| `sokuon` | `SokuonPolicy::Geminate` | A sokuon doubles the next consonant: きって → `kitte`, まっちゃ → `matcha` |
| `hatsuon` | `HatsuonPolicy::Apostrophe` | A hatsuon before a vowel or `y` is written `n'`: しんよう → `shin'you` |
| `longVowel` | `LongVowelPolicy::Repeat` | A long vowel repeats the previous vowel: コーヒー → `koohii` |
| `letterCase` | `CasePolicy::Lower` | Lower-case output |
| `separator` | `SeparatorPolicy::None` | Compact output with no separator between morae |
| `unknown` | `UnknownPolicy::Keep` | A character the table does not know is kept and marked per mora |

`KanaOptions` (for `romajiToKana`) defaults to `Script::Hiragana` and `UnknownPolicy::Keep`.

The policy enums and their values:

- `Script`: `Hiragana` = 0, `Katakana` = 1, `AsIs` = 2 (`AsIs` applies to `convertScript` only).
- `SokuonPolicy`: `Geminate` = 0, `Drop` = 1 (renders nothing for the sokuon).
- `HatsuonPolicy`: `Apostrophe` = 0, `Plain` = 1 (always writes `n`).
- `LongVowelPolicy`: `Repeat` = 0, `Drop` = 1 (`kohi`), `KatakanaDash` = 2 (`ko-hi-`).
- `CasePolicy`: `Lower` = 0, `Upper` = 1, `Capitalize` = 2.
- `SeparatorPolicy`: `Space` = 0, `None` = 1.
- `UnknownPolicy`: `Keep` = 0, `Ignore` = 1, `Fail` = 2 (`Fail` makes the call fail instead of marking the
  mora).

The C ABI mirrors these enums by number (`CPPK2_SOKUON_*`, `CPPK2_LONG_VOWEL_*`, ...), so a binding does not
need its own table. Per-unit results carry a `MoraKind` (`Syllable`, `Sokuon`, `Hatsuon`, `LongVowel`,
`Other`, `Archaic`, `Romaji`) and a per-unit `Code`, which is how a caller tells an ordinary syllable from a
kept unknown character without parsing the rendered string.

## Gates

The repository-wide checks are Python scripts under `tools/` and are registered by name in
[tools/run_all_gates.py](tools/run_all_gates.py):

- `check_source_list` — the explicit source lists in `CMakeLists.txt` describe `src/` and `include/` in both
  directions, so a file cannot join or leave the tree silently.
- `check_layers` — `src/` includes only point downwards through the layer table, and every `src/` file
  belongs to a layer.
- `check_public_surface` — the public headers, the C ABI symbols and the namespace-level names match the
  frozen lists in `tests/abi/`, in both directions.
- `check_bindings` — the Python and C# bindings transcribe the C ABI exactly: same entry points, same
  structures, same field order.
- `check_version_tokens` — `project(cpp-kana2 VERSION ...)` is the single authority, and the version header,
  the Python `_version.py`, the C# `version.props` and the CHANGELOG's top entry agree with it.
- `check_data_tables` — re-runs the table derivation and audits `data/moras.tsv` itself (duplicate kana,
  ambiguous aliases, lying `secondary` flags, stray carriage returns).
- `check_conformance` — pins `conformance/cases.json` by its sha256 and checks the provenance field.
- `check_doc_links` — every relative link in every Markdown file resolves (a link to a generated, gitignored
  file is a NOTE, not a failure).
- `check_doc_citations` — every `file:line` reference in the docs points at a line that still exists, so a
  cleanup that deletes code cannot leave a citation pointing at nothing.
- `check_gate_inventory` — the gate registry, the scripts on disk and the `CPP_KANA2_GATES` list in
  `tests/CMakeLists.txt` name the same gates.

Each gate is also a ctest test (`Gate_<name>`) when a Python 3 interpreter is found, and each one has a
`--self-test` that drives a synthetic failure through it. `tools/run_all_gates.py` runs the self-test before
the real check and stops the run if a self-test fails. When Python 3 is not found the gates are not
registered, and the C++ suite stays runnable on a compiler-only machine.

ctest also carries the non-Python assertions: `Smoke` and `CapiSmoke` (the C ABI smoke test is compiled as C),
`ConformanceFixture` (re-derives the fixture and compares it with the tracked bytes), `Gate_ExportGate` and its
self-test (the shared library's export table against the frozen symbol list), `Gate_SonameGate` on ELF, and
`Gate_InstallConsumer` (installs into a throwaway prefix and builds a consumer, including one that must be
refused for asking for a different major version).

## Project layout

```
include/cpp-kana2/core/   Convert.h Global.h Mora.h Types.h Utf8.h Version.h
include/cpp-kana2/ext/    c_api.h
src/infra/                UTF-8, Unicode and status-code text
src/data/                 the compiled-in mora table (generated from data/moras.tsv)
src/mora/                 normalisation, segmentation, kana -> romaji, romaji -> kana
src/core/                 the public entry points
src/c_api/                the C ABI implementation
python/cpp_kana2/         the Python package (ctypes)
python/tests/             the pytest suite
csharp/CppKana2/          the C# binding (P/Invoke)
csharp/CppKana2.Tests/    the C# suite
tests/                    cpp/ suites, abi/ frozen lists, install/ consumer check, data/corpus/
tools/                    the gates, the table generator, the conformance emitter
data/                     moras.tsv, the table's source
conformance/              cases.json + cases.sha256, the fixture all three suites read
```

## License

Apache License 2.0; see [LICENSE](LICENSE).
