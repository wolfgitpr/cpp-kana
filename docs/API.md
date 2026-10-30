# cpp-kana2 API reference

Package `cpp-kana2`, version `2.0.0`, C++ namespace `cpp_kana::v2`, headers under `<cpp-kana2/...>`.
Every entry point takes and returns UTF-8. The library is stateless — the mora table is compiled in and
never mutated — so there is no engine object, no `load` phase, and concurrent calls are safe without locking.

Public headers:

| Header | Contains |
|---|---|
| `<cpp-kana2/core/Global.h>` | `CPP_KANA2_EXPORT`, `CPP_KANA2_NODISCARD` |
| `<cpp-kana2/core/Version.h>` | `CPP_KANA2_VERSION_MAJOR/MINOR/PATCH`, `CPP_KANA2_VERSION_STRING`, `CPP_KANA2_VERSION_NUMBER`, `CPP_KANA2_VERSION_AT_LEAST` |
| `<cpp-kana2/core/Types.h>` | `Code`, `Error`, `ErrorException`, `Outcome<T>`, the policy enums, `RomajiOptions`, `KanaOptions` |
| `<cpp-kana2/core/Mora.h>` | `MoraKind`, `Mora`, `Conversion` |
| `<cpp-kana2/core/Utf8.h>` | `utf8ToCodepoints`, `codepointsToUtf8`, `isValidUtf8` |
| `<cpp-kana2/core/Convert.h>` | `kanaToRomaji`, `romajiToKana`, `convertScript`, `version` |
| `<cpp-kana2/ext/c_api.h>` | the C ABI (`cppk2_*`) |

## Error model

### `Outcome<T>`

`Outcome<T>` is the only failure channel of the C++ API. It holds *either* a `T` *or* an `Error`, never
both. No entry point throws to report a conversion failure.

```cpp
template <typename T>
class Outcome {
public:
    Outcome(T value);
    Outcome(Error error);

    bool ok() const noexcept;               // true when it holds a T
    explicit operator bool() const noexcept; // same as ok()

    T &value();            // throws ErrorException when this is a failure
    const T &value() const;
    const Error &error() const;  // std::bad_variant_access when this is a success
};
```

`value()` on a failed outcome throws `ErrorException` (a `std::runtime_error` carrying the `Error` through
`error()`); its `what()` is `codeText(code)` plus `": " + detail` when the detail is non-empty. Asking for
the wrong side is a programming error, so it is thrown rather than returned as a `Code`.

`Outcome<void>` — used where a call has nothing to return — is the specialisation: default-constructing it
means success, `ok()` is `error().code == Code::Ok`, and `error()` never throws.

```cpp
struct Error {
    Code code = Code::Ok;
    std::string detail;  // human-readable context, may be empty
};
```

### `Code`

`enum class Code : std::int32_t`. The numbers are frozen: they are mirrored by the C ABI as integers, so
existing values may only be appended to, never renumbered or reused.

| Value | Member | `codeText()` | Meaning |
|---|---|---|---|
| 0 | `Code::Ok` | `ok` | no failure |
| 1 | `Code::InvalidArgument` | `invalid argument` | a parameter was unusable: null pointer on the C face, an out-of-range code point, an options `struct_size` smaller than the header's `sizeof` |
| 2 | `Code::InvalidUtf8` | `invalid UTF-8` | the input was not well-formed UTF-8 (overlong, surrogate, above U+10FFFF, truncated, bad continuation byte) |
| 3 | `Code::UnsupportedCharacter` | `unsupported character` | a character is not in the mora table; whether that fails the call depends on `UnknownPolicy` |
| 4 | `Code::NotReady` | `not ready` | engine not usable (moved from, table not loaded) |
| 5 | `Code::DataCorrupt` | `data corrupt` | the table is internally inconsistent |
| 6 | `Code::NotImplemented` | `not implemented` | a documented capability this build does not provide |
| 7 | `Code::Internal` | `internal error` | a C++ exception was caught at the boundary, or an allocation failed |

`NotReady`, `DataCorrupt` and `NotImplemented` are declared, mirrored by the C ABI and mapped by both
bindings, but no code path in this tree currently produces them. `Internal` is produced only by the C ABI
wrapper (`catch (...)`, `std::bad_alloc`, or a failed allocation while building the C view).

`codeText(Code)` is the single source of the message wording: the C function `cppk2_code_text` and both
language shells call it instead of keeping their own copy. It returns a pointer to static storage. Passing
an integer outside `0..7` to `cppk2_code_text` returns `"unknown error"`.

### How the same codes are projected

| Surface | Failure reported as | Detail text |
|---|---|---|
| C++ | `Outcome<T>` holding an `Error` | `Error::detail` |
| C ABI | `int32_t` return, `CPPK2_OK = 0` … `CPPK2_ERR_INTERNAL = 7` (same numbers) | `cppk2_error::detail`, freed with `cppk2_string_free` |
| Python | exception per code (`CppKanaError` hierarchy), with `.code` and `.detail` | rendered into the exception message |
| C# | exception per code (`CppKana2Exception` hierarchy), with `.Code` | rendered into the exception message |

## Options

There is no "scheme" switch: the romanisation is Hepburn, and what varies between the ways people write the
same text down is expressed by these policies. Member order of both option structs is part of the frozen
surface (it is mirrored field-for-field by the C ABI and both bindings).

### `RomajiOptions` — for `kanaToRomaji()`

| Member | Type | Default | Meaning |
|---|---|---|---|
| `sokuon` | `SokuonPolicy` | `SokuonPolicy::Geminate` | what a sokuon (っ) contributes: `Geminate` doubles the next consonant (きって → `kitte`, まっちゃ → `matcha`), `Drop` renders nothing (きって → `kite`) |
| `hatsuon` | `HatsuonPolicy` | `HatsuonPolicy::Apostrophe` | what a hatsuon (ん) contributes before a vowel or `y`: `Apostrophe` writes `n'` (しんよう → `shin'you`), `Plain` always writes `n` (→ `shinyou`) |
| `longVowel` | `LongVowelPolicy` | `LongVowelPolicy::Repeat` | what the ー mark contributes: `Repeat` repeats the previous rendered vowel (コーヒー → `koohii`), `Drop` writes nothing (→ `kohi`), `KatakanaDash` writes `-` (→ `ko-hi-`) |
| `letterCase` | `CasePolicy` | `CasePolicy::Lower` | output case: `Lower`, `Upper`, `Capitalize` (only the first ASCII letter of the whole result is upper-cased) |
| `separator` | `SeparatorPolicy` | `SeparatorPolicy::None` | `None` joins morae with nothing, `Space` joins them with `" "` (こんにちは → `ko nn ni chi ha`) |
| `unknown` | `UnknownPolicy` | `UnknownPolicy::Keep` | what happens to a character that is neither kana nor an ASCII letter: `Keep` renders it unchanged and records `Code::UnsupportedCharacter` on that mora, `Ignore` renders nothing, `Fail` fails the call |

### `KanaOptions` — for `romajiToKana()`

| Member | Type | Default | Meaning |
|---|---|---|---|
| `script` | `Script` | `Script::Hiragana` | target script of the produced kana; `Katakana` converts every produced kana after parsing |
| `unknown` | `UnknownPolicy` | `UnknownPolicy::Keep` | what happens to an input character that is not an ASCII letter (and to a letter run that cannot be read): `Keep` passes it through as its own unit, `Ignore` drops it, `Fail` fails the call |

### Policy enums

| Enum | Members and values |
|---|---|
| `Script` | `Hiragana` = 0, `Katakana` = 1, `AsIs` = 2 (`AsIs` is meaningful for `convertScript` only) |
| `SokuonPolicy` | `Geminate` = 0, `Drop` = 1 |
| `HatsuonPolicy` | `Apostrophe` = 0, `Plain` = 1 |
| `LongVowelPolicy` | `Repeat` = 0, `Drop` = 1, `KatakanaDash` = 2 |
| `CasePolicy` | `Lower` = 0, `Upper` = 1, `Capitalize` = 2 |
| `SeparatorPolicy` | `Space` = 0, `None` = 1 |
| `UnknownPolicy` | `Keep` = 0, `Ignore` = 1, `Fail` = 2 |

## Functions

All four are free functions in `cpp_kana::v2` (declared in `<cpp-kana2/core/Convert.h>`). The three
conversion functions are marked `CPP_KANA2_NODISCARD` and take and return UTF-8; `version()` is not.

### `kanaToRomaji`

```cpp
CPP_KANA2_NODISCARD CPP_KANA2_EXPORT
Outcome<Conversion> kanaToRomaji(std::string_view kana, const RomajiOptions &options = {});
```

Accepts hiragana, katakana, half-width katakana, and a base kana followed by a combining dakuten/handakuten.
Before segmentation the input is normalised to hiragana, so the three spellings of the same text produce the
same romanisation (`か゛く` → `gaku`, `ｶﾀｶﾅ` → `katakana`, `カタカナ` → `katakana`). Segmentation is
longest-match against the compiled-in table, so きゃ is one mora, not two.

Sokuon, hatsuon, long vowels, case and the separator are applied as described in
[Options](#options); the result is a `Conversion` whose morae are in input order.

```cpp
const auto result = kanaToRomaji("きって");
// result.ok()                == true    (the call succeeded)
// result.value().rendered()  == "kitte"
// result.value().moras()     == { き/ki/Syllable, っ/t/Sokuon, て/te/Syllable }
```

```cpp
RomajiOptions options;
options.longVowel = LongVowelPolicy::KatakanaDash;
kanaToRomaji("コーヒー", options).value().rendered();  // "ko-hi-"
```

### `romajiToKana`

```cpp
CPP_KANA2_NODISCARD CPP_KANA2_EXPORT
Outcome<Conversion> romajiToKana(std::string_view romaji, const KanaOptions &options = {});
```

Accepts the Hepburn spellings held in the table plus the input aliases the table also holds — for example
`sya` = `sha` = しゃ, `hu` = `fu` = ふ, `zi` = `ji` = じ, and `dji` / `dzu` for ぢ / づ. Matching is
longest-first, so `ni` is a syllable and not ん + い. Note that `ti` and `tu` are table entries of their own
(てぃ, とぅ), not alternative spellings of `chi` and `tsu`. ASCII letters are folded to lower case before
matching, so `KITTE` is the same request as `kitte`. A doubled consonant produces っ (`kitte`, `gakkou`),
`tch` produces っ + ち (`matcha`), and ん comes from `n'`, from `nn`, or from `n` before a consonant other
than `y`. Characters that are not ASCII letters are passed through as their own unit (or dropped, or
rejected — see `KanaOptions::unknown`). When `script` is `Katakana`, every produced kana is converted after
parsing.

```cpp
const auto kana = romajiToKana("shin'you");
// kana.value().rendered() == "しんよう"   (4 morae)
```

```cpp
romajiToKana("konnichiha!").value().rendered();  // "こんにちは!"   (6 morae; "!" is passed through)
```

Note that a letter run with no table entry is not an error under the default `UnknownPolicy::Keep`: `tya`
becomes `tや` (two morae: the passthrough `t` and や), with `Code::Ok` on both morae.

### `convertScript`

```cpp
CPP_KANA2_NODISCARD CPP_KANA2_EXPORT
Outcome<Conversion> convertScript(std::string_view text, Script target,
                                  UnknownPolicy unknown = UnknownPolicy::Keep);
```

Hiragana ↔ katakana. The input is normalised first (so `ｶﾞｯｺｳ` and `がっこう` both convert to `ガッコウ`),
then each code point of the normalised text produces exactly one mora. Characters that are not kana are
recorded as `MoraKind::Other` and kept, dropped or rejected according to `unknown`. A kept non-kana
character carries `Code::Ok` — it was passed through on purpose, so `ok()` stays true — which is the
difference from `kanaToRomaji`, where a table miss is recorded as `Code::UnsupportedCharacter`.
`Script::AsIs` converts nothing, which makes the call a normaliser.

```cpp
convertScript("こんにちは", Script::Katakana).value().rendered();   // "コンニチハ"
convertScript("ｶﾀｶﾅ", Script::Hiragana).value().rendered();        // "かたかな"
```

### `version`

```cpp
CPP_KANA2_EXPORT const char *version() noexcept;  // "2.0.0", static storage
```

The same fact is available at compile time:

```cpp
CPP_KANA2_VERSION_MAJOR   // 2
CPP_KANA2_VERSION_MINOR   // 0
CPP_KANA2_VERSION_PATCH   // 0
CPP_KANA2_VERSION_STRING  // "2.0.0"
CPP_KANA2_VERSION_NUMBER(major, minor, patch)   // (major)*10000 + (minor)*100 + (patch)
CPP_KANA2_VERSION_AT_LEAST(2, 1, 0)             // preprocessor-comparable
```

These are the package version, not an ABI version.

## Morae

### `MoraKind`

| Value | Member | Meaning |
|---|---|---|
| 0 | `MoraKind::Syllable` | an ordinary table syllable |
| 1 | `MoraKind::Sokuon` | っ / ッ |
| 2 | `MoraKind::Hatsuon` | ん / ン |
| 3 | `MoraKind::LongVowel` | the ー mark |
| 4 | `MoraKind::Other` | a character that is neither kana nor a roman letter (kanji, digits, punctuation); rendered unchanged under `UnknownPolicy::Keep` |
| 5 | `MoraKind::Archaic` | ゐ / ゑ, recognised and folded to `i` / `e` on the romaji side |
| 6 | `MoraKind::Romaji` | reserved for "a run of roman letters consumed as one unit" |

`MoraKind::Romaji` is declared and mirrored by the C ABI and both bindings, but the current implementation
does not emit it: `romajiToKana` marks table-matched units `Syllable`, pass-through characters `Other`, and
singletons っ / ん `Sokuon` / `Hatsuon`.

### `Mora`

```cpp
struct CPP_KANA2_EXPORT Mora {
    std::string text;      // the unit in the NORMALISED stream (hiragana), not the caller's spelling: カ -> か
    std::string rendered;  // what the unit contributes, policy decisions already applied
    MoraKind kind = MoraKind::Syllable;
    Code code = Code::Ok;  // per unit; UnsupportedCharacter means the table does not know it
};
```

The concatenation of `rendered` (with the requested separator) is the whole result; there is no hidden state
that changes it.

### `Conversion`

```cpp
class CPP_KANA2_EXPORT Conversion {
public:
    const std::string &text() const noexcept;       // the source text, unchanged
    const std::vector<Mora> &moras() const noexcept; // one entry per unit, in input order
    const std::string &separator() const noexcept;   // "" when the caller asked for none

    std::string rendered() const;                    // units joined with the conversion's separator
    std::string rendered(const char *separator) const; // units joined with an explicit separator

    bool ok() const noexcept;                        // every mora has Code::Ok
};
```

Both directions of a conversion produce a `Conversion`; `romajiToKana` and `convertScript` always set the
separator to `""`.

### `ok()` versus a successful call

These are two different questions:

* the **call** succeeded — the returned `Outcome` holds a `Conversion` (no `Error`);
* the **conversion** converted everything — `Conversion::ok()` is `true`, i.e. every mora has `Code::Ok`.

`UnknownPolicy::Keep` (the default for both directions) makes a call succeed while `ok()` is `false`:

```cpp
const auto result = kanaToRomaji("あ、い。う");
// result.ok()               == true      the call succeeded
// result.value().ok()       == false     、 and 。 did not convert
// result.value().rendered() == "a、i。u"
// result.value().moras()    == { あ/a/Ok, 、/、/UnsupportedCharacter, い/i/Ok, 。/。/UnsupportedCharacter, う/u/Ok }
```

The C ABI and both bindings keep the two apart in the same way: `cppk2_conversion::ok`, `Conversion.ok`
(Python) and `Conversion.Ok` (C#) are the conversion's verdict, never the status code.

## Unicode boundary

* UTF-8 is the encoding at every boundary: the C ABI, the mora table shipped under `data/`, and every
  `std::string` in the headers.
* Inside, the library works on code points (`std::u32string` / `char32_t`), not UTF-16 code units, so a
  non-BMP character is one unit and cannot be split into a surrogate pair.
* Decoding is strict: overlong forms, surrogates encoded as UTF-8, code points above U+10FFFF, truncated
  sequences and bad continuation bytes are all rejected. Nothing is replaced with U+FFFD, and no input is
  silently repaired.
* Invalid input makes the call return `Code::InvalidUtf8` with a detail naming the violation and the byte
  offset, for example `invalid UTF-8 continuation byte at byte offset 3` or
  `truncated UTF-8 sequence at byte offset 0`.
* `codepointsToUtf8` refuses a `char32_t` above U+10FFFF or a surrogate with `Code::InvalidArgument`
  (`code point out of range: ...`) rather than writing a replacement character.
* `isValidUtf8` answers the same question without allocating and without producing an `Outcome`.

```cpp
CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<std::u32string> utf8ToCodepoints(std::string_view utf8);
CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<std::string>
        codepointsToUtf8(std::u32string_view codepoints);
CPP_KANA2_EXPORT bool isValidUtf8(std::string_view utf8) noexcept;
```

## C ABI

`<cpp-kana2/ext/c_api.h>` compiles as C: it includes only `<stddef.h>`, `<stdint.h>` and
`<cpp-kana2/core/Version.h>` (which is plain `#define`s), and declares the status codes as anonymous enums.
Every symbol and enum is prefixed `cppk2_*` / `CPPK2_*`, and the export decoration is the C spelling
`CPPK2_API`, which mirrors `CPP_KANA2_EXPORT` and keys off `_WIN32` (so MinGW is covered) with
`CPP_KANA2_STATIC` selecting "no dllimport".

### Functions

| Function | Purpose |
|---|---|
| `const char *cppk2_version(void)` | package version, e.g. `"2.0.0"`; static storage, do not free |
| `const char *cppk2_code_text(int32_t code)` | wording for a status code; static storage |
| `int32_t cppk2_romaji_options_init(cppk2_romaji_options *options, size_t struct_size)` | fill defaults |
| `int32_t cppk2_kana_options_init(cppk2_kana_options *options, size_t struct_size)` | fill defaults |
| `int32_t cppk2_kana_to_romaji(const char *kana, const cppk2_romaji_options *options, cppk2_conversion **out, cppk2_error *err)` | kana → romaji |
| `int32_t cppk2_romaji_to_kana(const char *romaji, const cppk2_kana_options *options, cppk2_conversion **out, cppk2_error *err)` | romaji → kana |
| `int32_t cppk2_convert_script(const char *text, int32_t script, int32_t unknown, cppk2_conversion **out, cppk2_error *err)` | hiragana ↔ katakana |
| `void cppk2_conversion_free(cppk2_conversion *conversion)` | release a conversion and everything it owns; `NULL` is a no-op |
| `void cppk2_string_free(char *text)` | release a detached string; `NULL` is a no-op |

`options` may be `NULL` for the defaults. `err` may be `NULL` when the caller does not want the reason.
Strings are NUL-terminated UTF-8 with no length parameter; the counts a caller needs are next to the
strings (`mora_count`).

### Structures the library fills (frozen for 2.x)

```c
typedef struct cppk2_mora {
    const char *text;      /* points into the owning conversion */
    const char *rendered;  /* points into the owning conversion */
    int32_t kind;          /* CPPK2_MORA_* */
    int32_t code;          /* CPPK2_OK, or CPPK2_ERR_UNSUPPORTED_CHARACTER for a kept unknown character */
} cppk2_mora;

typedef struct cppk2_conversion {
    char *text;        /* owned by this structure */
    char *rendered;    /* units joined with the requested separator, owned by this structure */
    cppk2_mora *moras; /* array of mora_count entries, owned by this structure */
    int32_t mora_count;
    int32_t ok;        /* 1 when every mora converted */
} cppk2_conversion;

typedef struct cppk2_error {
    int32_t code;
    int32_t reserved_0; /* claims the declaration's trailing padding */
    char *detail;
} cppk2_error;
```

These never gain a member inside 2.x: a caller walks them with its own `sizeof`, so appending a field would
change the stride under binaries compiled before the change, and nothing could detect it.

### Structures the caller fills (grown with `struct_size`)

`cppk2_romaji_options` (`struct_size`, `sokuon`, `hatsuon`, `long_vowel`, `letter_case`, `separator`,
`unknown`, `reserved_1`, `reserved_2`) and `cppk2_kana_options` (`struct_size`, `script`, `unknown`, `reserved_1`,
`reserved_2`). The two reserved fields are not decoration: on a 64-bit target the struct is 40 (24) bytes and
would otherwise be 36 (20), so a newly added `int32_t` would leave `sizeof` unchanged and the `struct_size`
handshake would accept an old caller while the library read a field it never set. Reserving enough to fill
the tail padding means `sizeof` grows the moment a real field is added.

The rule, in order:

1. declare the struct and **set `struct_size = sizeof(your_struct)`**;
2. call `cppk2_*_options_init(&options, sizeof(options))` — it zeroes the struct and fills the library
   defaults, and returns `CPPK2_ERR_INVALID_ARGUMENT` when the pointer is `NULL` or `struct_size` is
   smaller than this header's `sizeof` (which is how an old binding against a newer library is refused
   instead of reading uninitialised memory);
3. change the fields you care about;
4. pass `&options` to the conversion function.

`cppk2_kana_to_romaji` / `cppk2_romaji_to_kana` re-check `struct_size` and fail with
`CPPK2_ERR_INVALID_ARGUMENT` (`"options.struct_size is smaller than this header's sizeof"`) if it is too
small. New fields are appended after the old `sizeof`, never squeezed into a gap.

### Ownership

* Everything the library allocates is freed by the library: a conversion (including `text`, `rendered`, the
  mora array and every string the morae point at) goes to `cppk2_conversion_free`; a detached string goes
  to `cppk2_string_free`.
* Do not call the C runtime's `free()` on those pointers — on Windows that is heap corruption, not a leak.
* The `text` / `rendered` pointers of a `cppk2_mora` are owned by their conversion; they stay valid until
  `cppk2_conversion_free` and must not be freed individually.

### Failure codes and the detail string

* Failures are return codes; no C++ exception crosses the boundary.
* On failure the function returns the code, leaves `*out` as a null pointer (it is set to `NULL` before any
  work starts), and — when `err` is not `NULL` — sets `err->code` and, when it has something to say,
  allocates `err->detail`: a NUL-terminated UTF-8 string the caller must release with `cppk2_string_free`.
  `err->detail` **may be `NULL`** (out of memory, or a failure with no message); treat `NULL` as "no detail".
* Each failed call allocates a fresh `detail`; the previous one is not released for you, so free it before
  reusing the same `cppk2_error` struct.
* On success `err` is not written.

```c
#include <stdio.h>
#include <string.h>

#include <cpp-kana2/ext/c_api.h>

int main(void) {
    cppk2_conversion *conversion = NULL;
    cppk2_error error;
    memset(&error, 0, sizeof(error));

    const int32_t status = cppk2_kana_to_romaji("きって", NULL, &conversion, &error);
    if (status != CPPK2_OK) {
        printf("%s: %s\n", cppk2_code_text(status), error.detail);
        cppk2_string_free(error.detail);   /* the caller frees the detail */
        return 1;
    }
    printf("%s (%d morae)\n", conversion->rendered, conversion->mora_count);  /* kitte (3 morae) */
    cppk2_conversion_free(conversion);
    return 0;
}
```

## Python and C#

Both shells are thin: they call the C ABI, copy the result into plain objects, and raise/marshal the same
codes. The option names differ in case and shape, but the values and defaults are the library's.

| C++ | Python (`python/cpp_kana2`) | C# (`csharp/CppKana2`) |
|---|---|---|
| `kanaToRomaji(text, options)` | `kana_to_romaji(text, options=None)` | `Kana.KanaToRomaji(text, options = null)` |
| `romajiToKana(text, options)` | `romaji_to_kana(text, options=None)` | `Kana.RomajiToKana(text, options = null)` |
| `convertScript(text, target, unknown)` | `convert_script(text, target="hiragana", unknown="keep")` | `Kana.ConvertScript(text, target = Script.Hiragana, unknown = UnknownPolicy.Keep)` |
| `version()` | `version()` (native version; `__version__` is the staged package version) | `Kana.Version` (property) |
| `codeText(code)` | `code_text(code)` | `Kana.CodeText(int status)` |
| `RomajiOptions` | `RomajiOptions` (frozen dataclass: `sokuon`, `hatsuon`, `long_vowel`, `case`, `separator`, `unknown` — lower-case string values or ints) | `RomajiOptions` (class: `Sokuon`, `Hatsuon`, `LongVowel`, `LetterCase`, `Separator`, `Unknown`) |
| `KanaOptions` | `KanaOptions` (`script`, `unknown`) | `KanaOptions` (`Script`, `Unknown`) |
| `Conversion` | `Conversion` (`text`, `rendered`, `moras`, `ok`; `__str__` → `rendered`) | `Conversion` (`Text`, `Rendered`, `Moras`, `Ok`; `ToString()` → `Rendered`) |
| `Mora` | `Mora` (`text`, `rendered`, `kind`, `code`, `ok`) | `Mora` (`Text`, `Rendered`, `Kind`, `Code`, `Ok`) |
| `MoraKind` | `MoraKind` (`SYLLABLE`, `SOKUON`, …) | `MoraKind` (`Syllable`, `Sokuon`, …) |
| `Script` | `Script` (`HIRAGANA`, `KATAKANA`, `AS_IS`) | `Script` (`Hiragana`, `Katakana`, `AsIs`) |
| `Code` | plain `int` (constants live in `cpp_kana2._ffi`, e.g. `_ffi.ERR_INVALID_UTF8`) | plain `int` (constants on `Status`, e.g. `Status.InvalidUtf8`) |
| `ErrorException` | `CppKanaError` and one subclass per code | `CppKana2Exception` and one subclass per code |

Both shells reject a `str`/`string` containing U+0000 with `InvalidArgumentError`
(`text must not contain U+0000`) before calling the native layer, because the C ABI takes NUL-terminated
strings and an embedded NUL would truncate silently.

Exception per status code:

| Code | Python | C# |
|---|---|---|
| 1 `InvalidArgument` | `InvalidArgumentError` | `InvalidArgumentError` |
| 2 `InvalidUtf8` | `InvalidUtf8Error` (also a `ValueError`) | `InvalidUtf8Error` |
| 3 `UnsupportedCharacter` | `UnsupportedCharacterError` | `UnsupportedCharacterError` |
| 4 `NotReady` | `NotReadyError` | `NotReadyError` |
| 5 `DataCorrupt` | `DataCorruptError` | `DataCorruptError` |
| 6 `NotImplemented` | `NotImplementedError_` (in `cpp_kana2._errors`, not re-exported in `__all__`) | no dedicated class — the base `CppKana2Exception` |
| 7 `Internal` | `InternalError` | no dedicated class — the base `CppKana2Exception` |
| any other int | `CppKanaError` (the base class, carrying `.code`) | `CppKana2Exception` (carrying `.Code`) |

Python also exposes `NativeLibraryError` (a `RuntimeError`) when the native library cannot be found or
loaded; it is raised at import time and is not a status code.
