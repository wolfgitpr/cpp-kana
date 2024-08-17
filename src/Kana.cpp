#include <regex>

#include <cpp-kana/Kana.h>
#include "Kana_p.h"

#include "Utils.h"

namespace Kana
{
    constexpr int hiraganaStart = 0x3041;
    constexpr int katakanaStart = 0x30A1;

    static u32str convertKana(const u32str &kana, KanaType kanaType) {
        u32str convertedKana;
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
    static u32strVec convertKana(const u32strVec &kanaList, KanaType kanaType) {
        u32strVec inputlist;
        for (const auto &kana : kanaList)
            inputlist.emplace_back(kana);

        u32strVec convertedList;
        const std::regex rx("[\u3040-\u309F\u30A0-\u30FF]+");

        for (const auto &kana : kanaList) {
            u32str convertedKana;
            if (std::regex_match(kana.encodeUtf8(), rx)) {
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
        return convertKana(utf8strToU32str(kanaStr), kanaType).encodeUtf8();
    }

    std::vector<std::string> convertKana(const std::vector<std::string> &kanaList, KanaType kanaType) {
        u32strVec inputList;
        inputList.reserve(kanaList.size());
        for (const auto &item : kanaList) {
            inputList.emplace_back(utf8strToU32str(item));
        }

        const auto convertedList = convertKana(inputList, kanaType);
        std::vector<std::string> resultList;
        resultList.reserve(convertedList.size());
        for (const auto &item : convertedList) {
            resultList.emplace_back(item.encodeUtf8());
        }
        return resultList;
    }

    static KanaResVector u8kanaToRomaji(const u32strVec &kanaList, Error error, const bool &doubleWrittenSokuon) {
        KanaResVector res;
        const u32strVec inputList = convertKana(kanaList, KanaType::Hiragana);
        u32strVec romajiList;

        // convert kana to romaji
        for (const u32str &kana : inputList) {
            if (kana != U"゜" && kana != U"ー") {
                const auto it = kanaToRomajiMap.find(kana);
                if (it != kanaToRomajiMap.end())
                    res.emplace_back(KanaRes{kana.encodeUtf8(), it->second.encodeUtf8(), false});
                else if (error == Error::Default)
                    res.emplace_back(KanaRes{kana.encodeUtf8(), kana.encodeUtf8(), true});
            }
        }

        // double written sokuon
        for (int i = 0; i < res.size() - 1 && doubleWrittenSokuon; ++i) {
            char32_t nextChar = U' ';
            const auto it = romajiToKanaMap.find(utf8strToU32str(res[i].romaji));
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

    KanaResVector kanaToRomaji(const std::string &kanaStr, Error error, bool doubleWrittenSokuon) {
        return u8kanaToRomaji(splitString(utf8strToU32str(kanaStr)), error, doubleWrittenSokuon);
    }

    KanaResVector kanaToRomaji(const std::vector<std::string> &kanaList, Error error, bool doubleWrittenSokuon) {
        u32strVec inputList;
        inputList.reserve(kanaList.size());
        for (const auto &item : kanaList) {
            inputList.emplace_back(utf8strToU32str(item));
        }
        return u8kanaToRomaji(inputList, error, doubleWrittenSokuon);
    }

    static RomajiResVector u8romajiToKana(const u32strVec &romajiList, Error error, KanaType kanaType) {
        RomajiResVector res;
        for (const u32str &romaji : romajiList) {
            const auto it = romajiToKanaMap.find(romaji);
            if (it != romajiToKanaMap.end()) {
                res.emplace_back(RomajiRes{romaji.encodeUtf8(),
                                           kanaType == KanaType::Hiragana
                                           ? it->second.encodeUtf8()
                                           : convertKana(it->second, KanaType::Katakana).encodeUtf8(),
                                           false});
            } else if (error == Error::Default)
                res.emplace_back(RomajiRes{romaji.encodeUtf8(), romaji.encodeUtf8(), true});
        }
        return res;
    }

    RomajiResVector romajiToKana(const std::string &romajiStr, Error error, KanaType kanaType) {
        const u32strVec input = splitString(utf8strToU32str(romajiStr));
        return u8romajiToKana(input, error, kanaType);
    }

    RomajiResVector romajiToKana(const std::vector<std::string> &romajiList, Error error, KanaType kanaType) {
        u32strVec inputList;
        inputList.reserve(romajiList.size());
        for (const auto &item : romajiList) {
            inputList.emplace_back(utf8strToU32str(item));
        }
        return u8romajiToKana(inputList, error, kanaType);
    }
}
