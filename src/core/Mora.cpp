#include <cpp-kana2/core/Mora.h>

namespace cpp_kana::v2
{

    Conversion::Conversion(std::string text, std::vector<Mora> moras, std::string separator) :
        m_text(std::move(text)), m_moras(std::move(moras)), m_separator(std::move(separator)) {}

    std::string Conversion::rendered(const char *separator) const {
        std::string out;
        const char *sep = separator != nullptr ? separator : "";
        bool first = true;
        for (const Mora &mora : m_moras) {
            // A mora that renders nothing (a dropped sokuon, a dropped long vowel) is not a field: joining it
            // would put the separator at the start or the end of the string. MEASURED before this:
            // kanaToRomaji("ーあ", Space) produced " a" and kanaToRomaji("あっ", Space) produced "a ".
            if (mora.rendered.empty()) {
                continue;
            }
            if (!first) {
                out += sep;
            }
            out += mora.rendered;
            first = false;
        }
        return out;
    }

    bool Conversion::ok() const noexcept {
        for (const Mora &mora : m_moras) {
            if (mora.code != Code::Ok) {
                return false;
            }
        }
        return true;
    }

} // namespace cpp_kana::v2
