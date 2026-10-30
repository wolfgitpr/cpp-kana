#ifndef CPP_KANA2_CORE_TYPES_H
#define CPP_KANA2_CORE_TYPES_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <cpp-kana2/core/Global.h>

namespace cpp_kana::v2
{

    // ---------------------------------------------------------------------------------------------------
    // Error model
    //
    // There is exactly ONE error channel: `Outcome<T>`. Values 0..7 are frozen and may only be appended
    // to -- never renumbered -- because the C ABI mirrors them as integers (ext/c_api.h) and a binding that
    // compiled against 2.0 must keep reading the same numbers for the same conditions.
    // ---------------------------------------------------------------------------------------------------

    enum class Code : std::int32_t {
        Ok = 0,
        /// A parameter was unusable (for example a null pointer on the C face, or an out-of-range enum).
        InvalidArgument = 1,
        /// The input was not well-formed UTF-8 (truncated, overlong, or a lone surrogate).
        InvalidUtf8 = 2,
        /// A character was well-formed but is not in the mora table. Whether this is an error depends on
        /// UnknownPolicy: `Keep` and `Ignore` record it per mora and still succeed, `Fail` fails the call.
        UnsupportedCharacter = 3,
        /// The engine is not usable: it was moved from, or its table did not load.
        NotReady = 4,
        /// The compiled-in or installed table is internally inconsistent (duplicate key, empty table).
        DataCorrupt = 5,
        /// A documented capability that this build does not provide.
        NotImplemented = 6,
        /// A C++ exception was caught at the boundary. The only producer is `catch (...)`.
        Internal = 7,
    };

    /// `Code` as text. This function is the single source of message text: the C face's `cppk2_code_text`
    /// and both language shells call it rather than keeping their own copy of the wording.
    CPP_KANA2_EXPORT const char *codeText(Code code) noexcept;

    struct Error {
        Code code = Code::Ok;
        std::string detail;
    };

    /// Thrown by `Outcome::value()` when the outcome is a failure. A failed call that the caller never asks
    /// about itself throws nothing: the failure is the returned value.
    class CPP_KANA2_EXPORT ErrorException : public std::runtime_error {
    public:
        explicit ErrorException(Error error);

        const Error &error() const noexcept { return m_error; }

    private:
        Error m_error;
    };

    namespace detail
    {
        // (empty: the outcome storage lives in Outcome itself, so there is one place that decides it)
    } // namespace detail

    /// `Outcome<T>` -- "a T, or an Error". Never both, never neither.
    template <typename T>
    class Outcome {
    public:
        Outcome(T value) : m_value(std::move(value)) {} // NOLINT(google-explicit-constructor)
        Outcome(Error error) : m_value(std::move(error)) {} // NOLINT(google-explicit-constructor)

        bool ok() const noexcept { return std::holds_alternative<T>(m_value); }
        explicit operator bool() const noexcept { return ok(); }

        /// The value. Throws ErrorException when this outcome is a failure -- asking for the wrong side is a
        /// programming error, not a library outcome, so it is not expressed as a Code.
        T &value() {
            if (const auto *error = std::get_if<Error>(&m_value)) {
                throw ErrorException(*error);
            }
            return std::get<T>(m_value);
        }
        const T &value() const {
            if (const auto *error = std::get_if<Error>(&m_value)) {
                throw ErrorException(*error);
            }
            return std::get<T>(m_value);
        }

        /// The failure. Throws std::bad_variant_access on a success, exactly as std::variant does.
        const Error &error() const { return std::get<Error>(m_value); }

    private:
        std::variant<T, Error> m_value;
    };

    /// `Outcome<void>` -- "succeeded, nothing to return"; `error()` returns a default `Error` instead of
    /// throwing, because there is no alternative to get wrong.
    template <>
    class Outcome<void> {
    public:
        Outcome() = default; // NOLINT(google-explicit-constructor)
        Outcome(Error error) : m_error(std::move(error)) {} // NOLINT(google-explicit-constructor)

        bool ok() const noexcept { return m_error.code == Code::Ok; }
        explicit operator bool() const noexcept { return ok(); }

        const Error &error() const noexcept { return m_error; }

    private:
        Error m_error;
    };

    // ---------------------------------------------------------------------------------------------------
    // Options
    //
    // These are the only knobs. There is deliberately no "scheme" dimension: the romanisation implemented
    // here is Hepburn, and what varies between the way people write it down is captured by these policies
    // (docs/DECISIONS.md D1/D10).
    // ---------------------------------------------------------------------------------------------------

    /// Target script for romaji -> kana, and the argument of convertScript().
    enum class Script : std::int32_t {
        Hiragana = 0,
        Katakana = 1,
        /// Leave each kana in the script it was given (convertScript only).
        AsIs = 2,
    };

    /// What a sokuon (っ) becomes. `Geminate` doubles the next consonant (きって -> kitte, まっちゃ -> matcha);
    /// `Drop` renders nothing for it.
    enum class SokuonPolicy : std::int32_t {
        Geminate = 0,
        Drop = 1,
    };

    /// What a hatsuon (ん) becomes before a vowel or `y`. `Apostrophe` writes n' (しんよう -> shin'you);
    /// `Plain` always writes n.
    enum class HatsuonPolicy : std::int32_t {
        Apostrophe = 0,
        Plain = 1,
    };

    /// What a long vowel becomes -- both the ー mark and a vowel sequence such as おう, when the source has a
    /// mark. `Repeat` repeats the previous vowel (コーヒー -> koohii), `Drop` writes nothing (kohi),
    /// `KatakanaDash` writes `-` (ko-hi-).
    enum class LongVowelPolicy : std::int32_t {
        Repeat = 0,
        Drop = 1,
        KatakanaDash = 2,
    };

    enum class CasePolicy : std::int32_t {
        Lower = 0,
        Upper = 1,
        Capitalize = 2,
    };

    /// What separates rendered morae. `None` (compact) is the default: v1 wrote morae one space apart, which
    /// turned がっこう into "ga cl ko u". `Space` is still available for the per-mora view as text.
    enum class SeparatorPolicy : std::int32_t {
        Space = 0,
        None = 1,
    };

    /// What happens to a character that is not a kana and not an ASCII letter: `Keep` renders it unchanged
    /// and records Code::UnsupportedCharacter on that mora, `Ignore` drops it, `Fail` fails the call.
    enum class UnknownPolicy : std::int32_t {
        Keep = 0,
        Ignore = 1,
        Fail = 2,
    };

    /// Options for kanaToRomaji(). Member order is part of the frozen surface (tests/abi/).
    struct RomajiOptions {
        SokuonPolicy sokuon = SokuonPolicy::Geminate;
        HatsuonPolicy hatsuon = HatsuonPolicy::Apostrophe;
        LongVowelPolicy longVowel = LongVowelPolicy::Repeat;
        CasePolicy letterCase = CasePolicy::Lower;
        // Compact by default: a romanisation is something people paste into a file name or a URL, and v1's
        // spaced appearance ("ga cl ko u") was an artefact of rendering morae one at a time rather than a
        // useful choice. SeparatorPolicy::Space stays available for the per-mora view as text.
        SeparatorPolicy separator = SeparatorPolicy::None;
        UnknownPolicy unknown = UnknownPolicy::Keep;
    };

    /// Options for romajiToKana(). Member order is part of the frozen surface (tests/abi/).
    struct KanaOptions {
        Script script = Script::Hiragana;
        UnknownPolicy unknown = UnknownPolicy::Keep;
    };

} // namespace cpp_kana::v2

#endif // CPP_KANA2_CORE_TYPES_H
