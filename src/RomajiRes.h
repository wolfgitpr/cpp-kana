#ifndef ROMAJIRES_H
#define ROMAJIRES_H

#include <string>
#include <vector>

namespace Kana
{
    struct RomajiRes {
        std::string romaji;
        std::string kana;
        bool error = true; //  Whether the conversion failed.
    };

    class RomajiResVector : public std::vector<RomajiRes> {
    public:
        // Convert KanaResVector to std::vector<std::string>
        inline std::vector<std::string> toStdVector() const {
            std::vector<std::string> result;
            result.reserve(this->size());
            for (const auto &res : *this) {
                result.emplace_back(res.error ? res.romaji : res.kana);
            }
            return result;
        }

        // Convert KanaResVector to std::string with delimiter
        inline std::string toStdStr(const std::string &delimiter = " ") const {
            std::string result;
            bool first = true;

            for (const auto &res : *this) {
                if (!first) {
                    result += delimiter;
                }
                result += res.error ? res.romaji : res.kana;
                first = false;
            }

            return result;
        }
    };
}

#endif //ROMAJIRES_H
