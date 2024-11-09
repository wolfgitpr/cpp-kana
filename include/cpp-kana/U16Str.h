#ifndef U32STR_H
#define U32STR_H

#include <string>

#include <cpp-kana/KanaGlobal.h>

namespace Kana
{
    std::string CPP_KANA_EXPORT u16strToUtf8str(const char16_t &ch16);
    std::string CPP_KANA_EXPORT u16strToUtf8str(const std::u16string &u16str);
    std::u16string CPP_KANA_EXPORT utf8strToU16str(const std::string &utf8str);
}
#endif //U32STR_H
