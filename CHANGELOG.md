# Changelog

All notable changes to this project are documented in this file.

The format follows Keep a Changelog (Added / Changed / Deprecated / Removed / Fixed / Security), and the
package version follows Semantic Versioning. The version is not an ABI promise: 2.x is one compatibility
level, and anything outside it needs a rebuild.

## [2.0.0] - 2026-10-31

A from-scratch rewrite. The aim is deliberate behavioural difference from v1 rather than compatibility: v1's
output is not a contract, and each intentional change is documented in [docs/V1_DIFFERENCES.md](docs/V1_DIFFERENCES.md).

### Added

- New package identity, installable alongside v1: package `cpp-kana2`, headers `<cpp-kana2/...>`, C++
  namespace `cpp_kana::v2`, C symbols and typedefs prefixed `cppk2_` with enum constants and macros prefixed
  `CPPK2_`, CMake target `cpp-kana2::cpp-kana2`, and a pkg-config file.
- A C ABI (`include/cpp-kana2/ext/c_api.h`) with nine entry points, append-only status codes, `struct_size`
  on every caller-filled structure, and one ownership rule: memory allocated by the library is freed by the
  library.
- Python (`python/cpp_kana2/`, ctypes) and C# (`csharp/CppKana2/`, P/Invoke) bindings over that C ABI, plus a
  gate that compares both transcriptions with one frozen symbol list and one frozen structure layout.
- A per-unit result model — `Conversion` carries the rendered string and a `Mora` per unit, each with its kind
  (`Syllable`, `Sokuon`, `Hatsuon`, `LongVowel`, `Other`, `Archaic`, `Romaji`) and its own status code — and
  the policy options that were hard-coded in v1: `SokuonPolicy`, `HatsuonPolicy`, `LongVowelPolicy`,
  `CasePolicy`, `SeparatorPolicy`, `UnknownPolicy`, `Script`.
- A gate suite (ten checked gates) registered as ctest tests, each with a `--self-test`, plus a runner that
  proves a gate can fail before its verdict is read; a conformance fixture (`conformance/cases.json` with
  `cases.sha256`) read by the C++, Python and C# suites; and export-table, SONAME and install-consumer
  assertions registered as ctest tests.

### Changed

- Romaji-to-kana is no longer a per-token table lookup: it uses longest match, sokuon by doubled consonant,
  and explicit hatsuon and long-vowel handling. Multi-syllable input converts (`konnichiha` → こんにちは)
  where v1 returned it unchanged.
- The error model is one channel: `Outcome<T>` with a frozen `Code` 0..7. A failure is a returned value, and
  no C++ exception crosses the C ABI. v1 threw `std::invalid_argument` out of its UTF-8 decoder.
- Default output is compact (no separator) and lower-case; `SeparatorPolicy::Space` is available when the
  per-mora view is wanted as text.
- Build and install hygiene: explicit source lists instead of `file(GLOB_RECURSE)`, hidden-by-default symbol
  visibility with a checked export table, a SONAME tied to the package major version, `SameMajorVersion`
  package compatibility that refuses a cross-major request, reproducible-build flags, and an opt-in sanitizer
  build. The library and its tests still have zero third-party dependencies; the assertion harness is
  `tests/harness.h` rather than a vendored test framework.

### Fixed

- Placeholder and dropped-mark defects in the kana-to-romaji path: a sokuon no longer emits an internal
  placeholder (きって → `kitte`, がっこう → `gakkou`, まっちゃ → `matcha`), and a long-vowel mark is no longer
  silently discarded (コーヒー → `koohii`, v1: `ko hi`).
- Characters v1 mishandled are normalised before conversion: dakuten and handakuten are composed onto the base
  kana (`か` + U+309B or U+3099 followed by `く` → `gaku`), and half-width katakana (U+FF61–FF9F) is folded to
  full width instead of crashing the process.
- Duplicate keys in the reverse table no longer pick the wrong kana: `ji` → じ, `zu` → ず, `wo` → を, where v1
  answered ぢ / づ / うぉ. Canonical Hepburn spellings take precedence, and the input aliases (`si`, `hu`,
  `sya`, `dji`, `dzu`, ...) fill only spellings no canonical entry claims.
- Empty input, and text with no kana at all, are valid empty results rather than an out-of-range access.
