#ifndef CPP_KANA2_INFRA_UNICODE_H
#define CPP_KANA2_INFRA_UNICODE_H

#include <cstdint>
#include <string>
#include <string_view>

namespace cpp_kana::v2::infra
{

    // Character classes the conversion needs. A code point falls in exactly one of them. This is what tells a
    // caller "kana or not"; the segmenter works from the mora table instead (longest match), so the order of
    // these enumerators carries no algorithmic meaning.
    enum class CharClass : std::int32_t {
        Hiragana,
        Katakana,
        HalfwidthKatakana,
        /// っ / ッ -- a sokuon.
        Sokuon,
        /// ー / ｰ -- the long vowel mark.
        LongVowelMark,
        /// U+3099 / U+309A, and the standalone U+309B / U+309C.
        VoicingMark,
        /// ASCII letters: the reverse direction consumes them as one run.
        LatinLetter,
        /// Everything else: kanji, digits, punctuation, whitespace, other scripts.
        Other,
    };

    CharClass Classify(char32_t codePoint) noexcept;

    /// Folds half-width katakana (U+FF61..U+FF9F) to its full-width form. Voiced half-width kana are NOT
    /// folded to a precomposed kana here -- they keep their base kana and the caller applies the voicing
    /// mark, because ﾊﾞ and バ must end up as the same mora.
    /// Returns the folded code point, or the input when it is not half-width katakana.
    char32_t FoldHalfwidth(char32_t codePoint) noexcept;

    /// katakana -> hiragana for every kataKana code point that HAS a hiragana counterpart. Three groups are
    /// deliberately left alone, and the caller handles each: ヷヸヹヺ (0x30F7..0x30FA) have no hiragana form and
    /// are expanded to ゔ + small vowel by the normaliser; the iteration marks ヽヾ (0x30FD/0x30FE) and the
    /// ヵヶ placeholders have no exact counterpart either. Returns the input for those and for non-katakana.
    char32_t KatakanaToHiragana(char32_t codePoint) noexcept;

    /// hiragana -> katakana (the inverse of the above).
    char32_t HiraganaToKatakana(char32_t codePoint) noexcept;

    /// Applies a voicing mark to a kana: か + ゛ -> が, は + ゜ -> ぱ. Returns 0 when the pair has no voiced
    /// form (the caller then keeps both characters as separate morae).
    char32_t ApplyVoicing(char32_t base, bool semiVoiced) noexcept;

    /// Encodes code points that are already known to be valid. This is the unchecked path used inside the
    /// conversion pipeline; the checked, fallible version for callers is cpp_kana::v2::codepointsToUtf8.
    std::string ToUtf8(std::u32string_view codepoints);

    /// Decodes table keys, which are valid by construction (they come from data/moras.tsv and the generator
    /// refuses anything else). Callers that read bytes from outside the library use the checked
    /// cpp_kana::v2::utf8ToCodepoints instead.
    std::u32string FromUtf8(std::string_view utf8);

} // namespace cpp_kana::v2::infra

#endif // CPP_KANA2_INFRA_UNICODE_H
