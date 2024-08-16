#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

#include <tinyutf8.h>

typedef tiny_utf8::string u8string;
typedef std::vector<tiny_utf8::string> u8stringlist;

namespace Kana
{
    std::vector<std::string> split(const std::string &s, const std::string &delimiter);

    u8stringlist splitString(const u8string &input);

    std::vector<std::string> toStdList(const u8stringlist &input);

    bool isLetter(char32_t c);

    bool isKana(char32_t c);

    bool isDigit(char32_t c);

    bool isSpace(char32_t c);

    bool isSpecialKana(char32_t c);
}
#endif //UTIL_H
