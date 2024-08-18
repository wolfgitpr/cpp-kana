#include <regex>

#include <cpp-kana/Kana.h>

#include "Utils.h"

namespace Kana
{
    constexpr int hiraganaStart = 0x3041;
    constexpr int katakanaStart = 0x30A1;

    static const u32strHashMap<u32str, u32str> kanaToRomajiMap = {
        {U"っ", U"cl"},
        {U"あ", U"a"},
        {U"い", U"i"},
        {U"いぇ", U"ye"},
        {U"う", U"u"},
        {U"うぁ", U"wa"},
        {U"うぃ", U"wi"},
        {U"うぇ", U"we"},
        {U"うぉ", U"wo"},
        {U"え", U"e"},
        {U"お", U"o"},
        {U"か", U"ka"},
        {U"が", U"ga"},
        {U"き", U"ki"},
        {U"きぇ", U"kye"},
        {U"きゃ", U"kya"},
        {U"きゅ", U"kyu"},
        {U"きょ", U"kyo"},
        {U"ぎ", U"gi"},
        {U"ぎぇ", U"gye"},
        {U"ぎゃ", U"gya"},
        {U"ぎゅ", U"gyu"},
        {U"ぎょ", U"gyo"},
        {U"く", U"ku"},
        {U"くぁ", U"kwa"},
        {U"くぃ", U"kwi"},
        {U"くぇ", U"kwe"},
        {U"くぉ", U"kwo"},
        {U"ぐ", U"gu"},
        {U"ぐぁ", U"gwa"},
        {U"ぐぃ", U"gwi"},
        {U"ぐぇ", U"gwe"},
        {U"ぐぉ", U"gwo"},
        {U"け", U"ke"},
        {U"げ", U"ge"},
        {U"こ", U"ko"},
        {U"ご", U"go"},
        {U"さ", U"sa"},
        {U"ざ", U"za"},
        {U"し", U"shi"},
        {U"しぇ", U"she"},
        {U"しゃ", U"sha"},
        {U"しゅ", U"shu"},
        {U"しょ", U"sho"},
        {U"ぢ", U"ji"},
        {U"じ", U"ji"},
        {U"じぇ", U"je"},
        {U"じゃ", U"ja"},
        {U"じゅ", U"ju"},
        {U"じょ", U"jo"},
        {U"す", U"su"},
        {U"すぁ", U"swa"},
        {U"すぃ", U"swi"},
        {U"すぇ", U"swe"},
        {U"すぉ", U"swo"},
        {U"づ", U"zu"},
        {U"ず", U"zu"},
        {U"ずぁ", U"zwa"},
        {U"ずぃ", U"zwi"},
        {U"ずぇ", U"zwe"},
        {U"ずぉ", U"zwo"},
        {U"せ", U"se"},
        {U"ぜ", U"ze"},
        {U"そ", U"so"},
        {U"ぞ", U"zo"},
        {U"た", U"ta"},
        {U"だ", U"da"},
        {U"ち", U"chi"},
        {U"ちぇ", U"che"},
        {U"ちゃ", U"cha"},
        {U"ちゅ", U"chu"},
        {U"ちょ", U"cho"},
        {U"つ", U"tsu"},
        {U"つぁ", U"tsa"},
        {U"つぃ", U"tsi"},
        {U"つぇ", U"tse"},
        {U"つぉ", U"tso"},
        {U"て", U"te"},
        {U"てぃ", U"ti"},
        {U"てゅ", U"tyu"},
        {U"で", U"de"},
        {U"でぃ", U"di"},
        {U"でゅ", U"dyu"},
        {U"と", U"to"},
        {U"とぅ", U"tu"},
        {U"ど", U"do"},
        {U"どぅ", U"du"},
        {U"な", U"na"},
        {U"に", U"ni"},
        {U"にぇ", U"nye"},
        {U"にゃ", U"nya"},
        {U"にゅ", U"nyu"},
        {U"にょ", U"nyo"},
        {U"ぬ", U"nu"},
        {U"ぬぁ", U"nwa"},
        {U"ぬぃ", U"nwi"},
        {U"ぬぇ", U"nwe"},
        {U"ぬぉ", U"nwo"},
        {U"ね", U"ne"},
        {U"の", U"no"},
        {U"は", U"ha"},
        {U"ば", U"ba"},
        {U"ぱ", U"pa"},
        {U"ひ", U"hi"},
        {U"ひぇ", U"hye"},
        {U"ひゃ", U"hya"},
        {U"ひゅ", U"hyu"},
        {U"ひょ", U"hyo"},
        {U"び", U"bi"},
        {U"びぇ", U"bye"},
        {U"びゃ", U"bya"},
        {U"びゅ", U"byu"},
        {U"びょ", U"byo"},
        {U"ぴ", U"pi"},
        {U"ぴぇ", U"pye"},
        {U"ぴゃ", U"pya"},
        {U"ぴゅ", U"pyu"},
        {U"ぴょ", U"pyo"},
        {U"ふ", U"fu"},
        {U"ふぁ", U"fa"},
        {U"ふぃ", U"fi"},
        {U"ふぇ", U"fe"},
        {U"ふぉ", U"fo"},
        {U"ぶ", U"bu"},
        {U"ぶぁ", U"bwa"},
        {U"ぶぃ", U"bwi"},
        {U"ぶぇ", U"bwe"},
        {U"ぶぉ", U"bwo"},
        {U"ぷ", U"pu"},
        {U"ぷぁ", U"pwa"},
        {U"ぷぃ", U"pwi"},
        {U"ぷぇ", U"pwe"},
        {U"ぷぉ", U"pwo"},
        {U"へ", U"he"},
        {U"べ", U"be"},
        {U"ぺ", U"pe"},
        {U"ほ", U"ho"},
        {U"ぼ", U"bo"},
        {U"ぽ", U"po"},
        {U"ま", U"ma"},
        {U"み", U"mi"},
        {U"みぇ", U"mye"},
        {U"みゃ", U"mya"},
        {U"みゅ", U"myu"},
        {U"みょ", U"myo"},
        {U"む", U"mu"},
        {U"むぁ", U"mwa"},
        {U"むぃ", U"mwi"},
        {U"むぇ", U"mwe"},
        {U"むぉ", U"mwo"},
        {U"め", U"me"},
        {U"も", U"mo"},
        {U"や", U"ya"},
        {U"ゆ", U"yu"},
        {U"よ", U"yo"},
        {U"ら", U"ra"},
        {U"り", U"ri"},
        {U"りぇ", U"rye"},
        {U"りゃ", U"rya"},
        {U"りゅ", U"ryu"},
        {U"りょ", U"ryo"},
        {U"る", U"ru"},
        {U"るぁ", U"rwa"},
        {U"るぃ", U"rwi"},
        {U"るぇ", U"rwe"},
        {U"るぉ", U"rwo"},
        {U"れ", U"re"},
        {U"ろ", U"ro"},
        {U"わ", U"wa"},
        {U"を", U"o"},
        {U"ん", U"N"},
        {U"ゔぁ", U"va"},
        {U"ゔぃ", U"vi"},
        {U"ゔ", U"vu"},
        {U"ゔぇ", U"ve"},
        {U"ゔぉ", U"vo"}};

    static const u32strHashMap<u32str, u32str> romajiToKanaMap = {
        {U"cl", U"っ"},
        {U"a", U"あ"},
        {U"i", U"い"},
        {U"ye", U"いぇ"},
        {U"u", U"う"},
        {U"wa", U"うぁ"},
        {U"wi", U"うぃ"},
        {U"we", U"うぇ"},
        {U"wo", U"うぉ"},
        {U"e", U"え"},
        {U"o", U"お"},
        {U"ka", U"か"},
        {U"ga", U"が"},
        {U"ki", U"き"},
        {U"kye", U"きぇ"},
        {U"kya", U"きゃ"},
        {U"kyu", U"きゅ"},
        {U"kyo", U"きょ"},
        {U"gi", U"ぎ"},
        {U"gye", U"ぎぇ"},
        {U"gya", U"ぎゃ"},
        {U"gyu", U"ぎゅ"},
        {U"gyo", U"ぎょ"},
        {U"ku", U"く"},
        {U"kwa", U"くぁ"},
        {U"kwi", U"くぃ"},
        {U"kwe", U"くぇ"},
        {U"kwo", U"くぉ"},
        {U"gu", U"ぐ"},
        {U"gwa", U"ぐぁ"},
        {U"gwi", U"ぐぃ"},
        {U"gwe", U"ぐぇ"},
        {U"gwo", U"ぐぉ"},
        {U"ke", U"け"},
        {U"ge", U"げ"},
        {U"ko", U"こ"},
        {U"go", U"ご"},
        {U"sa", U"さ"},
        {U"za", U"ざ"},
        {U"shi", U"し"},
        {U"she", U"しぇ"},
        {U"sha", U"しゃ"},
        {U"shu", U"しゅ"},
        {U"sho", U"しょ"},
        {U"ji", U"ぢ"},
        {U"ji", U"じ"},
        {U"je", U"じぇ"},
        {U"ja", U"じゃ"},
        {U"ju", U"じゅ"},
        {U"jo", U"じょ"},
        {U"su", U"す"},
        {U"swa", U"すぁ"},
        {U"swi", U"すぃ"},
        {U"swe", U"すぇ"},
        {U"swo", U"すぉ"},
        {U"zu", U"づ"},
        {U"zu", U"ず"},
        {U"zwa", U"ずぁ"},
        {U"zwi", U"ずぃ"},
        {U"zwe", U"ずぇ"},
        {U"zwo", U"ずぉ"},
        {U"se", U"せ"},
        {U"ze", U"ぜ"},
        {U"so", U"そ"},
        {U"zo", U"ぞ"},
        {U"ta", U"た"},
        {U"da", U"だ"},
        {U"chi", U"ち"},
        {U"che", U"ちぇ"},
        {U"cha", U"ちゃ"},
        {U"chu", U"ちゅ"},
        {U"cho", U"ちょ"},
        {U"tsu", U"つ"},
        {U"tsa", U"つぁ"},
        {U"tsi", U"つぃ"},
        {U"tse", U"つぇ"},
        {U"tso", U"つぉ"},
        {U"te", U"て"},
        {U"ti", U"てぃ"},
        {U"tyu", U"てゅ"},
        {U"de", U"で"},
        {U"di", U"でぃ"},
        {U"dyu", U"でゅ"},
        {U"to", U"と"},
        {U"tu", U"とぅ"},
        {U"do", U"ど"},
        {U"du", U"どぅ"},
        {U"na", U"な"},
        {U"ni", U"に"},
        {U"nye", U"にぇ"},
        {U"nya", U"にゃ"},
        {U"nyu", U"にゅ"},
        {U"nyo", U"にょ"},
        {U"nu", U"ぬ"},
        {U"nwa", U"ぬぁ"},
        {U"nwi", U"ぬぃ"},
        {U"nwe", U"ぬぇ"},
        {U"nwo", U"ぬぉ"},
        {U"ne", U"ね"},
        {U"no", U"の"},
        {U"ha", U"は"},
        {U"ba", U"ば"},
        {U"pa", U"ぱ"},
        {U"hi", U"ひ"},
        {U"hye", U"ひぇ"},
        {U"hya", U"ひゃ"},
        {U"hyu", U"ひゅ"},
        {U"hyo", U"ひょ"},
        {U"bi", U"び"},
        {U"bye", U"びぇ"},
        {U"bya", U"びゃ"},
        {U"byu", U"びゅ"},
        {U"byo", U"びょ"},
        {U"pi", U"ぴ"},
        {U"pye", U"ぴぇ"},
        {U"pya", U"ぴゃ"},
        {U"pyu", U"ぴゅ"},
        {U"pyo", U"ぴょ"},
        {U"fu", U"ふ"},
        {U"fa", U"ふぁ"},
        {U"fi", U"ふぃ"},
        {U"fe", U"ふぇ"},
        {U"fo", U"ふぉ"},
        {U"bu", U"ぶ"},
        {U"bwa", U"ぶぁ"},
        {U"bwi", U"ぶぃ"},
        {U"bwe", U"ぶぇ"},
        {U"bwo", U"ぶぉ"},
        {U"pu", U"ぷ"},
        {U"pwa", U"ぷぁ"},
        {U"pwi", U"ぷぃ"},
        {U"pwe", U"ぷぇ"},
        {U"pwo", U"ぷぉ"},
        {U"he", U"へ"},
        {U"be", U"べ"},
        {U"pe", U"ぺ"},
        {U"ho", U"ほ"},
        {U"bo", U"ぼ"},
        {U"po", U"ぽ"},
        {U"ma", U"ま"},
        {U"mi", U"み"},
        {U"mye", U"みぇ"},
        {U"mya", U"みゃ"},
        {U"myu", U"みゅ"},
        {U"myo", U"みょ"},
        {U"mu", U"む"},
        {U"mwa", U"むぁ"},
        {U"mwi", U"むぃ"},
        {U"mwe", U"むぇ"},
        {U"mwo", U"むぉ"},
        {U"me", U"め"},
        {U"mo", U"も"},
        {U"ya", U"や"},
        {U"yu", U"ゆ"},
        {U"yo", U"よ"},
        {U"ra", U"ら"},
        {U"ri", U"り"},
        {U"rye", U"りぇ"},
        {U"rya", U"りゃ"},
        {U"ryu", U"りゅ"},
        {U"ryo", U"りょ"},
        {U"ru", U"る"},
        {U"rwa", U"るぁ"},
        {U"rwi", U"るぃ"},
        {U"rwe", U"るぇ"},
        {U"rwo", U"るぉ"},
        {U"re", U"れ"},
        {U"ro", U"ろ"},
        {U"wa", U"わ"},
        {U"o", U"を"},
        {U"N", U"ん"},
        {U"va", U"ゔぁ"},
        {U"vi", U"ゔぃ"},
        {U"vu", U"ゔ"},
        {U"ve", U"ゔぇ"},
        {U"vo", U"ゔぉ"}};

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
