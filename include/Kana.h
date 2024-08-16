#ifndef JPG2P_H
#define JPG2P_H

#include <tinyutf8.h>

typedef tiny_utf8::string u8string;
typedef std::vector<tiny_utf8::string> u8stringlist;

#include "../src/KanaRes.h"
#include "../src/RomajiRes.h"

namespace Kana
{
    enum class Error {
        // Keep original characters
        Default = 0,
        // Ignore this character (do not export)
        Ignore = 1
    };

    enum class KanaType { Hiragana, Katakana };

    KanaResVector kanaToRomaji(const std::string &kanaStr, Error error = Error::Default,
                               const bool &doubleWrittenSokuon = false);
    KanaResVector kanaToRomaji(const std::vector<std::string> &kanaList, const bool &doubleWrittenSokuon = false);

    RomajiResVector romajiToKana(const std::string &romajiStr, Error error = Error::Default,
                                 KanaType kanaType = KanaType::Hiragana);
    RomajiResVector romajiToKana(const std::vector<std::string> &romajiList, Error error = Error::Default,
                                 KanaType kanaType = KanaType::Hiragana);

    std::string convertKana(const std::string &kanaStr, KanaType kanaType);

}

#endif // JPG2P_H
