#ifndef CPP_KANA2_MORA_INTERNAL_H
#define CPP_KANA2_MORA_INTERNAL_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <cpp-kana2/core/Mora.h>
#include <cpp-kana2/core/Types.h>

namespace cpp_kana::v2::internal
{

    // Internal interfaces between the conversion stages. Nothing here is exported; src/ is not installed.
    //
    // Forward:  utf8 -> codepoints -> normalise -> segment -> render
    // Reverse:  utf8 -> codepoints -> parse runs -> kana

    /// Folds half-width katakana, combining and precomposed voicing marks, half-width voicing marks and
    /// katakana to hiragana, so every later stage sees exactly one spelling of each mora.
    Outcome<std::u32string> NormalizeToHiragana(std::u32string_view codepoints);

    /// Splits normalised text into morae: a base kana plus at most one small kana (きゃ), or a single special
    /// unit (っ / ー / ん). Non-kana characters come back as one-code-point morae of their own.
    std::vector<std::u32string> SegmentMorae(std::u32string_view normalized);

    /// Renders a mora sequence to romaji. The sokuon, hatsuon and long-vowel policies need neighbouring
    /// morae, so the whole sequence is rendered in one pass rather than unit by unit.
    Outcome<std::vector<Mora>> RenderMorae(const std::vector<std::u32string> &morae, const RomajiOptions &options);

    /// Parses roman letters into kana units. Each returned string is already in the requested script.
    Outcome<std::vector<std::u32string>> ParseRomaji(std::u32string_view romaji, const KanaOptions &options);

} // namespace cpp_kana::v2::internal

#endif // CPP_KANA2_MORA_INTERNAL_H
