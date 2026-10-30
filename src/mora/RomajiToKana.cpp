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

        bool IsVowel(char letter) noexcept {
            return letter == 'a' || letter == 'i' || letter == 'u' || letter == 'e' || letter == 'o';
        }

        bool IsConsonant(char letter) noexcept { return letter >= 'a' && letter <= 'z' && !IsVowel(letter); }

        bool IsAsciiLetter(char letter) noexcept { return letter >= 'a' && letter <= 'z'; }

        /// Parses one run of lower-case roman letters. Returns false when UnknownPolicy::Fail rejects a symbol.
        bool ParseRun(const std::string &run, const KanaOptions &options, std::vector<std::u32string> *out) {
            const std::size_t maxLength = data::MaxRomajiLength();
            std::size_t i = 0;
            while (i < run.size()) {
                const char current = run[i];

                // Longest table match first: this is what makes "ni" a syllable rather than ん + い.
                const data::MoraEntry *entry = nullptr;
                std::size_t matchLength = 0;
                for (std::size_t length = maxLength; length >= 1; --length) {
                    if (i + length > run.size()) {
                        continue;
                    }
                    if (const data::MoraEntry *candidate = data::FindRomaji(std::string_view(run.data() + i, length))) {
                        entry = candidate;
                        matchLength = length;
                        break;
                    }
                }
                if (entry != nullptr) {
                    out->push_back(infra::FromUtf8(entry->kana));
                    i += matchLength;
                    continue;
                }

                // ん, in the order a Japanese IME applies the rules:
                //   n'                -> ん
                //   nn + a syllable   -> ん, then that syllable (konnichiha = ko-n-ni-chi-ha)
                //   nn otherwise      -> ん
                //   n  + consonant    -> ん
                if (current == 'n' && i + 1 < run.size()) {
                    const char next = run[i + 1];
                    if (next == '\'') {
                        out->emplace_back(U"ん");
                        i += 2;
                        continue;
                    }
                    if (next == 'n') {
                        // Does a syllable start at the second n? The probe has to try 3 letters as well as 2,
                        // because "nya/nyu/nyo" start with two consonants: MEASURED before this, "nnya" came
                        // out んや (the に was lost) instead of んにゃ.
                        const std::size_t remaining = run.size() - (i + 1);
                        const bool secondStartsSyllable =
                            (remaining >= 3 &&
                             data::FindRomaji(std::string_view(run.data() + i + 1, 3)) != nullptr) ||
                            (remaining >= 2 &&
                             data::FindRomaji(std::string_view(run.data() + i + 1, 2)) != nullptr);
                        out->emplace_back(U"ん");
                        i += secondStartsSyllable ? 1 : 2;
                        continue;
                    }
                    if (IsConsonant(next) && next != 'y') {
                        out->emplace_back(U"ん");
                        ++i;
                        continue;
                    }
                }
                if (current == 'n' && i + 1 >= run.size()) {
                    out->emplace_back(U"ん");
                    ++i;
                    continue;
                }

                // っ: a doubled consonant (kitte, gakkou), and Hepburn's tch (matcha).
                if (IsConsonant(current) && i + 1 < run.size()) {
                    if (run.compare(i, 3, "tch") == 0) {
                        out->emplace_back(U"っ");
                        ++i;
                        continue;
                    }
                    if (run[i + 1] == current) {
                        out->emplace_back(U"っ");
                        ++i;
                        continue;
                    }
                }

                switch (options.unknown) {
                case UnknownPolicy::Keep:
                    out->emplace_back(1, static_cast<char32_t>(static_cast<unsigned char>(current)));
                    break;
                case UnknownPolicy::Ignore:
                    break;
                case UnknownPolicy::Fail:
                    return false;
                }
                ++i;
            }
            return true;
        }

    } // namespace

    Outcome<std::vector<std::u32string>> ParseRomaji(std::u32string_view romaji, const KanaOptions &options) {
        std::vector<std::u32string> out;
        out.reserve(romaji.size());

        std::size_t i = 0;
        while (i < romaji.size()) {
            const char32_t codePoint = romaji[i];
            const bool ascii = codePoint < 0x80;
            const char letter = ascii ? static_cast<char>(codePoint) : '\0';
            const char lower = (letter >= 'A' && letter <= 'Z') ? static_cast<char>('a' + (letter - 'A')) : letter;

            if (!ascii || !IsAsciiLetter(lower)) {
                // Not part of a romanised syllable: hand it back as its own unit (or drop it).
                if (options.unknown == UnknownPolicy::Ignore) {
                    ++i;
                    continue;
                }
                if (options.unknown == UnknownPolicy::Fail) {
                    return Error{Code::UnsupportedCharacter,
                                 "cannot read code point U+" + std::to_string(static_cast<unsigned>(codePoint))};
                }
                out.emplace_back(1, codePoint);
                ++i;
                continue;
            }

            // Collect the maximal run of ASCII letters, folded to lower case: "Kimochi" is the same request
            // as "kimochi", and a case-sensitive parser would answer "unsupported character" for a spelling
            // people actually type.
            std::string run;
            while (i < romaji.size()) {
                const char32_t next = romaji[i];
                if (next == U'\'') {
                    // The apostrophe belongs to the run: "shin'you" would otherwise be cut at the quote,
                    // and the hatsuon rule (n') would never see it.
                    run.push_back('\'');
                    ++i;
                    continue;
                }
                if (next >= 0x80) {
                    break;
                }
                const char raw = static_cast<char>(next);
                const char folded = (raw >= 'A' && raw <= 'Z') ? static_cast<char>('a' + (raw - 'A')) : raw;
                if (!IsAsciiLetter(folded)) {
                    break;
                }
                run.push_back(folded);
                ++i;
            }
            if (!ParseRun(run, options, &out)) {
                return Error{Code::UnsupportedCharacter, "cannot read '" + run + "'"};
            }
        }

        if (options.script == Script::Katakana) {
            for (std::u32string &unit : out) {
                for (char32_t &codePoint : unit) {
                    codePoint = infra::HiraganaToKatakana(codePoint);
                }
            }
        }
        return out;
    }

} // namespace cpp_kana::v2::internal
