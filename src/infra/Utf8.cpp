#include <cpp-kana2/core/Utf8.h>

#include <cstddef>

namespace cpp_kana::v2
{
    namespace
    {

        Error Invalid(std::size_t offset, const char *what) {
            return Error{Code::InvalidUtf8, std::string(what) + " at byte offset " + std::to_string(offset)};
        }

        /// Decodes one sequence starting at `i`. Returns the number of bytes consumed, or 0 on a violation
        /// (with `error` filled).
        std::size_t DecodeOne(std::string_view utf8, std::size_t i, char32_t *out, Error *error) {
            const auto lead = static_cast<unsigned char>(utf8[i]);
            if (lead < 0x80) {
                *out = lead;
                return 1;
            }

            std::size_t extra = 0;
            char32_t codePoint = 0;
            char32_t minimum = 0;
            if ((lead & 0xE0U) == 0xC0U) {
                extra = 1;
                codePoint = lead & 0x1FU;
                minimum = 0x80;
            } else if ((lead & 0xF0U) == 0xE0U) {
                extra = 2;
                codePoint = lead & 0x0FU;
                minimum = 0x800;
            } else if ((lead & 0xF8U) == 0xF0U) {
                extra = 3;
                codePoint = lead & 0x07U;
                minimum = 0x10000;
            } else {
                *error = Invalid(i, "invalid UTF-8 lead byte");
                return 0;
            }

            if (i + extra >= utf8.size()) {
                *error = Invalid(i, "truncated UTF-8 sequence");
                return 0;
            }
            for (std::size_t k = 1; k <= extra; ++k) {
                const auto cont = static_cast<unsigned char>(utf8[i + k]);
                if ((cont & 0xC0U) != 0x80U) {
                    *error = Invalid(i + k, "invalid UTF-8 continuation byte");
                    return 0;
                }
                codePoint = (codePoint << 6U) | (cont & 0x3FU);
            }
            // A decoder that accepts these three forms is how a library ends up with two spellings of the same
            // character, so they are refused rather than normalised.
            if (codePoint < minimum) {
                *error = Invalid(i, "overlong UTF-8 sequence");
                return 0;
            }
            if (codePoint > 0x10FFFF) {
                *error = Invalid(i, "code point above U+10FFFF");
                return 0;
            }
            if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
                *error = Invalid(i, "UTF-16 surrogate encoded as UTF-8");
                return 0;
            }
            *out = codePoint;
            return extra + 1;
        }

    } // namespace

    Outcome<std::u32string> utf8ToCodepoints(std::string_view utf8) {
        std::u32string out;
        out.reserve(utf8.size());
        std::size_t i = 0;
        while (i < utf8.size()) {
            char32_t codePoint = 0;
            Error error;
            const std::size_t consumed = DecodeOne(utf8, i, &codePoint, &error);
            if (consumed == 0) {
                return error;
            }
            out.push_back(codePoint);
            i += consumed;
        }
        return out;
    }

    Outcome<std::string> codepointsToUtf8(std::u32string_view codepoints) {
        std::string out;
        out.reserve(codepoints.size());
        for (const char32_t codePoint : codepoints) {
            if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
                return Error{Code::InvalidArgument, "code point out of range: " + std::to_string(codePoint)};
            }
            if (codePoint < 0x80) {
                out.push_back(static_cast<char>(codePoint));
            } else if (codePoint < 0x800) {
                out.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            } else if (codePoint < 0x10000) {
                out.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            } else {
                out.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }
        return out;
    }

    bool isValidUtf8(std::string_view utf8) noexcept {
        std::size_t i = 0;
        while (i < utf8.size()) {
            char32_t codePoint = 0;
            Error error;
            const std::size_t consumed = DecodeOne(utf8, i, &codePoint, &error);
            if (consumed == 0) {
                return false;
            }
            i += consumed;
        }
        return true;
    }

} // namespace cpp_kana::v2
