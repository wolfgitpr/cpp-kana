#ifndef CPP_KANA2_CORE_GLOBAL_H
#define CPP_KANA2_CORE_GLOBAL_H

// CPP_KANA2_EXPORT -- the one spelling of "this symbol is part of the installed surface".
//
// TWO THINGS ARE LOAD-BEARING HERE, both learned from the v1 tree:
//
// 1. The Windows branch keys off `_WIN32`, NOT `_MSC_VER`. v1 used `_MSC_VER`, so a MinGW build fell
//    into the ELF branch, where `__attribute__((visibility("default")))` means nothing on PE; GNU ld
//    then auto-exported every global symbol because no dllexport existed. MEASURED on v1: its MinGW
//    import library carried 1082 symbols, including std::__detail regex internals.
// 2. CPP_KANA2_STATIC selects "no dllimport". A static consumer that is not told this compiles against
//    the dllimport declaration, which is wrong on MSVC and MinGW (harmless on ELF, where both spell
//    the same). The CMake target publishes the definition; the .pc file writes it out separately
//    because pkg-config has no channel for compile definitions.
#if defined(_WIN32)
#if defined(CPP_KANA2_STATIC)
#define CPP_KANA2_EXPORT
#elif defined(CPP_KANA2_LIBRARY)
#define CPP_KANA2_EXPORT __declspec(dllexport)
#else
#define CPP_KANA2_EXPORT __declspec(dllimport)
#endif
#else
#define CPP_KANA2_EXPORT __attribute__((visibility("default")))
#endif

// [[nodiscard]] spelled as a macro so a C++14 translation unit including this header still compiles.
#if defined(__cplusplus) && __cplusplus >= 201703L
#define CPP_KANA2_NODISCARD [[nodiscard]]
#else
#define CPP_KANA2_NODISCARD
#endif

#endif // CPP_KANA2_CORE_GLOBAL_H
