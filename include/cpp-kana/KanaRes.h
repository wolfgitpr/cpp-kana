#ifndef KANARES_H
#define KANARES_H

#include <string>
#include <vector>

namespace Kana
{
    struct KanaRes {
        std::string kana;
        std::string romaji;
        bool error = true; //  Whether the conversion failed.
    };

    class KanaResVector : public std::vector<KanaRes> {
    public:
        // Convert KanaResVector to std::vector<std::string>
        inline std::vector<std::string> toStdVector() const {
            std::vector<std::string> result;
            result.reserve(this->size());
            for (const auto &res : *this) {
                result.emplace_back(res.error ? res.kana : res.romaji);
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
                result += res.error ? res.kana : res.romaji;
                first = false;
            }

            return result;
        }
    };
}

#endif //KANARES_H
