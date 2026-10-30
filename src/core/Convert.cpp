#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Utf8.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "infra/Unicode.h"
#include "mora/Internal.h"

namespace cpp_kana::v2
{
    namespace
    {

        const char *SeparatorFor(SeparatorPolicy policy) noexcept {
            return policy == SeparatorPolicy::Space ? " " : "";
        }

        /// Applies CasePolicy to already-rendered morae. Doing it here (one place, after rendering) keeps every
        /// policy out of the renderer's per-mora logic, which is what makes "upper case" mean the same thing for
        /// a hatsuon's apostrophe and for a syllable.
        void ApplyCase(std::vector<Mora> *morae, CasePolicy policy) {
            if (policy == CasePolicy::Lower) {
                return;
            }
            bool firstLetterSeen = false;
            for (Mora &mora : *morae) {
                for (char &letter : mora.rendered) {
                    if (letter < 'a' || letter > 'z') {
                        continue;
                    }
                    const bool capitalise = policy == CasePolicy::Upper || !firstLetterSeen;
                    if (capitalise) {
                        letter = static_cast<char>('A' + (letter - 'a'));
                    }
                    firstLetterSeen = true;
                }
            }
        }

    } // namespace

    Outcome<Conversion> kanaToRomaji(std::string_view kana, const RomajiOptions &options) {
        const Outcome<std::u32string> codepoints = utf8ToCodepoints(kana);
        if (!codepoints.ok()) {
            return codepoints.error();
        }
        const Outcome<std::u32string> normalized = internal::NormalizeToHiragana(codepoints.value());
        if (!normalized.ok()) {
            return normalized.error();
        }
        const std::vector<std::u32string> morae = internal::SegmentMorae(normalized.value());
        Outcome<std::vector<Mora>> rendered = internal::RenderMorae(morae, options);
        if (!rendered.ok()) {
            return rendered.error();
        }
        ApplyCase(&rendered.value(), options.letterCase);
        return Conversion(std::string(kana), std::move(rendered.value()), SeparatorFor(options.separator));
    }

    Outcome<Conversion> romajiToKana(std::string_view romaji, const KanaOptions &options) {
        const Outcome<std::u32string> codepoints = utf8ToCodepoints(romaji);
        if (!codepoints.ok()) {
            return codepoints.error();
        }
        Outcome<std::vector<std::u32string>> parsed = internal::ParseRomaji(codepoints.value(), options);
        if (!parsed.ok()) {
            return parsed.error();
        }

        std::vector<Mora> morae;
        morae.reserve(parsed.value().size());
        for (const std::u32string &unit : parsed.value()) {
            Mora mora;
            mora.text = infra::ToUtf8(unit);
            mora.rendered = mora.text;
            // Every unit is either kana produced by a table match or a character passed through unchanged, and
            // the parser only passes through characters that are not kana -- so classification, not length, is
            // what distinguishes them. MEASURED before this: `unit.size() == 1 &&` labelled every multi-code-point
            // syllable (きゃ, てぃ, ふぁ, ゔぁ) as MoraKind::Other, which is the kind the documentation reserves for
            // "neither kana nor a roman letter".
            mora.kind = !unit.empty() && infra::Classify(unit.front()) != infra::CharClass::Other
                ? MoraKind::Syllable
                : MoraKind::Other;
            if (mora.kind == MoraKind::Other) {
                mora.code = Code::Ok; // passed through on purpose, not a table miss
            }
            if (unit.size() == 1 && (unit.front() == U'っ')) {
                mora.kind = MoraKind::Sokuon;
            } else if (unit.size() == 1 && unit.front() == U'ん') {
                mora.kind = MoraKind::Hatsuon;
            }
            morae.push_back(std::move(mora));
        }
        return Conversion(std::string(romaji), std::move(morae), "");
    }

    Outcome<Conversion> convertScript(std::string_view text, Script target, UnknownPolicy unknown) {
        const Outcome<std::u32string> codepoints = utf8ToCodepoints(text);
        if (!codepoints.ok()) {
            return codepoints.error();
        }
        // Normalise first, so a half-width or combining spelling converts the same way as the precomposed
        // one (ｶﾞｯｺｳ and がっこう both become ガッコウ). v1 left half-width input unsupported -- and then
        // faulted on it, because a zero-token input wrapped size()-1.
        const Outcome<std::u32string> normalized = internal::NormalizeToHiragana(codepoints.value());
        if (!normalized.ok()) {
            return normalized.error();
        }

        std::vector<Mora> morae;
        morae.reserve(normalized.value().size());
        for (const char32_t codePoint : normalized.value()) {
            char32_t converted = codePoint;
            if (target == Script::Katakana) {
                converted = infra::HiraganaToKatakana(codePoint);
            } else if (target == Script::Hiragana) {
                converted = infra::KatakanaToHiragana(codePoint);
            }

            Mora mora;
            mora.text = infra::ToUtf8(std::u32string(1, codePoint));
            mora.rendered = infra::ToUtf8(std::u32string(1, converted));
            const bool isKana = infra::Classify(codePoint) == infra::CharClass::Hiragana ||
                infra::Classify(codePoint) == infra::CharClass::Katakana ||
                infra::Classify(codePoint) == infra::CharClass::Sokuon ||
                infra::Classify(codePoint) == infra::CharClass::LongVowelMark;
            if (isKana) {
                mora.kind = MoraKind::Syllable;
            } else {
                mora.kind = MoraKind::Other;
                if (unknown == UnknownPolicy::Ignore) {
                    mora.rendered.clear();
                } else if (unknown == UnknownPolicy::Fail) {
                    return Error{Code::UnsupportedCharacter,
                                 "not kana: U+" + std::to_string(static_cast<unsigned>(codePoint))};
                }
            }
            morae.push_back(std::move(mora));
        }
        return Conversion(std::string(text), std::move(morae), "");
    }

    const char *version() noexcept { return CPP_KANA2_VERSION_STRING; }

} // namespace cpp_kana::v2
