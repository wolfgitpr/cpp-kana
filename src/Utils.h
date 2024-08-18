#ifndef UTIL_H
#define UTIL_H

#include <vector>

#include "U32Str.h"

namespace Kana
{
    u32strVec splitString(const u32str &input);

    bool isLetter(char32_t c);

    bool isKana(char32_t c);

    bool isDigit(char32_t c);

    bool isSpace(char32_t c);

    bool isSpecialKana(char32_t c);
}
#endif //UTIL_H
