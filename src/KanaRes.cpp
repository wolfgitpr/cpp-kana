#include <cpp-kana/KanaRes.h>

namespace Kana
{
    // Convert KanaResVector to std::vector<std::string>
    std::vector<std::string> KanaResVector::toStdVector() const {
        std::vector<std::string> result;
        result.reserve(this->size());
        for (const auto &[romaji, kana, error] : *this) {
            result.emplace_back(error ? romaji : kana);
        }
        return result;
    }

    // Convert KanaResVector to std::string with delimiter
    std::string KanaResVector::toStdStr(const std::string &delimiter) const {
        std::string result;
        bool first = true;

        for (const auto &[romaji, kana, error] : *this) {
            if (!first) {
                result += delimiter;
            }
            result += error ? romaji : kana;
            first = false;
        }

        return result;
    }
}
