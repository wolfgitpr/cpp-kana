#ifndef CPP_KANA2_CORE_UTF8_H
#define CPP_KANA2_CORE_UTF8_H

#include <string>
#include <string_view>

#include <cpp-kana2/core/Global.h>
#include <cpp-kana2/core/Types.h>

namespace cpp_kana::v2
{

    // UTF-8 is the encoding at every boundary (the C ABI, the files under data/, and all std::string in
    // these headers). Internally the library works on code points (std::u32string), not UTF-16 code units:
    // v1 used UTF-16 and treated a surrogate pair as two characters, which silently produced invalid output
    // for non-BMP input (docs/V1_DIFFERENCES.md).
    //
    // Neither function throws on bad input: the failure is an Outcome carrying Code::InvalidUtf8 and, in
    // `detail`, the byte offset where decoding stopped.

    /// Decode strictly. Rejects overlong forms, surrogates, code points above U+10FFFF and truncated
    /// sequences.
    CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<std::u32string> utf8ToCodepoints(std::string_view utf8);

    /// Encode. A code point above U+10FFFF or a surrogate makes this a Code::InvalidArgument failure rather
    /// than a replacement character: a library that silently substitutes U+FFFD in its own output hides
    /// bugs in its callers.
    CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<std::string> codepointsToUtf8(std::u32string_view codepoints);

    /// True when the bytes are well-formed UTF-8 (same rules as utf8ToCodepoints, without allocating).
    CPP_KANA2_EXPORT bool isValidUtf8(std::string_view utf8) noexcept;

} // namespace cpp_kana::v2

#endif // CPP_KANA2_CORE_UTF8_H
