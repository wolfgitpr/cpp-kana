#ifndef JPG2P_H
#define JPG2P_H

#include <cpp-kana/KanaGlobal.h>
#include <cpp-kana/KanaRes.h>
#include <cpp-kana/RomajiRes.h>

namespace Kana
{
    enum CPP_KANA_EXPORT Error {
        // Keep original characters
        Default = 0,
        // Ignore this character (do not export)
        Ignore = 1
    };

    enum CPP_KANA_EXPORT KanaType { Hiragana, Katakana };

    KanaResVector CPP_KANA_EXPORT kanaToRomaji(const std::string &kanaStr, Error error = Default,
                                               bool doubleWrittenSokuon = false);
    KanaResVector CPP_KANA_EXPORT kanaToRomaji(const std::vector<std::string> &kanaList, Error error = Default,
                                               bool doubleWrittenSokuon = false);

    RomajiResVector CPP_KANA_EXPORT romajiToKana(const std::string &romajiStr, Error error = Default,
                                                 KanaType kanaType = Hiragana);
    RomajiResVector CPP_KANA_EXPORT romajiToKana(const std::vector<std::string> &romajiList,
                                                 Error error = Default,
                                                 KanaType kanaType = Hiragana);

    std::string CPP_KANA_EXPORT convertKana(const std::string &kanaStr, KanaType kanaType);

    std::vector<std::string> CPP_KANA_EXPORT convertKana(const std::vector<std::string> &kanaStr, KanaType kanaType);

}

#endif // JPG2P_H
