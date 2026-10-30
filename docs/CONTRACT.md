# Contract

What a caller may rely on. Written against the `v2` branch as released (tag `2.0.0`); citations are paths plus line numbers, so they survive a commit but not an edit.

The authority is the headers and the three frozen lists under [`tests/abi/`](../tests/abi/). Where this
document and a header disagree, the header wins and this document is the bug. Every claim below cites the
file it came from; nothing here is a policy that no file states.

Related documents: [`DECISIONS.md`](DECISIONS.md) (why the surface looks like this), [`STABILITY.md`](STABILITY.md)
(what a version bump may change), [`V1_DIFFERENCES.md`](V1_DIFFERENCES.md) (what deliberately changed from v1).

---

## 1. The frozen surface

Three checked-in lists describe the public surface, and
[`tools/check_public_surface.py`](../tools/check_public_surface.py) compares them against the tree **in both
directions**: a name declared but not listed is red, and a name listed but no longer declared is red.

| List | Holds | Compared with |
|---|---|---|
| [`expected_public_headers.txt`](../tests/abi/expected_public_headers.txt) | the 7 installed headers | every `*.h` under `include/` |
| [`expected_c_api_symbols.txt`](../tests/abi/expected_c_api_symbols.txt) | the 9 `cppk2_*` entry points | the `CPPK2_API` declarations in [`c_api.h`](../include/cpp-kana2/ext/c_api.h) |
| [`expected_public_names.txt`](../tests/abi/expected_public_names.txt) | the 24 namespace-level names of `cpp_kana::v2` | types and free functions in the six C++ public headers |

The lists are not a formality: the Python and C# shells transcribe this surface, so "a name was added and
nobody noticed" is a binding that is quietly incomplete ([`check_public_surface.py:1-16`](../tools/check_public_surface.py#L1-L16)).

**A change to the surface is a deliberate act.** The edit is
`python tools/check_public_surface.py --freeze`, which rewrites all three lists
([`check_public_surface.py:122-133`](../tools/check_public_surface.py#L122-L133)), and the diff of those lists
is the review ([`check_public_surface.py:10-13`](../tools/check_public_surface.py#L10-L13)). Nothing else
re-freezes them; there is no wildcard, no "allow unknown".

Two guards keep the freeze from agreeing with itself:

- the extractor asserts it still sees 18 known names, so a regression in the extractor cannot shrink the list
  into passing ([`check_public_surface.py:54-58`](../tools/check_public_surface.py#L54-L58));
- an empty or missing list is a failure, not a pass ("an empty list freezes nothing",
  [`check_public_surface.py:93-104`](../tools/check_public_surface.py#L93-L104)).

The symbol list is also measured **from the built library**, not only from the header: the `ExportGate` ctest
test reads the export table (`objdump -p` on PE, `nm` on ELF) and requires all 9 C entry points present, zero
`std::` internal symbols, and fewer than 65 exports overall
([`check_exports.cmake:103-138`](../tests/abi/check_exports.cmake#L103-L138)).

## 2. Status codes

Every C function returns `int32_t` in one domain, and the C enumerators are the same numbers as
`cpp_kana::v2::Code`:

| Value | C name | C++ name | Meaning |
|---|---|---|---|
| 0 | `CPPK2_OK` | `Code::Ok` | success |
| 1 | `CPPK2_ERR_INVALID_ARGUMENT` | `Code::InvalidArgument` | unusable parameter (NULL pointer, out-of-range enum, undersized struct) |
| 2 | `CPPK2_ERR_INVALID_UTF8` | `Code::InvalidUtf8` | input is not well-formed UTF-8 |
| 3 | `CPPK2_ERR_UNSUPPORTED_CHARACTER` | `Code::UnsupportedCharacter` | character is well-formed but not in the mora table |
| 4 | `CPPK2_ERR_NOT_READY` | `Code::NotReady` | "the engine is not usable: it was moved from, or its table did not load" — a state this implementation cannot enter (see below) |
| 5 | `CPPK2_ERR_DATA_CORRUPT` | `Code::DataCorrupt` | table internally inconsistent |
| 6 | `CPPK2_ERR_NOT_IMPLEMENTED` | `Code::NotImplemented` | a documented capability this build does not provide |
| 7 | `CPPK2_ERR_INTERNAL` | `Code::Internal` | a C++ exception was caught at the boundary |

Sources: [`Types.h:24-41`](../include/cpp-kana2/core/Types.h#L24-L41) and
[`c_api.h:54-63`](../include/cpp-kana2/ext/c_api.h#L54-L63); the C file states the reason ("APPEND ONLY: a
value never changes meaning inside 2.x, because binaries compiled against 2.0 keep reading these numbers",
[`c_api.h:51-53`](../include/cpp-kana2/ext/c_api.h#L51-L53)).

**What is frozen.** Values `0..7` never change meaning and are never renumbered. The header permits the list
to be *extended by appending* ([`Types.h:19-22`](../include/cpp-kana2/core/Types.h#L19-L22)); it does not
permit a new meaning for an old number. So a caller must treat "non-zero" as the failure test, not "equal to
one of the seven I know", and ask
[`cppk2_code_text`](../include/cpp-kana2/ext/c_api.h#L178) for the wording.

**Wording is single-sourced.** `cpp_kana::v2::codeText()` is the only place the text lives
([`Types.h:43-45`](../include/cpp-kana2/core/Types.h#L43-L45)); the C face mirrors the same table
([`c_api.cpp:119-124`](../src/c_api/c_api.cpp#L119-L124)) and both language shells call it instead of keeping
a copy ([`python/tests/test_shell.py`](../python/tests/test_shell.py) `test_code_text_comes_from_the_library`).
For a value outside `0..7`, `cppk2_code_text` answers `"unknown error"`
([`c_api.cpp:120-122`](../src/c_api/c_api.cpp#L120-L122)) — treat that as "a code this library build does
not name", not as a new condition.

**Which values are currently produced.** `Ok`, `InvalidArgument`, `InvalidUtf8`, `UnsupportedCharacter` and
`Internal` have producers in `src/`. `NotReady`, `DataCorrupt` and `NotImplemented` are defined and have
text, but no conversion path returns them today — they appear only in `codeText`'s switch
([`src/infra/CodeText.cpp`](../src/infra/CodeText.cpp)). They are reserved, not removable.

## 3. Structures

### 3.1 Structures the caller fills

`cppk2_romaji_options` and `cppk2_kana_options` begin with `struct_size` and end with two `reserved` fields
that claims the current version's trailing padding
([`c_api.h:151-168`](../include/cpp-kana2/ext/c_api.h#L151-L168)). The rule, stated in the header
([`c_api.h:16-18`](../include/cpp-kana2/ext/c_api.h#L16-L18)) and implemented in the library:

- set `struct_size = sizeof(your_struct)` before calling `cppk2_*_options_init`;
- `*_options_init` returns `CPPK2_ERR_INVALID_ARGUMENT` when the pointer is NULL or `struct_size` is smaller
  than the installed header's `sizeof` ([`c_api.cpp:107-109`](../src/c_api/c_api.cpp#L107-L109),
  [`:126-152`](../src/c_api/c_api.cpp#L126-L152));
- the three conversion functions do the same when an options pointer is non-NULL
  ([`c_api.cpp:163-165`](../src/c_api/c_api.cpp#L163-L165),
  [`:190-192`](../src/c_api/c_api.cpp#L190-L192));
- a `struct_size` **larger** than the library's `sizeof` is accepted, which is the direction that lets a
  newer caller talk to an older library.

Passing `NULL` for `options` means "library defaults" ([`c_api.h:188`](../include/cpp-kana2/ext/c_api.h#L188),
[`:194`](../include/cpp-kana2/ext/c_api.h#L194)).

### 3.2 Structures the library fills

`cppk2_mora`, `cppk2_conversion` and `cppk2_error` never gain a member inside 2.x
([`c_api.h:19-23`](../include/cpp-kana2/ext/c_api.h#L19-L23), [`c_api.h:116-144`](../include/cpp-kana2/ext/c_api.h#L116-L144)).
The reason is in the header and is mechanical: a caller walks `moras` with its own `sizeof`, so appending a
field would silently change the stride under binaries compiled before the change. A new field waits for 3.0
or arrives through a new function.

`cppk2_error.reserved_0` exists so that a future field has a documented slot after the current trailing
padding ([`c_api.h:140-144`](../include/cpp-kana2/ext/c_api.h#L140-L144)).

The field order of these three structures is transcribed, in order, by both bindings and compared
field-by-field by [`tools/check_bindings.py`](../tools/check_bindings.py); names are compared
case- and underscore-insensitively on purpose (`letter_case` in C, `LetterCase` in C#,
[`check_bindings.py:8-12`](../tools/check_bindings.py#L8-L12)).

### 3.3 The C++ option structs

`RomajiOptions` and `KanaOptions` say "member order is part of the frozen surface"
([`Types.h:177`](../include/cpp-kana2/core/Types.h#L177), [`:190`](../include/cpp-kana2/core/Types.h#L190)),
and their defaults are normative: `Geminate`, `Apostrophe`, `Repeat`, `Lower`, `None`, `Keep`
([`Types.h:178-194`](../include/cpp-kana2/core/Types.h#L178-L194)).

Note the asymmetry, which is honest rather than reassuring: the **C** structures are compared field by field
by a gate, while the C++ member order is frozen by comment only — no gate parses the C++ struct layout.

## 4. Ownership

The library frees what the library allocates, with the same allocator that allocated it, and nothing else may
free it. This is header rule 2, in those words: "Calling the CRT's free() on these pointers is heap corruption
on Windows, not a leak" ([`c_api.h:10-12`](../include/cpp-kana2/ext/c_api.h#L10-L12)).

- `cppk2_conversion_free(cppk2_conversion *)` releases the struct, `text`, `rendered`, the `moras` array and
  both strings of every mora ([`c_api.h:202-203`](../include/cpp-kana2/ext/c_api.h#L202-L203),
  [`c_api.cpp:224-238`](../src/c_api/c_api.cpp#L224-L238)). NULL is a no-op.
- `cppk2_string_free(char *)` releases a string the library allocated — `cppk2_error.detail`, or a detached
  string ([`c_api.h:205-206`](../include/cpp-kana2/ext/c_api.h#L205-L206),
  [`c_api.cpp:220-222`](../src/c_api/c_api.cpp#L220-L222)). NULL is a no-op.
- `cppk2_mora.text` and `cppk2_mora.rendered` point **into** the conversion that owns them and stay valid
  until `cppk2_conversion_free` ([`c_api.h:120-121`](../include/cpp-kana2/ext/c_api.h#L120-L121)).
- Both sides use `std::malloc`/`std::free` inside the library
  ([`c_api.cpp:22-30`](../src/c_api/c_api.cpp#L22-L30)); a binding must not reallocate or `free` them.

Two observations a caller should build in, both read off the implementation rather than the prose:

- `cppk2_error.detail` is documented as possibly NULL (out of memory, or a failure with nothing to say), and
  ([`c_api.h:138-139`](../include/cpp-kana2/ext/c_api.h#L138-L139)), but it is `malloc`'d lazily
  ([`c_api.cpp:32-38`](../src/c_api/c_api.cpp#L32-L38)); under memory exhaustion it can be NULL. Treat NULL
  `detail` as the out-of-memory path.
- `*out` is documented as set to NULL on failure, with `err` carrying the reason
  ([`c_api.h:188-190`](../include/cpp-kana2/ext/c_api.h#L188-L190)). The implementation sets it to NULL after the
  NULL-argument check ([`c_api.cpp:159`](../src/c_api/c_api.cpp#L159), [`:186`](../src/c_api/c_api.cpp#L186),
  [`:209`](../src/c_api/c_api.cpp#L209)), so on the NULL-argument path with a valid `out` the pointer is not
  written ([`c_api.cpp:156-158`](../src/c_api/c_api.cpp#L156-L158)). Initialise `*out` and only read it when the
  return value is `CPPK2_OK`.

## 5. Text and encoding

- UTF-8 in, UTF-8 out, NUL-terminated, with no length parameter; every count a caller needs sits next to the
  string (`mora_count`) (header rule 1, [`c_api.h:8-9`](../include/cpp-kana2/ext/c_api.h#L8-L9)).
- Consequently the C ABI cannot express a string containing U+0000. The Python shell refuses such input
  rather than letting it truncate ([`python/cpp_kana2/__init__.py:175-176`](../python/cpp_kana2/__init__.py#L175-L176),
  pinned by `test_embedded_nul_is_refused` in [`python/tests/test_shell.py`](../python/tests/test_shell.py)).
- Decoding is strict: overlong forms, surrogates, code points above U+10FFFF and truncated sequences are
  rejected as `Code::InvalidUtf8` with the byte offset where decoding stopped
  ([`Utf8.h:21-23`](../include/cpp-kana2/core/Utf8.h#L21-L23)).
- Empty input is a legal empty result, not an error ([`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp)
  "empty input is a legal empty result"; [`python/tests/test_shell.py`](../python/tests/test_shell.py)
  `test_empty_input_is_an_empty_result_not_an_error`).
- A call can succeed while a mora inside it did not convert: `Mora.code` is per unit, and
  `UnknownPolicy::Keep` renders the unknown character as-is and records
  `Code::UnsupportedCharacter` on that mora ([`Mora.h:31-47`](../include/cpp-kana2/core/Mora.h#L31-L47)).
  `Conversion::ok()` is true only when every mora converted ([`Mora.h:72-73`](../include/cpp-kana2/core/Mora.h#L72-L73)).
  The conformance fixture keeps the two facts apart: `status` is the call result, `ok` is the conversion's
  ([`conformance/README.md:29-46`](../conformance/README.md#L29-L46)).

## 6. Determinism

Same input and same option values produce the same bytes, in every language shell that replays the fixture. The
properties that make this checkable rather than hoped for:

- **No locale.** Nothing under `src/` or `include/` includes `<clocale>`/`<locale>`, calls `setlocale`, uses
  `std::locale`, or uses `tolower`/`toupper`; ASCII case folding is arithmetic
  ([`RomajiToKana.cpp:119`](../src/mora/RomajiToKana.cpp#L119), [`:153`](../src/mora/RomajiToKana.cpp#L153);
  [`Convert.cpp:36`](../src/core/Convert.cpp#L36)).
- **No environment.** The library reads no environment variable (no `getenv` anywhere in `src/` or
  `include/`).
- **No mutable state.** The only `static` in the library is a function-local `const` array
  ([`Normalize.cpp:91`](../src/mora/Normalize.cpp#L91)); the tables are `constexpr`
  ([`TableData.inc:8`](../src/data/TableData.inc#L8), [`Unicode.cpp:21`](../src/infra/Unicode.cpp#L21),
  [`:74`](../src/infra/Unicode.cpp#L74)). There is no mutex, no atomic and no `thread_local`.
- **The lock.** [`conformance/cases.json`](../conformance/cases.json) holds 604 cases (`schema` 1,
  `engine_version` `"2.0.0"`) and is pinned by sha256 in
  [`conformance/cases.sha256`](../conformance/cases.sha256):
  `0ccde5eb0d19ef3a5f8cf12cd0d49e9b9cf5a1575efbd0113b3da5613e647ebd` (verified with `Get-FileHash`).
  [`tools/check_conformance.py`](../tools/check_conformance.py) verifies the digest, cross-checks
  `cases_count` against the array, and checks the provenance field
  ([`check_conformance.py:35-63`](../tools/check_conformance.py#L35-L63)).
- **Who reads it.** The Python and C# suites replay `cases.json` itself
  ([`python/tests/test_conformance.py`](../python/tests/test_conformance.py),
  [`csharp/CppKana2.Tests/Program.cs:149`](../csharp/CppKana2.Tests/Program.cs#L149)); the C++ suite replays
  `tests/data/corpus/expected.tsv`, produced from the same corpus by the same generator
  ([`conformance/README.md:10-16`](../conformance/README.md#L10-L16)). The `ConformanceFixture` ctest test
  re-derives both files and compares the bytes ([`tests/CMakeLists.txt:56-61`](../tests/CMakeLists.txt#L56-L61)).
- **Line endings.** `conformance/cases.json` and `conformance/cases.sha256` are pinned to LF, and the data
  files (`data/**`, `tests/data/**`) are marked `-text` so git does not normalise them at all
  ([`.gitattributes`](../.gitattributes)); `tools/check_data_tables.py` separately rejects a CR in
  `moras.tsv` ([`check_data_tables.py:79-81`](../tools/check_data_tables.py#L79-L81)) — the pin is over bytes.

## 7. Threads

Header rule 7 is the contract: "Nothing here is thread-affine. Concurrent calls on distinct conversions are
safe; the library holds no mutable state, so there is no load phase to serialise"
([`c_api.h:23-24`](../include/cpp-kana2/ext/c_api.h#L23-L24)). The C++ side states the same thing and names
this document as the place it is a contract rather than an implementation detail
([`Convert.h:15-20`](../include/cpp-kana2/core/Convert.h#L15-L20)).

What backs that up: the library has no mutable global state (see §6), every allocation is per call, and the
mora table is compiled in. What is **not** promised, and is not in the header:

- no statement about using **one** conversion object from several threads at once (read it from one thread,
  or serialise your access);
- no locking, and no internal thread pool — "safe" here means "no shared mutable state", nothing more;
- no claim of async-signal-safety.

The only test of this is a smoke check, not a race detector: four threads run 200 conversions each and must
all produce `gakkou` ([`python/tests/test_shell.py`](../python/tests/test_shell.py)
`test_conversions_are_independent_across_threads`).

## 8. What is explicitly not promised

- **Compatibility with v1.** Not a goal, by decision ([`DECISIONS.md`](DECISIONS.md) D3). v1's outputs are not
  a reference; [`V1_DIFFERENCES.md`](V1_DIFFERENCES.md) is the itemised list of what changed and why.
- **Romanisation-scheme switching.** Only Hepburn is implemented; there is no scheme registry, no plugin
  surface and no user-defined scheme ([`DECISIONS.md`](DECISIONS.md) D1, D8). What varies is captured by the
  six policy enums in [`Types.h:126-175`](../include/cpp-kana2/core/Types.h#L126-L175).
- **Binary compatibility across major versions.** The package promises API compatibility inside a major
  version and never binary compatibility ([`CMakeLists.txt:319-328`](../CMakeLists.txt#L319-L328)); the ELF
  SONAME is `libcpp-kana2.so.2` and is explicitly not a binary-compatibility promise
  ([`CMakeLists.txt:126-130`](../CMakeLists.txt#L126-L130)).
- **Reproducible binaries.** The build sets reproducibility flags for all four toolchains
  ([`CMakeLists.txt:45-84`](../CMakeLists.txt#L45-L84)), but no gate compares two builds, so byte-identical
  output across rebuilds is a build setting here, not a promise this document can make.
- **Performance numbers.** No performance figure for this library is stated in the repository, so none is
  promised.
- **Diagnostic wording.** `Error.detail` strings are diagnostics. The only wording that is pinned anywhere is
  the `codeText` table (§2) and one substring (`"byte offset"`) asserted by the Python suite.
- **Codes outside `0..7`.** §2 says what they mean to this build: "unknown error".

## 9. How each promise is checked

| Promise | Enforced by |
|---|---|
| surface is exactly the three lists | `tools/check_public_surface.py` (both directions, `--freeze` to change) |
| the binary exports exactly the C entry points | `tests/abi/check_exports.cmake` (`ExportGate`, `ExportGateSelfTest`) |
| the bindings transcribe the surface | `tools/check_bindings.py`; `python/tests/test_contract.py` |
| status codes and their text | `tests/abi/expected_c_api_symbols.txt`; `cpp_kana::v2::codeText`; `python/tests/test_contract.py::test_status_constants_match_the_header` |
| struct layouts | `tools/check_bindings.py` (field order); `python/tests/test_contract.py::test_struct_layouts_match_the_header` |
| same input → same bytes | `ConformanceFixture` ctest test; `tools/check_conformance.py`; the three language suites |
| same version everywhere | `tools/check_version_tokens.py` |
| install surface usable, cross-major refused | `tests/install/check_install_consumer.cmake` (`Gate_InstallConsumer`) |
| ELF SONAME carries the major | `tests/abi/check_soname.cmake` (`Gate_SonameGate`, ELF only) |
| every gate is wired into ctest and into the runner | `tools/check_gate_inventory.py` |

One command for every Python gate, with a single exit code, self-tests before verdicts:
`python tools/run_all_gates.py` ([`run_all_gates.py:1-11`](../tools/run_all_gates.py#L1-L11)). The three
CMake gates (`ExportGate`, `SonameGate`, `InstallConsumer`) are ctest tests and need a build tree.
