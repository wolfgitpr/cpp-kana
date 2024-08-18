#include <cpp-kana/RomajiRes.h>

namespace Kana
{
    // Convert KanaResVector to std::vector<std::string>
    std::vector<std::string> RomajiResVector::toStdVector() const {
        std::vector<std::string> result;
        result.reserve(this->size());
        for (const auto &res : *this) {
            result.emplace_back(res.error ? res.romaji : res.kana);
        }
        return result;
    }

    // Convert KanaResVector to std::string with delimiter
    std::string RomajiResVector::toStdStr(const std::string &delimiter) const {
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
}
