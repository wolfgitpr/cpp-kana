#ifndef CPP_KANA2_CORE_CONVERT_H
#define CPP_KANA2_CORE_CONVERT_H

#include <string>
#include <string_view>

#include <cpp-kana2/core/Global.h>
#include <cpp-kana2/core/Mora.h>
#include <cpp-kana2/core/Types.h>
#include <cpp-kana2/core/Version.h>

namespace cpp_kana::v2
{

    // The four entry points. All of them take and return UTF-8, and all of them return an Outcome: a
    // failure is a value, never an exception (v1 threw std::invalid_argument out of its UTF-8 decoder).
    //
    // The library is stateless -- the mora table is compiled in and never mutated -- so these are free
    // functions rather than an engine object. That also means concurrent calls are safe with no locking;
    // docs/CONTRACT.md states it as the contract rather than as an implementation detail.

    /// Kana -> romaji. Katakana, half-width katakana and combining dakuten are normalised first, so all
    /// three spellings of the same text produce the same romanisation.
    CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<Conversion> kanaToRomaji(std::string_view kana,
                                                                          const RomajiOptions &options = {});

    /// Romaji -> kana. Accepts the Hepburn spellings plus the input aliases the table declares (si/shi,
    /// zi/ji, dji/dzu for ぢ/づ, hu/fu, sya/sha, zya/jya for じゃ, ...), sokuon by doubled consonant and
    /// hatsuon by nn / n' / n-before-consonant. `ti` and `tu` are NOT aliases of ち/つ: they are the canonical
    /// spellings of てぃ/とぅ, because the table refuses to let one romaji mean two kana.
    CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<Conversion> romajiToKana(std::string_view romaji,
                                                                          const KanaOptions &options = {});

    /// Hiragana <-> katakana. Characters that are not kana are left alone (and recorded as MoraKind::Other),
    /// which is what makes this usable on mixed text.
    CPP_KANA2_NODISCARD CPP_KANA2_EXPORT Outcome<Conversion> convertScript(std::string_view text, Script target,
                                                                           UnknownPolicy unknown = UnknownPolicy::Keep);

    /// The package version, e.g. "2.0.0". The same fact is available at compile time as
    /// CPP_KANA2_VERSION_STRING; tools/check_version_tokens.py keeps the two and `project(... VERSION ...)`
    /// in step.
    CPP_KANA2_EXPORT const char *version() noexcept;

} // namespace cpp_kana::v2

#endif // CPP_KANA2_CORE_CONVERT_H
