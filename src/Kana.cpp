#include <regex>

#include "Kana.h"
#include "Kana_p.h"

#include "Utils.h"

namespace Kana
{
    // keep only letters
    static std::string filterString(const std::string &str) {
        std::string words;
        for (const auto &ch : str) {
            const auto u = ch;
            if (u >= 128 || !isLetter(ch)) {
                if (words.empty() || words.back() != ' ') {
                    words += ' ';
                }
                continue;
            }
            words += ch;
        }
        return words;
    }

    constexpr int hiraganaStart = 0x3041;
    constexpr int katakanaStart = 0x30A1;

    static u8string convertKana(const u8string &kana, KanaType kanaType) {
        u8string convertedKana;
        for (const char32_t kanaChar : kana) {
            if (kanaType == KanaType::Hiragana) {
                // target is Hiragana
                if (kanaChar >= katakanaStart && kanaChar < katakanaStart + 0x5E) {
                    // Katakana->Hiragana
                    convertedKana += static_cast<char32_t>(kanaChar - katakanaStart + hiraganaStart);
                } else {
                    convertedKana += kanaChar;
                }
            } else {
                // target is Katakana
                if (kanaChar >= hiraganaStart && kanaChar < hiraganaStart + 0x5E) {
                    // Hiragana->Katakana
                    convertedKana += static_cast<char32_t>(kanaChar + katakanaStart - hiraganaStart);
                } else {
                    convertedKana += kanaChar;
                }
            }
        }
        return convertedKana;
    }

    // convert Hiragana to Katakana
    static u8stringlist convertKana(const u8stringlist &kanaList, KanaType kanaType) {
        u8stringlist inputlist;
        for (const auto &kana : kanaList)
            inputlist.emplace_back(kana);

        u8stringlist convertedList;
        const std::regex rx("[\u3040-\u309F\u30A0-\u30FF]+");

        for (const auto &kana : kanaList) {
            u8string convertedKana;
            if (std::regex_match(kana.cpp_str(), rx)) {
                for (const char32_t kanaChar : kana) {
                    if (kanaType == KanaType::Hiragana) {
                        // target is Hiragana
                        if (kanaChar >= katakanaStart && kanaChar < katakanaStart + 0x5E) {
                            // Katakana->Hiragana
                            convertedKana +=
                                static_cast<char32_t>(kanaChar - katakanaStart + hiraganaStart);
                        } else {
                            convertedKana += kanaChar;
                        }
                    } else {
                        // target is Katakana
                        if (kanaChar >= hiraganaStart && kanaChar < hiraganaStart + 0x5E) {
                            // Hiragana->Katakana
                            convertedKana +=
                                static_cast<char32_t>(kanaChar + katakanaStart - hiraganaStart);
                        } else {
                            convertedKana += kanaChar;
                        }
                    }
                }
            } else {
                convertedKana = kana;
            }
            convertedList.emplace_back(convertedKana);
        }
        return convertedList;
    }

    std::string convertKana(const std::string &kanaStr, KanaType kanaType) {
        return convertKana(u8string(kanaStr), kanaType).c_str();
    }

    static KanaResVector u8kanaToRomaji(const u8stringlist &kanaList, Error error, const bool &doubleWrittenSokuon) {
        KanaResVector res;
        const u8stringlist inputList = convertKana(kanaList, KanaType::Hiragana);
        u8stringlist romajiList;

        // convert kana to romaji
        for (const u8string &kana : inputList) {
            if (kana != "゜" && kana != "ー") {
                const auto it = kanaToRomajiMap.find(kana);
                if (it != kanaToRomajiMap.end())
                    res.emplace_back(KanaRes{kana.c_str(), it->second.c_str(), false});
                else if (error == Error::Default)
                    res.emplace_back(KanaRes{kana.c_str(), kana.c_str(), true});
            }
        }

        // double written sokuon
        for (int i = 0; i < res.size() - 1 && doubleWrittenSokuon; ++i) {
            char32_t nextChar = U' ';
            const auto it = romajiToKanaMap.find(res[i].romaji);
            if (it != romajiToKanaMap.end())
                nextChar = it->second.at(0);
            if (res[i].romaji == "cl" && isKana(nextChar) &&
                std::u32string(U"あいうえおアイウエオっんを").find(nextChar) == std::u32string::npos) {
                res[i].romaji.insert(res[i].romaji.begin(), res[i].romaji[0]);
                res.erase(res.begin() + i);
            }
        }

        return res;
    }

    KanaResVector kanaToRomaji(const std::string &kanaStr, Error error, const bool &doubleWrittenSokuon) {
        return u8kanaToRomaji(splitString(u8string(kanaStr)), error, doubleWrittenSokuon);
    }

    KanaResVector kanaToRomaji(const std::vector<std::string> &kanaList, Error error, const bool &doubleWrittenSokuon) {
        u8stringlist inputList;
        inputList.reserve(kanaList.size());
        for (const auto &item : kanaList) {
            inputList.emplace_back(item);
        }
        return u8kanaToRomaji(inputList, error, doubleWrittenSokuon);
    }

    static RomajiResVector u8romajiToKana(const u8stringlist &romajiList, Error error, KanaType kanaType) {
        RomajiResVector res;
        for (const u8string &romaji : romajiList) {
            const auto it = romajiToKanaMap.find(romaji);
            if (it != romajiToKanaMap.end()) {
                res.emplace_back(RomajiRes{romaji.c_str(),
                                           kanaType == KanaType::Hiragana
                                           ? it->second.c_str()
                                           : convertKana(it->second, KanaType::Katakana).c_str(),
                                           false});
            } else if (error == Error::Default)
                res.emplace_back(RomajiRes{romaji.c_str(), romaji.c_str(), true});
        }
        return res;
    }

    RomajiResVector romajiToKana(const std::string &romajiStr, Error error, KanaType kanaType) {
        const u8stringlist input = splitString(romajiStr);
        return u8romajiToKana(input, error, kanaType);
    }

    RomajiResVector romajiToKana(const std::vector<std::string> &romajiList, Error error, KanaType kanaType) {
        u8stringlist inputList;
        inputList.reserve(romajiList.size());
        for (const auto &item : romajiList) {
            inputList.emplace_back(item);
        }
        return u8romajiToKana(inputList, error, kanaType);
    }
}
