#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

#include <cpp-kana/KanaGlobal.h>

namespace Kana
{
    std::vector<std::u16string> splitString(const std::u16string &input);

    bool CPP_KANA_EXPORT isLetter(const char16_t &c);

    bool CPP_KANA_EXPORT isHanzi(const char16_t &c);

    bool CPP_KANA_EXPORT isKana(const char16_t &c);

    bool CPP_KANA_EXPORT isDigit(const char16_t &c);

    bool CPP_KANA_EXPORT isSpace(const char16_t &c);

    bool CPP_KANA_EXPORT isSpecialKana(const char16_t &c);
}
#endif //UTIL_H
