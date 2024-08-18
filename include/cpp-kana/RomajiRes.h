#ifndef ROMAJIRES_H
#define ROMAJIRES_H

#include <string>
#include <vector>

#include <cpp-kana/KanaGlobal.h>

namespace Kana
{
    struct CPP_KANA_EXPORT RomajiRes {
        std::string romaji;
        std::string kana;
        bool error = true; //  Whether the conversion failed.
    };

    class CPP_KANA_EXPORT RomajiResVector : public std::vector<RomajiRes> {
    public:
        // Convert KanaResVector to std::vector<std::string>
        std::vector<std::string> toStdVector() const;

        // Convert KanaResVector to std::string with delimiter
        std::string toStdStr(const std::string &delimiter = " ") const;
    };
}

#endif //ROMAJIRES_H
