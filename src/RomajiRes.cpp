#include <cpp-kana/RomajiRes.h>

namespace Kana
{
    // Convert RomajiResVector to std::vector<std::string>
    std::vector<std::string> RomajiResVector::toStdVector() const {
        std::vector<std::string> result;
        result.reserve(this->size());
        for (const auto &[kana, romaji, error] : *this) {
            result.emplace_back(error ? kana : romaji);
        }
        return result;
    }

    // Convert RomajiResVector to std::string with delimiter
    std::string RomajiResVector::toStdStr(const std::string &delimiter) const {
        std::string result;
        bool first = true;

        for (const auto &[kana, romaji, error] : *this) {
            if (!first) {
                result += delimiter;
            }
            result += error ? kana : romaji;
            first = false;
        }

        return result;
    }
}
