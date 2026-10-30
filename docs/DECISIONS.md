# Decisions

The decision ledger. **D1–D16** are reproduced from the plan's ledger
([`plans/cpp-kana2-v2-refactor.md` §15](plans/cpp-kana2-v2-refactor.md)), which is the source for the rulings the
user made on 2026-10-31 (D1–D4) and for the design decisions taken with them. **D17 onward** are decisions
that were settled later, during implementation, and are recorded here from the code and the commit that made
them — each one is verifiable at the paths it cites.

Written against the `v2` branch as released (tag `2.0.0`); citations are paths plus line numbers, so they survive a commit but not an edit.

Format: **decision** — why — where it is enforced. "Enforced" means a gate, a test, or a header statement; when
nothing enforces a decision mechanically, this document says so instead of implying coverage.

---

## D1 — "multi-language" means C ABI + Python + C#

**Decision.** The three shells are the only multi-language surface. The core keeps exactly one romanisation
(Hepburn) plus policy switches; there is no romanisation-scheme registry, no plugin surface, no user-defined
scheme.
**Why.** The project owner decided the scope: a C ABI, plus Python and C# shells. The plan records the consequence
explicitly: `include/cpp-kana2/schemes/`, `ext/RomajiScheme.h`, `ext/Registry.h`, `data/schemes/<id>/` are not
built, and what stays are the six policy enums — knobs, not extension points.
**Enforced at.** [`Types.h:118-124`](../include/cpp-kana2/core/Types.h#L118-L124) ("there is deliberately no
'scheme' dimension"); [`tests/abi/expected_public_names.txt`](../tests/abi/expected_public_names.txt) — no
`Registry`/`Scheme`-shaped name may appear without a `--freeze`; `tools/check_public_surface.py` fails on any
added name.

## D2 — a new package, `cpp-kana2` 2.0.0

**Decision.** New package identity: headers `<cpp-kana2/...>`, namespace `cpp_kana::v2`, C symbols `cppk2_*`,
CMake package `cpp-kana2`, version 2.0.0. It coexists with v1 on one machine.
**Why.** A rewrite that deliberately breaks behaviour must not be installable under the v1 name.
**Enforced at.** [`CMakeLists.txt:6`](../CMakeLists.txt#L6) (`project(cpp-kana2 VERSION 2.0.0)`);
[`Global.h:16-26`](../include/cpp-kana2/core/Global.h#L16-L26) (`CPP_KANA2_*` macros);
[`c_api.h:54-63`](../include/cpp-kana2/ext/c_api.h#L54-L63) (`cppk2_*`); `tools/check_version_tokens.py`;
`tools/check_public_surface.py`; `Gate_InstallConsumer`.

## D3 — no v1 behaviour compatibility; rewrite to the Japanese romanisation rules

**Decision.** v1's output is not a contract. v2 is written to the Hepburn conventions and ships an itemised
diff instead of an equivalence gate.
**Why.** The plan's diagnosis (§2) found v1 wrong in ways a compatible rewrite would have to preserve:
`ji`→ぢ, `zu`→づ, っ rendered as a literal `cl`, ー dropped, half-width katakana crashing (0xC0000005).
**Enforced at.** Not mechanically — that is the point of the decision. The new behaviour is pinned by name in
[`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) ("kana to romaji: the cases v1 got wrong") and by
[`conformance/cases.json`](../conformance/cases.json); the diff itself is
[`V1_DIFFERENCES.md`](V1_DIFFERENCES.md).

## D4 — build and test locally; Linux replay on demand

**Decision.** The development machine may run configure/build/ctest/pytest/dotnet; Linux verification happens
on the firefly board when needed.
**Why.** The plan's phase table closes every phase on a measured gate rather than on inspection (§12, column
"完成判据").
**Enforced at.** Process, not code. The CI matrix is 8 jobs — `linux-gcc`, `linux-clang`, `linux-sanitize`,
`linux-static`, `windows-msvc`, `windows-mingw`, `python`, `csharp`
([`.github/workflows/ci.yml`](../.github/workflows/ci.yml)) — and the gates ride along as ctest tests.

## D5 — a small, hand-picked gate set instead of the reference project's 27

**Decision.** Few gates, each proven able to fail, rather than many gates nobody has seen fail.
**Why.** The gate runner states the reasoning: "A gate that only ever prints OK is worse than no gate: it costs
maintenance and buys false confidence" ([`run_all_gates.py:4-7`](../tools/run_all_gates.py#L4-L7)). The plan's
ledger records the count itself as revisitable ("本方案，可回退").
**Enforced at.** [`tools/run_all_gates.py:27-36`](../tools/run_all_gates.py#L27-L36) registers the gates and runs
each `--self-test` **before** its verdict; [`tools/check_gate_inventory.py`](../tools/check_gate_inventory.py)
compares registry ↔ scripts ↔ `tests/CMakeLists.txt`, so an unwired gate is red.
**Where the implementation differs from the plan's wording.** The plan said "10 gates". What exists is 10 Python
gates in the registry (`check_source_list`, `check_layers`, `check_public_surface`, `check_bindings`,
`check_version_tokens`, `check_data_tables`, `check_conformance`, `check_doc_links`, `check_doc_citations`,
`check_gate_inventory`) plus 3 CMake gates
(`Gate_ExportGate`, `Gate_SonameGate` on ELF only, `Gate_InstallConsumer`), the last three being ctest tests
rather than entries in `run_all_gates.py`.

## D6 — internally everything is `char32_t`; UTF-8 only at the boundary

**Decision.** Code points internally (`std::u32string`), UTF-8 at every boundary.
**Why.** v1 iterated a `u16string` as if it held `char32_t` and split surrogate pairs into two bogus code
points; the plan's C8.
**Enforced at.** [`Utf8.h:13-16`](../include/cpp-kana2/core/Utf8.h#L13-L16);
[`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) "UTF-8 round-trips, including non-BMP".

## D7 — `Outcome<T>` is the only error channel, `Code` 0..7 is frozen

**Decision.** Failures are values. `Outcome<T>` carries "a T or an Error". The eight `Code` values are frozen;
an exception is only for asking for the wrong side of the outcome.
**Why.** A C ABI cannot carry exceptions, and a binding must keep reading the same numbers for the same
conditions.
**Enforced at.** [`Types.h:16-41`](../include/cpp-kana2/core/Types.h#L16-L41),
[`:69-116`](../include/cpp-kana2/core/Types.h#L69-L116);
[`c_api.h:13-14`](../include/cpp-kana2/ext/c_api.h#L13-L14) (no exception crosses the boundary);
[`c_api.h:51-63`](../include/cpp-kana2/ext/c_api.h#L51-L63) (same numbers); `tools/check_public_surface.py`
freezes the names; [`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) "invalid UTF-8 is a value, not an exception"
and "asking for the wrong side of an Outcome throws".

## D8 — no virtual-function plugin surface

**Decision.** No virtual interface, no `Registry`, no `dlopen` plugin path. The plan's wording is "no plugins
this time, so nothing is involved".
**Why.** The ledger records the ruling as "no virtual-function plugin surface (there are no plugins this time, so
nothing is involved)", sourced from a lesson in the reference project.
**Enforced at.** The tree: no `virtual` appears in `include/` or `src/`; the only mention of "scheme" is the
comment that says there is no such dimension ([`Types.h:121`](../include/cpp-kana2/core/Types.h#L121)).
`tools/check_public_surface.py` would report any new namespace-level type.

## D9 — the data generator and its output are both committed; a gate checks that they agree

**Decision.** `tools/gen_moras.py` writes `data/moras.tsv` (reviewable) and `src/data/TableData.inc` (the C++
initialiser); both are in the repository, and a gate runs the generator in check mode.
**Why.** Generated data that is only generated is invisible in review; data that is only committed drifts from
its generator.
**Enforced at.** `tools/check_data_tables.py` runs `gen_moras.py --check` and then audits the TSV
([`check_data_tables.py:84-90`](../tools/check_data_tables.py#L84-L90),
[`:41-81`](../tools/check_data_tables.py#L41-L81)).

## D10 — the default policies: `Repeat`, `Geminate`, `Apostrophe`

**Decision.** `LongVowelPolicy::Repeat` (`コーヒー`→`koohii`), `SokuonPolicy::Geminate` (`きって`→`kitte`),
`HatsuonPolicy::Apostrophe` (`しんよう`→`shin'you`).
**Why.** These keep the information the source text carries, in pure ASCII. v1 dropped the long-vowel mark
entirely.
**Enforced at.** [`Types.h:178-188`](../include/cpp-kana2/core/Types.h#L178-L188) (the member initialisers);
`CPPK2_LONG_VOWEL_*`/`CPPK2_SOKUON_*`/`CPPK2_HATSUON_*` in [`c_api.h:72-86`](../include/cpp-kana2/ext/c_api.h#L72-L86);
the `default` option variant in [`conformance/cases.json`](../conformance/cases.json);
[`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) "kana to romaji: sokuon and hatsuon policies".

## D11 — a self-written test harness, not a bundled framework

**Decision.** [`tests/harness.h`](../tests/harness.h) instead of Catch2 (the plan's reasoning: Catch2 is a 25k-line
single header with its own licence and a format-gate exemption, against a library of a few thousand lines).
**Why.** No third-party dependency, including in the tests.
**Enforced at.** The tree: there is no vendor directory and no third-party header under `include/`; CI states
the position plainly ("The library has no third-party dependency, so every job is a compiler plus CMake",
[`.github/workflows/ci.yml:7-8`](../.github/workflows/ci.yml#L7-L8)).
**Where the implementation differs.** The plan estimated "~90 lines"; the file is 112 lines. The estimate was
not a contract.

## D12 — `BUILD_SHARED_LIBS` defaults ON; a static consumer defines `CPP_KANA2_STATIC`

**Decision.** Shared by default; a static build defines `CPP_KANA2_STATIC` publicly, and the `.pc` file writes
the same definition out because pkg-config has no channel for compile definitions.
**Why.** Without the definition a static consumer compiles against the `dllimport` declarations.
**Enforced at.** [`CMakeLists.txt:15`](../CMakeLists.txt#L15), [`:136-141`](../CMakeLists.txt#L136-L141),
[`:354-359`](../CMakeLists.txt#L354-L359); [`Global.h:12-15`](../include/cpp-kana2/core/Global.h#L12-L15);
`-DCPP_KANA2_STATIC` in [`cpp-kana2.pc.in`](../cpp-kana2.pc.in); the `linux-static` CI job.

## D13 — C# targets `net7.0;net10.0`

**Decision.** Two target frameworks, both verified on the development machine (SDK 7.0.302 and 10.0.402 per the
plan).
**Why.** One framework would either drop older runtimes or leave the current one untested.
**Enforced at.** [`csharp/CppKana2/CppKana2.csproj:7`](../csharp/CppKana2/CppKana2.csproj#L7);
[`csharp/CppKana2.Tests/CppKana2.Tests.csproj`](../csharp/CppKana2.Tests/CppKana2.Tests.csproj).

## D14 — Python 3.9+, native library staged at build time

**Decision.** `python_requires=">=3.9"`; the wheel is built from a native library that `tools/build_native.py`
stages into `cpp_kana2/_native`, recording its real file name in `name.txt` so nothing hard-codes a library
name.
**Why.** A hard-coded name is wrong on every platform but the one it was typed on.
**Enforced at.** [`python/setup.py:84`](../python/setup.py#L84); [`python/tools/build_native.py`](../python/tools/build_native.py);
[`python/cpp_kana2/_native/name.txt`](../python/cpp_kana2/_native/name.txt);
[`python/cpp_kana2/_native.py`](../python/cpp_kana2/_native.py).

## D15 — no environment variables; no implicit data location

**Decision.** The library reads no environment variable.
**Why.** Configuration that arrives through the environment is invisible at the call site and untestable in
combination.
**Enforced at.** No `getenv` in `src/` or `include/`.
**Where the implementation went further than the plan.** The plan's second half was "the data directory is
only a constructor parameter". There is no engine object and no data-directory parameter at all: the mora table
is compiled in ([`src/data/TableData.inc`](../src/data/TableData.inc)), and `data/` is installed as
read-only data for consumers to read ([`CMakeLists.txt:339-344`](../CMakeLists.txt#L339-L344)). K8's
`data/format.txt` (a data-format generation marker) does not exist in the tree.

## D16 — v1's `doubleWrittenSokuon` semantics are not kept in any form

**Decision.** The v1 parameter is gone; sokuon is handled by `SokuonPolicy`.
**Why.** In v1 the parameter was a dead branch whose effect was to delete っ rather than double it (plan C3;
`V1_DIFFERENCES.md` §1 records that `true` and `false` produce identical output).
**Enforced at.** No `doubleWrittenSokuon` appears in `include/`, `src/`, `tools/`, `tests/`, `python/`,
`csharp/` or `conformance/`; the behaviour is pinned by
[`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) "kana to romaji: sokuon and hatsuon policies".

---

## Later decisions

Settled during implementation (P1–P7) and recorded from the code and the commit that introduced them.

## D17 — the library is stateless free functions, not an engine object

**Decision.** `kanaToRomaji`, `romajiToKana`, `convertScript`, `version` are free functions in
`cpp_kana::v2`; there is no `Engine` class, no `open`/`destroy` on the C face.
**Why.** The mora table is compiled in and never mutated, so an object would carry no state — only a lifetime
for the caller to get wrong. It also makes "concurrent calls need no locking" true by construction.
**Enforced at.** [`Convert.h:15-20`](../include/cpp-kana2/core/Convert.h#L15-L20); the C ABI has nine functions
and none of them creates a context ([`c_api.h:174-206`](../include/cpp-kana2/ext/c_api.h#L174-L206));
`tools/check_layers.py` has no `engine` layer ([`check_layers.py:25-31`](../tools/check_layers.py#L25-L31)).
Supersedes the plan's `core/Engine.h` + `engine/EngineImpl.cpp` sketch.

## D18 — the default separator is `None` (compact)

**Decision.** `RomajiOptions::separator = SeparatorPolicy::None`; `SeparatorPolicy::Space` remains available.
**Why.** A romanisation is usually pasted into a file name or a URL; the spaced form (`ga k ko u`) was an
artefact of rendering morae one at a time. Found while writing the Python shell (commit `eca961e`).
**Enforced at.** [`Types.h:183-186`](../include/cpp-kana2/core/Types.h#L183-L186); the `default` option variant
in [`conformance/cases.json`](../conformance/cases.json);
[`tests/cpp/smoke.cpp`](../tests/cpp/smoke.cpp) "kana to romaji: case and separator policies".

## D19 — the fixture records both `status` and `ok`

**Decision.** Each conformance case carries `status` (the call's return code) and `ok` (whether every mora
converted).
**Why.** They differ exactly when an unknown character is kept: `これでもやれるだけ飞ばしてきたんだよ` succeeds as a call
and is not ok as a conversion. A shell that reported one as the other would claim a kanji converted
(commit `eca961e`).
**Enforced at.** [`conformance/README.md:29-46`](../conformance/README.md#L29-L46);
[`tools/conformance_emit.cpp`](../tools/conformance_emit.cpp); both shells replay the fields.

## D20 — `generated_by_commit` is recorded only when asked for

**Decision.** The fixture's provenance field is filled only when the generator is given `--commit`; otherwise
it says `"unknown"`, and the gate prints a note instead of failing.
**Why.** Baking the commit in at configure time made every commit invalidate the fixture, which teaches people
to regenerate blindly (commit `0f7a216`).
**Enforced at.** [`conformance/cases.json`](../conformance/cases.json) (currently `"unknown"`);
[`check_conformance.py:53-62`](../tools/check_conformance.py#L53-L62) (hex id + "is a commit this repository
contains").

## D21 — the public headers are `Convert.h` and `Utf8.h`

**Decision.** Six `core/` headers plus `ext/c_api.h`, i.e. `Convert`, `Global`, `Mora`, `Types`, `Utf8`,
`Version`, and the C header.
**Why.** The plan's sketch had `Engine.h` and `U16Str.h`; with the stateless free-function design (D17) the
entry points need a home and UTF-8 needs a public one, while an engine header and a UTF-16 string type have
nothing to declare.
**Enforced at.** [`tests/abi/expected_public_headers.txt`](../tests/abi/expected_public_headers.txt) compared
with the tree by `tools/check_public_surface.py`.

## D22 — the table declares its own collisions (`secondary`)

**Decision.** `data/moras.tsv` has five columns — kana, romaji, aliases, kind, `secondary` — and a row may
share its canonical romaji with an earlier row only if it sets `secondary=1` (ぢ/じ both spell `ji`, うぉ/を both
spell `wo`). The generator refuses a duplicate kana, a duplicate canonical romaji, and an alias that equals
another kana's canonical spelling.
**Why.** Reverse lookup must be a rule, not an accident of table order. v1 had the same four collisions as
duplicate `unordered_map` keys and silently kept the first (commit `db99840`).
**Enforced at.** [`tools/check_data_tables.py:41-81`](../tools/check_data_tables.py#L41-L81);
[`tools/gen_moras.py`](../tools/gen_moras.py); [`tests/cpp/data_table.cpp`](../tests/cpp/data_table.cpp)
("every table row renders to its own romaji", "every primary table row survives a round trip", "every accepted
alias reaches its own kana").

## D23 — the Python wheel's native library links the MinGW runtime statically

**Decision.** On MinGW, the DLL that ships in the wheel is linked with `-static -static-libgcc -static-libstdc++`.
**Why.** Measured: a MinGW-built DLL depends on `libgcc_s_seh-1.dll` and `libstdc++-6.dll`, so `ctypes` fails
with "could not find module ... (or one of its dependencies)" on every machine without MinGW on `PATH`
(commit `eca961e`).
**Enforced at.** [`python/tools/build_native.py:81-86`](../python/tools/build_native.py#L81-L86).

## D24 — C# P/Invoke is `Cdecl` + `ExactSpelling`, and native memory never escapes the call

**Decision.** Every `DllImport` states `CallingConvention = CallingConvention.Cdecl` and
`ExactSpelling = true`; the conversion's native memory is copied out and released before the object is handed
to the caller.
**Why.** The Windows default is `stdcall`, which corrupts the stack on the first call; and an object that
holds a native pointer is an object that can be used after free (commit `60ac381`).
**Enforced at.** [`csharp/CppKana2/NativeMethods.cs:8`](../csharp/CppKana2/NativeMethods.cs#L8) and the
`DllImport` attributes below it; `tools/check_bindings.py` compares the extern set with the frozen symbols.

## D25 — the export table is measured from the binary, with a cap as a smell test

**Decision.** The export gate requires all 9 C entry points, zero standard-library internal symbols, and fewer
than 65 exports overall.
**Why.** v1's MinGW DLL exported 269 symbols, 226 of them `std::__detail`/regex internals, because visibility
attributes were believed instead of measured. The cap is not a target; it is a tripwire.
**Enforced at.** [`tests/abi/check_exports.cmake:103-138`](../tests/abi/check_exports.cmake#L103-L138)
(`Gate_ExportGate`, with `Gate_ExportGateSelfTest` proving it can report a missing symbol).

## D26 — the ELF SONAME carries the major version, and is checked only where it exists

**Decision.** `SOVERSION = PROJECT_VERSION_MAJOR` → `libcpp-kana2.so.2`; the gate runs on ELF only and reports
PE/Mach-O as skipped rather than passed.
**Why.** PE has no SONAME and Mach-O uses a compatibility version instead, so a Windows-only "pass" would be a
lie about a check that never ran — the gate says so and reports the platform as skipped.
**Enforced at.** [`CMakeLists.txt:126-135`](../CMakeLists.txt#L126-L135);
[`tests/abi/check_soname.cmake`](../tests/abi/check_soname.cmake); registration in
[`tests/CMakeLists.txt:121-128`](../tests/CMakeLists.txt#L121-L128).

## D27 — `SameMajorVersion`, and a cross-major request must be refused

**Decision.** The package config uses `COMPATIBILITY SameMajorVersion`, and the install gate builds a consumer
that asks for 3.0 and requires `find_package` to reject it.
**Why.** Under `AnyNewerVersion` a 1.x request was satisfied by a 2.x install: configure succeeded, the compile
then failed on headers that no longer exist ([`CMakeLists.txt:309-313`](../CMakeLists.txt#L309-L313)).
**Enforced at.** [`CMakeLists.txt:314-318`](../CMakeLists.txt#L314-L318);
[`tests/install/check_install_consumer.cmake:132-146`](../tests/install/check_install_consumer.cmake#L132-L146).

## D28 — the conformance fixture is a lock, not an oracle

**Decision.** `conformance/cases.json` and `tests/data/corpus/expected.tsv` are generated from
`tests/data/corpus/cases.tsv`; `--check` re-derives both and compares bytes; an intended change re-pins them.
**Why.** "Correct" is decided by `V1_DIFFERENCES.md` and the hand-written cases. The fixture exists so the
three language shells cannot drift apart, and so an engine change is visible as a diff (commit `0f7a216`).
**Enforced at.** [`conformance/README.md:1-28`](../conformance/README.md#L1-L28); the `ConformanceFixture`
ctest test ([`tests/CMakeLists.txt:56-61`](../tests/CMakeLists.txt#L56-L61));
[`tools/check_conformance.py`](../tools/check_conformance.py) for the sha256 pin.

---

## Deviations from the plan that are not decisions

Recorded so a reader does not have to rediscover them. None of these was ruled on; each is simply what the
tree says today.

- `data/format.txt` (plan K8) does not exist; `data/` contains only `moras.tsv`.
- The plan's C ABI sketch named about 20 symbols including `cppk2_engine_open`/`_destroy`,
  `cppk2_analyze`, `cppk2_mora_free` and `cppk2_scheme_name`. The frozen list has nine
  ([`tests/abi/expected_c_api_symbols.txt`](../tests/abi/expected_c_api_symbols.txt)).
- `CHANGELOG.md` now exists and its top entry is `## [2.0.0] - 2026-10-31`, so
  `tools/check_version_tokens.py` compares that projection too
  ([`check_version_tokens.py:83-88`](../tools/check_version_tokens.py#L83-L88)).
- The version authority is documented in [`docs/STABILITY.md`](../docs/STABILITY.md), which is what
  [`CMakeLists.txt:4`](../CMakeLists.txt#L4) and [`python/setup.py:11`](../python/setup.py#L11) point at. `docs/`
  holds `API.md`, `BOARD_REPLAY.md`, `CONTRACT.md`, `DECISIONS.md`, `QUICKSTART.md`, `README.md`, `STABILITY.md`,
  `V1_DIFFERENCES.md` and `plans/`; the plan's other documents (REQUIREMENTS, PERFORMANCE, API_zh) are not there.
- CI has no macOS job, although the plan's G4 lists macOS/Clang in the platform matrix.
- (Resolved before the 2.0.0 tag: `.gitattributes` no longer pins a `vendor/LICENSE.txt` that does not exist.)
