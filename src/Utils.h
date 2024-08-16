#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

#include "U32Str.h"

namespace Kana
{
    std::vector<std::string> split(const std::string &s, const std::string &delimiter);

    u32strVec splitString(const u32str &input);

    std::vector<std::string> toStdList(const u32strVec &input);

    bool isLetter(char32_t c);

    bool isKana(char32_t c);

    bool isDigit(char32_t c);

    bool isSpace(char32_t c);

    bool isSpecialKana(char32_t c);
}
#endif //UTIL_H
