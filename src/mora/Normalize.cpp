#include "Internal.h"

#include <cstddef>

#include "../infra/Unicode.h"

namespace cpp_kana::v2::internal
{
    namespace
    {

        constexpr char32_t kHalfwidthVoiced = 0xFF9E;
        constexpr char32_t kHalfwidthSemiVoiced = 0xFF9F;
        constexpr char32_t kDakuten = 0x3099;
        constexpr char32_t kHandakuten = 0x309A;
        constexpr char32_t kKatakanaDakuten = 0x309B;
        constexpr char32_t kKatakanaHandakuten = 0x309C;

        bool IsVariationSelector(char32_t codePoint) noexcept { return codePoint >= 0xFE00 && codePoint <= 0xFE0F; }

        bool IsVoicingMark(char32_t codePoint) noexcept {
            return codePoint == kDakuten || codePoint == kHandakuten || codePoint == kKatakanaDakuten ||
                   codePoint == kKatakanaHandakuten;
        }

        bool IsHalfwidthVoicingMark(char32_t codePoint) noexcept {
            return codePoint == kHalfwidthVoiced || codePoint == kHalfwidthSemiVoiced;
        }

        /// The hiragana that ヷ/ヸ/ヹ/ヺ (wa-row kana with dakuten) expand into, or 0 when the code point is not
        /// one of them. They have no hiragana counterpart, so they become ゔ + small vowel, which is both how
        /// they are pronounced (va/vi/ve/vo) and inside the table's domain.
        char32_t WaRowExpansion(char32_t codePoint, std::u32string &sink) {
            if (codePoint < 0x30F7 || codePoint > 0x30FA) {
                return 0;
            }
            static const char32_t kSmallVowel[] = {U'ぁ', U'ぃ', U'ぇ', U'ぉ'};
            sink.push_back(U'ゔ');
            sink.push_back(kSmallVowel[codePoint - 0x30F7]);
            return 1;
        }

        /// The hiragana small vowel for a wa-row kana that has just received a dakuten, or 0.
        ///
        /// MEASURED defect this exists for: ワ゛ used to lose its dakuten, because ワ had already been folded to
        /// わ by the time the mark arrived and infra::ApplyVoicing(わ) has no answer. ヷ has no hiragana form, so
        /// the fold has to go through ゔ + small vowel here rather than through ApplyVoicing.
        char32_t WaRowDakuten(char32_t hiragana, std::u32string &sink) {
            char32_t small = 0;
            switch (hiragana) {
            case U'わ': small = U'ぁ'; break;
            case U'ゐ': small = U'ぃ'; break;
            case U'ゑ': small = U'ぇ'; break;
            case U'を': small = U'ぉ'; break;
            default: return 0;
            }
            sink.push_back(U'ゔ');
            sink.push_back(small);
            return 1;
        }

        /// Appends one kana after folding katakana and the wa-row forms into the hiragana table's domain.
        void AppendNormalized(std::u32string &out, char32_t codePoint) {
            if (WaRowExpansion(codePoint, out) != 0) {
                return;
            }
            if (codePoint >= 0x30A1 && codePoint <= 0x30FF) {
                codePoint = infra::KatakanaToHiragana(codePoint);
            }
            out.push_back(codePoint);
        }

        /// Applies a voicing mark to the last emitted kana, when it has a voiced form.
        void ApplyVoicingToLast(std::u32string &out, bool semiVoiced) {
            if (out.empty()) {
                return;
            }
            const char32_t voiced = infra::ApplyVoicing(out.back(), semiVoiced);
            if (voiced != 0) {
                out.pop_back();
                AppendNormalized(out, voiced);
                return;
            }
            if (!semiVoiced && WaRowDakuten(out.back(), out) != 0) {
                // わ/ゐ/ゑ/を + dakuten -> ゔ + small vowel; the base kana is gone, the expansion is what remains.
                out.erase(out.size() - 3, 1);
                return;
            }
            // A mark that cannot be applied is dropped rather than kept: keeping it would put a character in the
            // mora stream that no table entry can ever match, which is how v1 turned か゛く into "ka ゛ ku".
        }

    } // namespace

    Outcome<std::u32string> NormalizeToHiragana(std::u32string_view codepoints) {
        std::u32string out;
        out.reserve(codepoints.size());

        for (std::size_t i = 0; i < codepoints.size(); ++i) {
            char32_t codePoint = codepoints[i];

            if (IsVariationSelector(codePoint)) {
                continue;
            }

            if (IsVoicingMark(codePoint)) {
                ApplyVoicingToLast(out, codePoint == kHandakuten || codePoint == kKatakanaHandakuten);
                continue;
            }

            if (IsHalfwidthVoicingMark(codePoint)) {
                ApplyVoicingToLast(out, codePoint == kHalfwidthSemiVoiced);
                continue;
            }

            if (codePoint >= 0xFF61 && codePoint <= 0xFF9D) {
                codePoint = infra::FoldHalfwidth(codePoint);
                // The half-width mark follows its base kana as a separate code point; consume it here so the
                // loop does not have to special-case the next iteration.
                if (i + 1 < codepoints.size() && IsHalfwidthVoicingMark(codepoints[i + 1])) {
                    const char32_t voiced = infra::ApplyVoicing(codePoint, codepoints[i + 1] == kHalfwidthSemiVoiced);
                    if (voiced != 0) {
                        codePoint = voiced;
                        ++i;
                    }
                }
                // A folded half-width kana can be voiced (ﾜﾞ -> ワ -> ヷ), so it goes through the same
                // normalising append as everything else: the wa-row expansion must happen AFTER voicing.
                AppendNormalized(out, codePoint);
                continue;
            }

            // Iteration marks repeat the preceding kana (ゞ/ヾ also voice it).
            if (codePoint == 0x309D || codePoint == 0x309E || codePoint == 0x30FD || codePoint == 0x30FE) {
                if (!out.empty()) {
                    const char32_t repeated = out.back();
                    out.push_back(repeated);
                    if (codePoint == 0x309E || codePoint == 0x30FE) {
                        ApplyVoicingToLast(out, false);
                    }
                }
                continue;
            }

            AppendNormalized(out, codePoint);
        }

        return out;
    }

} // namespace cpp_kana::v2::internal
