#ifndef CPP_KANA2_CORE_VERSION_H
#define CPP_KANA2_CORE_VERSION_H

// CPP_KANA2_VERSION_* -- the COMPILE-TIME half of the version, so a consumer can write `#if` instead of
// calling a function at run time (the run-time half is `cpp_kana::v2::version()` and `cppk2_version()`).
//
// PLAIN #define, NOT A GENERATED FILE: the public headers are installed by a directory copy and every
// gate that freezes the public face works on paths under include/, so a configure_file()-generated
// header would need an extra install rule, a binary-directory include path and exemptions from those
// gates -- a second mechanism for one file. The numbers are written here ONCE and checked against
// `project(cpp-kana2 VERSION x.y.z)` by tools/check_version_tokens.py.
//
// These are the PACKAGE version, not an ABI version: 2.x is one compatibility level and one binary
// generation (docs/STABILITY.md).

#define CPP_KANA2_VERSION_MAJOR 2
#define CPP_KANA2_VERSION_MINOR 0
#define CPP_KANA2_VERSION_PATCH 0

// String form. Keep in step with the numbers above: the same gate compares it, so a half-edit
// ("bump MAJOR, forget the string") is red rather than silently reported by version().
#define CPP_KANA2_VERSION_STRING "2.0.0"

// `#if CPP_KANA2_VERSION_AT_LEAST(2, 1, 0)` -- integer comparison, no strings, works in the
// preprocessor everywhere. No `ULL` suffix, so the same spelling is legal in C and in -Wpedantic builds.
#define CPP_KANA2_VERSION_NUMBER(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define CPP_KANA2_VERSION_AT_LEAST(major, minor, patch)                                                                \
    (CPP_KANA2_VERSION_NUMBER(CPP_KANA2_VERSION_MAJOR, CPP_KANA2_VERSION_MINOR, CPP_KANA2_VERSION_PATCH) >=            \
     CPP_KANA2_VERSION_NUMBER(major, minor, patch))

#endif // CPP_KANA2_CORE_VERSION_H
