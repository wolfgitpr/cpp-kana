#ifndef CPP_KANA2_EXT_C_API_H
#define CPP_KANA2_EXT_C_API_H

// The C ABI: what the Python (ctypes) and C# (P/Invoke) shells call, and what a C program links.
//
// THE RULES THIS HEADER IS BUILT ON (they are the contract, not conventions):
//
// 1. Strings are UTF-8 in, UTF-8 out, NUL-terminated, with no length parameter. Every count that matters
//    is available next to the string (mora_count), so a caller never has to re-scan.
// 2. Memory: everything this library allocates is freed by THIS library -- cppk2_conversion_free for a
//    conversion, cppk2_string_free for a detached string. Calling the CRT's free() on these pointers is
//    heap corruption on Windows, not a leak.
// 3. Failures are return codes. No C++ exception crosses this boundary; the only producer of
//    CPPK2_ERR_INTERNAL is a catch(...) in a wrapper.
// 4. int32_t is used everywhere instead of C's bool/int, so the widths do not depend on the compiler.
// 5. Every struct that a caller fills carries `struct_size`. Read it before writing a field, and set it
//    to sizeof(your_struct) before calling any *_init. New fields are appended AFTER the old sizeof --
//    never squeezed into a gap -- and a new field claims trailing padding only through `reserved_*`.
// 6. Structures the library FILLS (cppk2_mora, cppk2_conversion, cppk2_error) never gain a member inside
//    2.x: a caller walks them with its own sizeof, so appending a field would change the stride under
//    binaries that were compiled before the change and nothing could detect it. A new field waits for
//    3.0 or arrives through a new function.
// 7. Nothing here is thread-affine. Concurrent calls on distinct conversions are safe; the library holds
//    no mutable state, so there is no "load" phase to serialise.

#include <stddef.h>
#include <stdint.h>

#include <cpp-kana2/core/Version.h>

#ifdef __cplusplus
extern "C" {
#endif

// CPPK2_API -- the export/import decoration for this surface, kept in C spelling so c_api.h compiles as
// C. It mirrors CPP_KANA2_EXPORT (core/Global.h): the Windows branch keys off `_WIN32` so that MinGW is
// covered too, and CPP_KANA2_STATIC selects "no dllimport" for a static consumer.
#if defined(_WIN32)
#  if defined(CPP_KANA2_STATIC)
#    define CPPK2_API
#  elif defined(CPP_KANA2_LIBRARY)
#    define CPPK2_API __declspec(dllexport)
#  else
#    define CPPK2_API __declspec(dllimport)
#  endif
#else
#  define CPPK2_API __attribute__((visibility("default")))
#endif

// ---------------------------------------------------------------------------------------------------
// Status codes -- the same domain as cpp_kana::v2::Code. APPEND ONLY: a value never changes meaning
// inside 2.x, because binaries compiled against 2.0 keep reading these numbers.
// ---------------------------------------------------------------------------------------------------
enum {
    CPPK2_OK = 0,
    CPPK2_ERR_INVALID_ARGUMENT = 1,
    CPPK2_ERR_INVALID_UTF8 = 2,
    CPPK2_ERR_UNSUPPORTED_CHARACTER = 3,
    CPPK2_ERR_NOT_READY = 4,
    CPPK2_ERR_DATA_CORRUPT = 5,
    CPPK2_ERR_NOT_IMPLEMENTED = 6,
    CPPK2_ERR_INTERNAL = 7
};

// Options enums, mirrored by number so a binding does not need its own table.
enum {
    CPPK2_SCRIPT_HIRAGANA = 0,
    CPPK2_SCRIPT_KATAKANA = 1,
    CPPK2_SCRIPT_AS_IS = 2
};

enum {
    CPPK2_SOKUON_GEMINATE = 0,
    CPPK2_SOKUON_DROP = 1
};

enum {
    CPPK2_HATSUON_APOSTROPHE = 0,
    CPPK2_HATSUON_PLAIN = 1
};

enum {
    CPPK2_LONG_VOWEL_REPEAT = 0,
    CPPK2_LONG_VOWEL_DROP = 1,
    CPPK2_LONG_VOWEL_KATAKANA_DASH = 2
};

enum {
    CPPK2_CASE_LOWER = 0,
    CPPK2_CASE_UPPER = 1,
    CPPK2_CASE_CAPITALIZE = 2
};

enum {
    CPPK2_SEPARATOR_SPACE = 0,
    CPPK2_SEPARATOR_NONE = 1
};

enum {
    CPPK2_UNKNOWN_KEEP = 0,
    CPPK2_UNKNOWN_IGNORE = 1,
    CPPK2_UNKNOWN_FAIL = 2
};

// MoraKind, mirrored for the same reason.
enum {
    CPPK2_MORA_SYLLABLE = 0,
    CPPK2_MORA_SOKUON = 1,
    CPPK2_MORA_HATSUON = 2,
    CPPK2_MORA_LONG_VOWEL = 3,
    CPPK2_MORA_OTHER = 4,
    CPPK2_MORA_ARCHAIC = 5,
    CPPK2_MORA_ROMAJI = 6
};

// ---------------------------------------------------------------------------------------------------
// Structures the library fills. Frozen for 2.x (rule 6 above).
// ---------------------------------------------------------------------------------------------------

/// One mora of a conversion. `text` and `rendered` point into the conversion that owns them; they stay
/// valid until cppk2_conversion_free.
typedef struct cppk2_mora {
    const char *text;
    const char *rendered;
    int32_t kind;  ///< CPPK2_MORA_*
    int32_t code;  ///< CPPK2_OK, or CPPK2_ERR_UNSUPPORTED_CHARACTER for a kept unknown character
} cppk2_mora;

/// A conversion result. `moras` holds `mora_count` entries. `ok` is 1 when every mora converted.
typedef struct cppk2_conversion {
    char *text;        ///< the source text, owned by this structure
    char *rendered;    ///< the units joined with the requested separator, owned by this structure
    cppk2_mora *moras; ///< array of mora_count entries, owned by this structure
    int32_t mora_count;
    int32_t ok;
} cppk2_conversion;

/// An error. `detail` is an allocated, NUL-terminated UTF-8 string that the caller frees with
/// cppk2_string_free. It is NULL when the library could not allocate it (out of memory, in which case the
/// code is CPPK2_ERR_INTERNAL) or when the call failed before there was anything to say -- so a caller must
/// treat NULL as "no detail", not as a bug. cppk2_string_free(NULL) is a no-op.
typedef struct cppk2_error {
    int32_t code;
    int32_t reserved_0; ///< must be 0; a new field goes AFTER this, so the layout of `detail` cannot move
    char *detail;
} cppk2_error;

// ---------------------------------------------------------------------------------------------------
// Structures the caller fills. These grow with `struct_size` (rule 5 above).
// ---------------------------------------------------------------------------------------------------

/// Options for cppk2_kana_to_romaji. Start from cppk2_romaji_options_init, then change what you need.
typedef struct cppk2_romaji_options {
    size_t struct_size; ///< MUST be set to sizeof(cppk2_romaji_options) before *_init is called
    int32_t sokuon;     ///< CPPK2_SOKUON_*
    int32_t hatsuon;    ///< CPPK2_HATSUON_*
    int32_t long_vowel; ///< CPPK2_LONG_VOWEL_*
    int32_t letter_case;///< CPPK2_CASE_*
    int32_t separator;  ///< CPPK2_SEPARATOR_*
    int32_t unknown;    ///< CPPK2_UNKNOWN_*
    // The two reserved fields are not decoration: on a 64-bit target this struct is 8 + 8*4 = 40 bytes and
    // would be 36 without them, so the (unwritten) tail padding would be 4 bytes. A new int32 field would
    // then leave sizeof unchanged, and the struct_size handshake would accept an old caller while the
    // library read a field that caller never set. Reserving enough to fill the tail means sizeof grows the
    // moment a real field is added -- which is exactly what makes the handshake work.
    int32_t reserved_1; ///< must be 0; claimed so that adding a field changes sizeof
    int32_t reserved_2; ///< must be 0; claimed so that adding a field changes sizeof
} cppk2_romaji_options;

/// Options for cppk2_romaji_to_kana.
typedef struct cppk2_kana_options {
    size_t struct_size; ///< MUST be set to sizeof(cppk2_kana_options) before *_init is called
    int32_t script;     ///< CPPK2_SCRIPT_*
    int32_t unknown;    ///< CPPK2_UNKNOWN_*
    int32_t reserved_1; ///< must be 0; see the note on cppk2_romaji_options
    int32_t reserved_2; ///< must be 0; see the note on cppk2_romaji_options
} cppk2_kana_options;

// ---------------------------------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------------------------------

/// The package version, e.g. "2.0.0". Static storage; do not free. Same fact as CPP_KANA2_VERSION_STRING.
CPPK2_API const char *cppk2_version(void);

/// Text for a status code. Static storage; the single source of the wording (cpp_kana::v2::codeText).
CPPK2_API const char *cppk2_code_text(int32_t code);

/// Fills `options` with the library defaults. Returns CPPK2_OK, or CPPK2_ERR_INVALID_ARGUMENT when
/// `options` is NULL or `struct_size` is smaller than this header's sizeof (which is how an old binding
/// against a newer library is refused instead of reading uninitialised memory).
CPPK2_API int32_t cppk2_romaji_options_init(cppk2_romaji_options *options, size_t struct_size);

/// Fills `options` with the library defaults. Same contract as cppk2_romaji_options_init.
CPPK2_API int32_t cppk2_kana_options_init(cppk2_kana_options *options, size_t struct_size);

/// Kana -> romaji. `options` may be NULL for the defaults. On success `*out` receives a conversion the
/// caller must free with cppk2_conversion_free; on failure it is set to NULL and `err` (when non-NULL)
/// carries the reason.
CPPK2_API int32_t cppk2_kana_to_romaji(const char *kana, const cppk2_romaji_options *options, cppk2_conversion **out,
                             cppk2_error *err);

/// Romaji -> kana. Same ownership and failure contract as cppk2_kana_to_romaji.
CPPK2_API int32_t cppk2_romaji_to_kana(const char *romaji, const cppk2_kana_options *options, cppk2_conversion **out,
                             cppk2_error *err);

/// Hiragana <-> katakana. `script` is CPPK2_SCRIPT_*, `unknown` is CPPK2_UNKNOWN_*. Same contract.
CPPK2_API int32_t cppk2_convert_script(const char *text, int32_t script, int32_t unknown, cppk2_conversion **out,
                             cppk2_error *err);

/// Frees a conversion, including every string inside it. NULL is a no-op.
CPPK2_API void cppk2_conversion_free(cppk2_conversion *conversion);

/// Frees a string this library allocated (cppk2_error.detail, or a detached string). NULL is a no-op.
CPPK2_API void cppk2_string_free(char *text);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CPP_KANA2_EXT_C_API_H
