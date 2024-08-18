#ifndef KANARES_H
#define KANARES_H

#include <string>
#include <vector>

#include <cpp-kana/KanaGlobal.h>

namespace Kana
{
    struct CPP_KANA_EXPORT KanaRes {
        std::string kana;
        std::string romaji;
        bool error = true; //  Whether the conversion failed.
    };

    class CPP_KANA_EXPORT KanaResVector : public std::vector<KanaRes> {
    public:
        // Convert KanaResVector to std::vector<std::string>
        std::vector<std::string> toStdVector() const;

        // Convert KanaResVector to std::string with delimiter
        std::string toStdStr(const std::string &delimiter = " ") const;
    };
}

#endif //KANARES_H
