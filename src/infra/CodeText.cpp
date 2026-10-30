#include <cpp-kana2/core/Types.h>

namespace cpp_kana::v2
{

    const char *codeText(Code code) noexcept {
        switch (code) {
        case Code::Ok:
            return "ok";
        case Code::InvalidArgument:
            return "invalid argument";
        case Code::InvalidUtf8:
            return "invalid UTF-8";
        case Code::UnsupportedCharacter:
            return "unsupported character";
        case Code::NotReady:
            return "not ready";
        case Code::DataCorrupt:
            return "data corrupt";
        case Code::NotImplemented:
            return "not implemented";
        case Code::Internal:
            return "internal error";
        }
        // Unreachable for a well-formed enum, and still not UB for one that is not.
        return "unknown error";
    }

    ErrorException::ErrorException(Error error) :
        std::runtime_error(std::string(codeText(error.code)) + (error.detail.empty() ? "" : ": " + error.detail)),
        m_error(std::move(error)) {}

} // namespace cpp_kana::v2
