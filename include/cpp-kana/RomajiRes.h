#ifndef KANARES_H
#define KANARES_H

#include <string>
#include <vector>

#include <cpp-kana/KanaGlobal.h>

namespace Kana
{
    struct CPP_KANA_EXPORT RomajiRes {
        std::string kana;
        std::string romaji;
        bool error = true; //  Whether the conversion failed.
    };

    class CPP_KANA_EXPORT RomajiResVector : public std::vector<RomajiRes> {
    public:
        // Convert RomajiResVector to std::vector<std::string>
        std::vector<std::string> toStdVector() const;

        // Convert RomajiResVector to std::string with delimiter
        std::string toStdStr(const std::string &delimiter = " ") const;
    };
}

#endif //KANARES_H
