#include "Internal.h"

#include <cstddef>
#include <string>
#include <vector>

#include "../data/Tables.h"
#include "../infra/Unicode.h"

namespace cpp_kana::v2::internal
{
    namespace
    {

        using infra::CharClass;
        using infra::Classify;

        bool IsVowel(char letter) noexcept {
            return letter == 'a' || letter == 'i' || letter == 'u' || letter == 'e' || letter == 'o';
        }

        bool IsConsonant(char letter) noexcept { return letter >= 'a' && letter <= 'z' && !IsVowel(letter); }

        char LastVowelOf(const std::string &text) noexcept {
            for (auto it = text.rbegin(); it != text.rend(); ++it) {
                if (IsVowel(*it)) {
                    return *it;
                }
            }
            return 0;
        }

        /// The table's canonical romanisation for one mora; empty when the table does not know it. Used for
        /// lookahead only (what the NEXT mora starts with), never as an answer.
        std::string BaseRomaji(const std::u32string &mora) {
            if (const auto *entry = data::FindMora(infra::ToUtf8(mora))) {
                return std::string(entry->romaji);
            }
            // Archaic kana are folded here as well: ゐ/ゑ are NOT table rows (they were removed because い/え already
    // own "i"/"e"), so this branch is the only place they are handled -- which is why the fold is stated once, in
            // RenderMorae, so lookahead and rendering cannot disagree.
            if (mora.size() == 1 && mora[0] == U'ゐ') {
                return "i";
            }
            if (mora.size() == 1 && mora[0] == U'ゑ') {
                return "e";
            }
            return std::string();
        }

    } // namespace

    Outcome<std::vector<Mora>> RenderMorae(const std::vector<std::u32string> &morae, const RomajiOptions &options) {
        std::vector<Mora> out;
        out.reserve(morae.size());
        char previousVowel = 0;

        for (std::size_t i = 0; i < morae.size(); ++i) {
            const std::u32string &mora = morae[i];
            Mora rendered;
            rendered.text = infra::ToUtf8(mora);
            const char32_t single = mora.size() == 1 ? mora[0] : 0;
            const CharClass charClass = single != 0 ? Classify(single) : CharClass::Other;

            if (single != 0 && charClass == CharClass::Sokuon) {
                rendered.kind = MoraKind::Sokuon;
                if (options.sokuon == SokuonPolicy::Drop) {
                    rendered.rendered.clear();
                } else {
                    const std::string next = (i + 1 < morae.size()) ? BaseRomaji(morae[i + 1]) : std::string();
                    if (next.rfind("ch", 0) == 0) {
                        // Hepburn spells っち as tch (まっちゃ -> matcha), not ccha.
                        rendered.rendered = "t";
                    } else if (!next.empty() && IsConsonant(next.front())) {
                        rendered.rendered = std::string(1, next.front());
                    }
                }
            } else if (single != 0 && charClass == CharClass::LongVowelMark) {
                rendered.kind = MoraKind::LongVowel;
                switch (options.longVowel) {
                case LongVowelPolicy::Repeat:
                    if (previousVowel != 0) {
                        rendered.rendered = std::string(1, previousVowel);
                    }
                    break;
                case LongVowelPolicy::Drop:
                    break;
                case LongVowelPolicy::KatakanaDash:
                    rendered.rendered = "-";
                    break;
                }
            } else if (single == U'ん') {
                rendered.kind = MoraKind::Hatsuon;
                const std::string next = (i + 1 < morae.size()) ? BaseRomaji(morae[i + 1]) : std::string();
                const bool beforeVowelOrY = !next.empty() && (IsVowel(next.front()) || next.front() == 'y');
                const bool apostrophe = options.hatsuon == HatsuonPolicy::Apostrophe && beforeVowelOrY;
                rendered.rendered = apostrophe ? "n'" : "n";
            } else {
                const data::MoraEntry *entry = data::FindMora(rendered.text);
                if (entry != nullptr) {
                    rendered.rendered = std::string(entry->romaji);
                    rendered.kind = static_cast<MoraKind>(entry->kind);
                } else if (single == U'ゐ' || single == U'ゑ') {
                    // Archaic kana: recognised, folded to the modern vowel, and marked as such so a caller
                    // can tell it apart from い/え.
                    rendered.rendered = single == U'ゐ' ? "i" : "e";
                    rendered.kind = MoraKind::Archaic;
                } else {
                    rendered.kind = MoraKind::Other;
                    rendered.code = Code::UnsupportedCharacter;
                    switch (options.unknown) {
                    case UnknownPolicy::Keep:
                        rendered.rendered = rendered.text;
                        break;
                    case UnknownPolicy::Ignore:
                        rendered.rendered.clear();
                        break;
                    case UnknownPolicy::Fail:
                        return Error{Code::UnsupportedCharacter, "no romanisation for '" + rendered.text + "'"};
                    }
                }
            }

            if (const char vowel = LastVowelOf(rendered.rendered); vowel != 0) {
                previousVowel = vowel;
            }
            out.push_back(std::move(rendered));
        }

        return out;
    }

} // namespace cpp_kana::v2::internal
