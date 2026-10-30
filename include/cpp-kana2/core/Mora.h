#ifndef CPP_KANA2_CORE_MORA_H
#define CPP_KANA2_CORE_MORA_H

#include <cstdint>
#include <string>
#include <vector>

#include <cpp-kana2/core/Global.h>
#include <cpp-kana2/core/Types.h>

namespace cpp_kana::v2
{

    /// What kind of unit a mora is. This is the "mark the hard cases instead of guessing" axis: a caller can
    /// tell an ordinary syllable from a sokuon, a hatsuon, a long vowel mark, or a character the table does
    /// not know, without parsing the rendered string.
    enum class MoraKind : std::int32_t {
        Syllable = 0,
        Sokuon = 1,
        Hatsuon = 2,
        LongVowel = 3,
        /// A character that is neither kana nor a roman letter (kanji, digits, punctuation). Rendered
        /// unchanged under UnknownPolicy::Keep.
        Other = 4,
        /// ゐ / ゑ in either script: recognised, and folded to i / e on the romaji side.
        Archaic = 5,
        /// Produced by the reverse direction: a run of roman letters consumed as one unit.
        Romaji = 6,  // reserved: not produced by 2.0 (see docs/API.md, MoraKind)
    };

    /// One unit of a conversion, in both spellings plus its classification.
    ///
    /// The rules that this structure carries are stated here and are normative:
    ///   1. `text` is the unit's spelling in the NORMALISED stream, not in the caller's input: half-width
    ///      katakana is folded to hiragana, katakana is folded to hiragana, and a voicing mark is applied to
    ///      the kana before it (so kanaToRomaji("カタカナ").moras()[0].text is "か", not "カ", and ヷ becomes
    ///      the two units ゔ + ぁ). This is deliberate: the table is keyed by hiragana, so the unit that
    ///      matched a table row is a hiragana unit, and reporting a source offset the caller would have to
    ///      recompute through the same normalisation would be a promise this structure cannot keep.
    ///      A caller that needs input offsets should normalise first (convertScript) and keep its own map.
    ///   2. `rendered` is what the unit contributes to the output, already carrying the policy decisions
    ///      (sokuon doubling, hatsuon apostrophe, long-vowel repetition, case, ...).
    ///   3. `code` is per unit: Code::Ok means it converted, Code::UnsupportedCharacter means the table does
    ///      not know it and `rendered` is the source spelling kept as-is. A call can therefore succeed with
    ///      a non-ok mora inside -- that is what UnknownPolicy::Keep means.
    ///   4. The concatenation of `rendered` (with the requested separator) is the whole result; there is no
    ///      hidden state that changes it.
    struct CPP_KANA2_EXPORT Mora {
        std::string text;
        std::string rendered;
        MoraKind kind = MoraKind::Syllable;
        Code code = Code::Ok;
    };

    /// A conversion result: the whole rendered string plus the per-unit breakdown behind it.
    ///
    /// Copyable and movable; both directions of a conversion produce one of these.
    class CPP_KANA2_EXPORT Conversion {
    public:
        Conversion() = default;
        Conversion(std::string text, std::vector<Mora> moras, std::string separator = {});

        /// The source text, unchanged.
        const std::string &text() const noexcept { return m_text; }

        /// One entry per unit, in input order.
        const std::vector<Mora> &moras() const noexcept { return m_moras; }

        /// The separator the conversion was asked for ("" when the caller asked for none).
        const std::string &separator() const noexcept { return m_separator; }

        /// The rendered units joined with the separator the conversion used.
        std::string rendered() const { return rendered(m_separator.c_str()); }

        /// The rendered units joined with an explicit separator.
        std::string rendered(const char *separator) const;

        /// True when every mora converted (no Code::UnsupportedCharacter and no other failure recorded).
        bool ok() const noexcept;

    private:
        std::string m_text;
        std::vector<Mora> m_moras;
        std::string m_separator;
    };

} // namespace cpp_kana::v2

#endif // CPP_KANA2_CORE_MORA_H
