#include <iostream>
#include <regex>
#include <unordered_map>

#include <cpp-kana/Kana.h>

#include "../include/cpp-kana/U16Str.h"
#include "Utils.h"

namespace Kana
{
    constexpr int hiraganaStart = 0x3041;
    constexpr int katakanaStart = 0x30A1;

    static const std::unordered_map<std::u16string, std::u16string> kanaToRomajiMap = {
        {u"っ", u"cl"},
        {u"あ", u"a"},
        {u"い", u"i"},
        {u"いぇ", u"ye"},
        {u"う", u"u"},
        {u"うぁ", u"wa"},
        {u"うぃ", u"wi"},
        {u"うぇ", u"we"},
        {u"うぉ", u"wo"},
        {u"え", u"e"},
        {u"お", u"o"},
        {u"か", u"ka"},
        {u"が", u"ga"},
        {u"き", u"ki"},
        {u"きぇ", u"kye"},
        {u"きゃ", u"kya"},
        {u"きゅ", u"kyu"},
        {u"きょ", u"kyo"},
        {u"ぎ", u"gi"},
        {u"ぎぇ", u"gye"},
        {u"ぎゃ", u"gya"},
        {u"ぎゅ", u"gyu"},
        {u"ぎょ", u"gyo"},
        {u"く", u"ku"},
        {u"くぁ", u"kwa"},
        {u"くぃ", u"kwi"},
        {u"くぇ", u"kwe"},
        {u"くぉ", u"kwo"},
        {u"ぐ", u"gu"},
        {u"ぐぁ", u"gwa"},
        {u"ぐぃ", u"gwi"},
        {u"ぐぇ", u"gwe"},
        {u"ぐぉ", u"gwo"},
        {u"け", u"ke"},
        {u"げ", u"ge"},
        {u"こ", u"ko"},
        {u"ご", u"go"},
        {u"さ", u"sa"},
        {u"ざ", u"za"},
        {u"し", u"shi"},
        {u"しぇ", u"she"},
        {u"しゃ", u"sha"},
        {u"しゅ", u"shu"},
        {u"しょ", u"sho"},
        {u"ぢ", u"ji"},
        {u"じ", u"ji"},
        {u"じぇ", u"je"},
        {u"じゃ", u"ja"},
        {u"じゅ", u"ju"},
        {u"じょ", u"jo"},
        {u"す", u"su"},
        {u"すぁ", u"swa"},
        {u"すぃ", u"swi"},
        {u"すぇ", u"swe"},
        {u"すぉ", u"swo"},
        {u"づ", u"zu"},
        {u"ず", u"zu"},
        {u"ずぁ", u"zwa"},
        {u"ずぃ", u"zwi"},
        {u"ずぇ", u"zwe"},
        {u"ずぉ", u"zwo"},
        {u"せ", u"se"},
        {u"ぜ", u"ze"},
        {u"そ", u"so"},
        {u"ぞ", u"zo"},
        {u"た", u"ta"},
        {u"だ", u"da"},
        {u"ち", u"chi"},
        {u"ちぇ", u"che"},
        {u"ちゃ", u"cha"},
        {u"ちゅ", u"chu"},
        {u"ちょ", u"cho"},
        {u"つ", u"tsu"},
        {u"つぁ", u"tsa"},
        {u"つぃ", u"tsi"},
        {u"つぇ", u"tse"},
        {u"つぉ", u"tso"},
        {u"て", u"te"},
        {u"てぃ", u"ti"},
        {u"てゅ", u"tyu"},
        {u"で", u"de"},
        {u"でぃ", u"di"},
        {u"でゅ", u"dyu"},
        {u"と", u"to"},
        {u"とぅ", u"tu"},
        {u"ど", u"do"},
        {u"どぅ", u"du"},
        {u"な", u"na"},
        {u"に", u"ni"},
        {u"にぇ", u"nye"},
        {u"にゃ", u"nya"},
        {u"にゅ", u"nyu"},
        {u"にょ", u"nyo"},
        {u"ぬ", u"nu"},
        {u"ぬぁ", u"nwa"},
        {u"ぬぃ", u"nwi"},
        {u"ぬぇ", u"nwe"},
        {u"ぬぉ", u"nwo"},
        {u"ね", u"ne"},
        {u"の", u"no"},
        {u"は", u"ha"},
        {u"ば", u"ba"},
        {u"ぱ", u"pa"},
        {u"ひ", u"hi"},
        {u"ひぇ", u"hye"},
        {u"ひゃ", u"hya"},
        {u"ひゅ", u"hyu"},
        {u"ひょ", u"hyo"},
        {u"び", u"bi"},
        {u"びぇ", u"bye"},
        {u"びゃ", u"bya"},
        {u"びゅ", u"byu"},
        {u"びょ", u"byo"},
        {u"ぴ", u"pi"},
        {u"ぴぇ", u"pye"},
        {u"ぴゃ", u"pya"},
        {u"ぴゅ", u"pyu"},
        {u"ぴょ", u"pyo"},
        {u"ふ", u"fu"},
        {u"ふぁ", u"fa"},
        {u"ふぃ", u"fi"},
        {u"ふぇ", u"fe"},
        {u"ふぉ", u"fo"},
        {u"ぶ", u"bu"},
        {u"ぶぁ", u"bwa"},
        {u"ぶぃ", u"bwi"},
        {u"ぶぇ", u"bwe"},
        {u"ぶぉ", u"bwo"},
        {u"ぷ", u"pu"},
        {u"ぷぁ", u"pwa"},
        {u"ぷぃ", u"pwi"},
        {u"ぷぇ", u"pwe"},
        {u"ぷぉ", u"pwo"},
        {u"へ", u"he"},
        {u"べ", u"be"},
        {u"ぺ", u"pe"},
        {u"ほ", u"ho"},
        {u"ぼ", u"bo"},
        {u"ぽ", u"po"},
        {u"ま", u"ma"},
        {u"み", u"mi"},
        {u"みぇ", u"mye"},
        {u"みゃ", u"mya"},
        {u"みゅ", u"myu"},
        {u"みょ", u"myo"},
        {u"む", u"mu"},
        {u"むぁ", u"mwa"},
        {u"むぃ", u"mwi"},
        {u"むぇ", u"mwe"},
        {u"むぉ", u"mwo"},
        {u"め", u"me"},
        {u"も", u"mo"},
        {u"や", u"ya"},
        {u"ゆ", u"yu"},
        {u"よ", u"yo"},
        {u"ら", u"ra"},
        {u"り", u"ri"},
        {u"りぇ", u"rye"},
        {u"りゃ", u"rya"},
        {u"りゅ", u"ryu"},
        {u"りょ", u"ryo"},
        {u"る", u"ru"},
        {u"るぁ", u"rwa"},
        {u"るぃ", u"rwi"},
        {u"るぇ", u"rwe"},
        {u"るぉ", u"rwo"},
        {u"れ", u"re"},
        {u"ろ", u"ro"},
        {u"わ", u"wa"},
        {u"を", u"o"},
        {u"ん", u"n"},
        {u"ゔぁ", u"va"},
        {u"ゔぃ", u"vi"},
        {u"ゔ", u"vu"},
        {u"ゔぇ", u"ve"},
        {u"ゔぉ", u"vo"}};

    static const std::unordered_map<std::u16string, std::u16string> romajiToKanaMap = {
        {u"cl", u"っ"},
        {u"a", u"あ"},
        {u"i", u"い"},
        {u"ye", u"いぇ"},
        {u"u", u"う"},
        {u"wa", u"うぁ"},
        {u"wi", u"うぃ"},
        {u"we", u"うぇ"},
        {u"wo", u"うぉ"},
        {u"e", u"え"},
        {u"o", u"お"},
        {u"ka", u"か"},
        {u"ga", u"が"},
        {u"ki", u"き"},
        {u"kye", u"きぇ"},
        {u"kya", u"きゃ"},
        {u"kyu", u"きゅ"},
        {u"kyo", u"きょ"},
        {u"gi", u"ぎ"},
        {u"gye", u"ぎぇ"},
        {u"gya", u"ぎゃ"},
        {u"gyu", u"ぎゅ"},
        {u"gyo", u"ぎょ"},
        {u"ku", u"く"},
        {u"kwa", u"くぁ"},
        {u"kwi", u"くぃ"},
        {u"kwe", u"くぇ"},
        {u"kwo", u"くぉ"},
        {u"gu", u"ぐ"},
        {u"gwa", u"ぐぁ"},
        {u"gwi", u"ぐぃ"},
        {u"gwe", u"ぐぇ"},
        {u"gwo", u"ぐぉ"},
        {u"ke", u"け"},
        {u"ge", u"げ"},
        {u"ko", u"こ"},
        {u"go", u"ご"},
        {u"sa", u"さ"},
        {u"za", u"ざ"},
        {u"shi", u"し"},
        {u"she", u"しぇ"},
        {u"sha", u"しゃ"},
        {u"shu", u"しゅ"},
        {u"sho", u"しょ"},
        {u"ji", u"ぢ"},
        {u"ji", u"じ"},
        {u"je", u"じぇ"},
        {u"ja", u"じゃ"},
        {u"ju", u"じゅ"},
        {u"jo", u"じょ"},
        {u"su", u"す"},
        {u"swa", u"すぁ"},
        {u"swi", u"すぃ"},
        {u"swe", u"すぇ"},
        {u"swo", u"すぉ"},
        {u"zu", u"づ"},
        {u"zu", u"ず"},
        {u"zwa", u"ずぁ"},
        {u"zwi", u"ずぃ"},
        {u"zwe", u"ずぇ"},
        {u"zwo", u"ずぉ"},
        {u"se", u"せ"},
        {u"ze", u"ぜ"},
        {u"so", u"そ"},
        {u"zo", u"ぞ"},
        {u"ta", u"た"},
        {u"da", u"だ"},
        {u"chi", u"ち"},
        {u"che", u"ちぇ"},
        {u"cha", u"ちゃ"},
        {u"chu", u"ちゅ"},
        {u"cho", u"ちょ"},
        {u"tsu", u"つ"},
        {u"tsa", u"つぁ"},
        {u"tsi", u"つぃ"},
        {u"tse", u"つぇ"},
        {u"tso", u"つぉ"},
        {u"te", u"て"},
        {u"ti", u"てぃ"},
        {u"tyu", u"てゅ"},
        {u"de", u"で"},
        {u"di", u"でぃ"},
        {u"dyu", u"でゅ"},
        {u"to", u"と"},
        {u"tu", u"とぅ"},
        {u"do", u"ど"},
        {u"du", u"どぅ"},
        {u"na", u"な"},
        {u"ni", u"に"},
        {u"nye", u"にぇ"},
        {u"nya", u"にゃ"},
        {u"nyu", u"にゅ"},
        {u"nyo", u"にょ"},
        {u"nu", u"ぬ"},
        {u"nwa", u"ぬぁ"},
        {u"nwi", u"ぬぃ"},
        {u"nwe", u"ぬぇ"},
        {u"nwo", u"ぬぉ"},
        {u"ne", u"ね"},
        {u"no", u"の"},
        {u"ha", u"は"},
        {u"ba", u"ば"},
        {u"pa", u"ぱ"},
        {u"hi", u"ひ"},
        {u"hye", u"ひぇ"},
        {u"hya", u"ひゃ"},
        {u"hyu", u"ひゅ"},
        {u"hyo", u"ひょ"},
        {u"bi", u"び"},
        {u"bye", u"びぇ"},
        {u"bya", u"びゃ"},
        {u"byu", u"びゅ"},
        {u"byo", u"びょ"},
        {u"pi", u"ぴ"},
        {u"pye", u"ぴぇ"},
        {u"pya", u"ぴゃ"},
        {u"pyu", u"ぴゅ"},
        {u"pyo", u"ぴょ"},
        {u"fu", u"ふ"},
        {u"fa", u"ふぁ"},
        {u"fi", u"ふぃ"},
        {u"fe", u"ふぇ"},
        {u"fo", u"ふぉ"},
        {u"bu", u"ぶ"},
        {u"bwa", u"ぶぁ"},
        {u"bwi", u"ぶぃ"},
        {u"bwe", u"ぶぇ"},
        {u"bwo", u"ぶぉ"},
        {u"pu", u"ぷ"},
        {u"pwa", u"ぷぁ"},
        {u"pwi", u"ぷぃ"},
        {u"pwe", u"ぷぇ"},
        {u"pwo", u"ぷぉ"},
        {u"he", u"へ"},
        {u"be", u"べ"},
        {u"pe", u"ぺ"},
        {u"ho", u"ほ"},
        {u"bo", u"ぼ"},
        {u"po", u"ぽ"},
        {u"ma", u"ま"},
        {u"mi", u"み"},
        {u"mye", u"みぇ"},
        {u"mya", u"みゃ"},
        {u"myu", u"みゅ"},
        {u"myo", u"みょ"},
        {u"mu", u"む"},
        {u"mwa", u"むぁ"},
        {u"mwi", u"むぃ"},
        {u"mwe", u"むぇ"},
        {u"mwo", u"むぉ"},
        {u"me", u"め"},
        {u"mo", u"も"},
        {u"ya", u"や"},
        {u"yu", u"ゆ"},
        {u"yo", u"よ"},
        {u"ra", u"ら"},
        {u"ri", u"り"},
        {u"rye", u"りぇ"},
        {u"rya", u"りゃ"},
        {u"ryu", u"りゅ"},
        {u"ryo", u"りょ"},
        {u"ru", u"る"},
        {u"rwa", u"るぁ"},
        {u"rwi", u"るぃ"},
        {u"rwe", u"るぇ"},
        {u"rwo", u"るぉ"},
        {u"re", u"れ"},
        {u"ro", u"ろ"},
        {u"wa", u"わ"},
        {u"o", u"を"},
        {u"n", u"ん"},
        {u"va", u"ゔぁ"},
        {u"vi", u"ゔぃ"},
        {u"vu", u"ゔ"},
        {u"ve", u"ゔぇ"},
        {u"vo", u"ゔぉ"}};

    static std::u16string convertKana(const std::u16string &kana, const KanaType kanaType) {
        std::u16string convertedKana;
        for (const char32_t kanaChar : kana) {
            if (kanaType == Hiragana) {
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
    static std::vector<std::u16string>
    convertKana(const std::vector<std::u16string> &kanaList, const KanaType kanaType) {
        std::vector<std::u16string> inputList;
        inputList.reserve(kanaList.size());
        for (const auto &kana : kanaList)
            inputList.emplace_back(kana);

        std::vector<std::u16string> convertedList;
        const std::regex rx("[\u3040-\u309F\u30A0-\u30FF]+");

        for (const auto &kana : kanaList) {
            std::u16string convertedKana;
            if (std::regex_match(u16strToUtf8str(kana), rx)) {
                for (const char32_t kanaChar : kana) {
                    if (kanaType == Hiragana) {
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

    std::string convertKana(const std::string &kanaStr, const KanaType &kanaType) {
        return u16strToUtf8str(convertKana(utf8strToU16str(kanaStr), kanaType));
    }

    std::vector<std::string> convertKana(const std::vector<std::string> &kanaList, const KanaType &kanaType) {
        std::vector<std::u16string> inputList;
        inputList.reserve(kanaList.size());
        for (const auto &item : kanaList) {
            inputList.emplace_back(utf8strToU16str(item));
        }

        const auto convertedList = convertKana(inputList, kanaType);
        std::vector<std::string> resultList;
        resultList.reserve(convertedList.size());
        for (const auto &item : convertedList) {
            resultList.emplace_back(u16strToUtf8str(item));
        }
        return resultList;
    }

    static RomajiResVector u8kanaToRomaji(const std::vector<std::u16string> &kanaList, const Error &error,
                                          const bool &doubleWrittenSokuon) {
        RomajiResVector res;
        const std::vector<std::u16string> inputList = convertKana(kanaList, Hiragana);
        std::vector<std::u16string> romajiList;

        // convert kana to romaji
        for (const std::u16string &kana : inputList) {
            if (kana != u"゜" && kana != u"ー") {
                if (const auto it = kanaToRomajiMap.find(kana); it != kanaToRomajiMap.end())
                    res.emplace_back(RomajiRes{u16strToUtf8str(kana), u16strToUtf8str(it->second), false});
                else if (error == Default)
                    res.emplace_back(RomajiRes{u16strToUtf8str(kana), u16strToUtf8str(kana), true});
            }
        }

        // double written sokuon
        for (int i = 0; i < res.size() - 1 && doubleWrittenSokuon; ++i) {
            char32_t nextChar = U' ';
            if (const auto it = romajiToKanaMap.find(utf8strToU16str(res[i].romaji)); it != romajiToKanaMap.end())
                nextChar = it->second.at(0);
            if (res[i].romaji == "cl" && isKana(nextChar) &&
                std::u32string(U"あいうえおアイウエオっんを").find(nextChar) == std::u32string::npos) {
                res[i].romaji.insert(res[i].romaji.begin(), res[i].romaji[0]);
                res.erase(res.begin() + i);
            }
        }

        return res;
    }

    RomajiResVector kanaToRomaji(const std::string &kanaStr, const Error &error, const bool doubleWrittenSokuon) {
        return u8kanaToRomaji(splitString(utf8strToU16str(kanaStr)), error, doubleWrittenSokuon);
    }

    RomajiResVector kanaToRomaji(const std::vector<std::string> &kanaList, const Error &error,
                                 const bool doubleWrittenSokuon) {
        std::vector<std::u16string> inputList;
        inputList.reserve(kanaList.size());
        for (const auto &item : kanaList) {
            inputList.emplace_back(utf8strToU16str(item));
        }
        return u8kanaToRomaji(inputList, error, doubleWrittenSokuon);
    }

    static KanaResVector u8romajiToKana(const std::vector<std::u16string> &romajiList, const Error &error,
                                        const KanaType &kanaType) {
        KanaResVector res;
        for (const std::u16string &romaji : romajiList) {
            const auto it = romajiToKanaMap.find(romaji);
            if (it != romajiToKanaMap.end()) {
                res.emplace_back(KanaRes{u16strToUtf8str(romaji),
                                         kanaType == KanaType::Hiragana
                                         ? u16strToUtf8str(it->second)
                                         : u16strToUtf8str(convertKana(it->second, KanaType::Katakana)),
                                         false});
            } else if (error == Error::Default)
                res.emplace_back(KanaRes{u16strToUtf8str(romaji), u16strToUtf8str(romaji), true});
        }
        return res;
    }

    KanaResVector romajiToKana(const std::string &romajiStr, const Error &error, const KanaType &kanaType) {
        const std::vector<std::u16string> input = splitString(utf8strToU16str(romajiStr));
        return u8romajiToKana(input, error, kanaType);
    }

    KanaResVector romajiToKana(const std::vector<std::string> &romajiList, const Error &error,
                               const KanaType &kanaType) {
        std::vector<std::u16string> inputList;
        inputList.reserve(romajiList.size());
        for (const auto &item : romajiList) {
            inputList.emplace_back(utf8strToU16str(item));
        }
        return u8romajiToKana(inputList, error, kanaType);
    }
}
