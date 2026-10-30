#include <cpp-kana2/ext/c_api.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Types.h>

using cpp_kana::v2::Code;
using cpp_kana::v2::Conversion;
using cpp_kana::v2::KanaOptions;
using cpp_kana::v2::Mora;
using cpp_kana::v2::Outcome;
using cpp_kana::v2::RomajiOptions;
using cpp_kana::v2::Script;
using cpp_kana::v2::UnknownPolicy;

namespace {

char *Dup(const std::string &text) {
    auto *buffer = static_cast<char *>(std::malloc(text.size() + 1));
    if (buffer == nullptr) {
        return nullptr;
    }
    std::memcpy(buffer, text.data(), text.size());
    buffer[text.size()] = '\0';
    return buffer;
}

void SetError(cppk2_error *error, Code code, const std::string &detail) noexcept {
    if (error == nullptr) {
        return;
    }
    error->code = static_cast<int32_t>(code);
    error->detail = Dup(detail);
}

int32_t Fail(cppk2_error *error, Code code, const std::string &detail) {
    SetError(error, code, detail);
    return static_cast<int32_t>(code);
}

/// Builds the C view of a conversion. Ownership: the struct owns text, rendered, and every string the
/// moras point at, plus the array itself; cppk2_conversion_free releases all of it with the same allocator.
cppk2_conversion *ToC(const Conversion &conversion) {
    auto *out = static_cast<cppk2_conversion *>(std::malloc(sizeof(cppk2_conversion)));
    if (out == nullptr) {
        return nullptr;
    }
    out->text = nullptr;
    out->rendered = nullptr;
    out->moras = nullptr;
    out->mora_count = 0;
    out->ok = 0;

    const std::string rendered = conversion.rendered();
    out->text = Dup(conversion.text());
    out->rendered = Dup(rendered);

    const auto count = conversion.moras().size();
    if (count != 0) {
        out->moras = static_cast<cppk2_mora *>(std::malloc(sizeof(cppk2_mora) * count));
        if (out->moras == nullptr) {
            cppk2_conversion_free(out);
            return nullptr;
        }
        // Set the count BEFORE filling so that a failed Dup half-way is still released by the free path
        // (the entries not yet written are zero-filled, and free(NULL) is a no-op).
        std::memset(out->moras, 0, sizeof(cppk2_mora) * count);
        out->mora_count = static_cast<int32_t>(count);
        for (std::size_t i = 0; i < count; ++i) {
            const Mora &mora = conversion.moras()[i];
            out->moras[i].text = Dup(mora.text);
            out->moras[i].rendered = Dup(mora.rendered);
            out->moras[i].kind = static_cast<int32_t>(mora.kind);
            out->moras[i].code = static_cast<int32_t>(mora.code);
            if (out->moras[i].text == nullptr || out->moras[i].rendered == nullptr) {
                cppk2_conversion_free(out);
                return nullptr;
            }
        }
    }
    out->ok = conversion.ok() ? 1 : 0;

    if (out->text == nullptr || out->rendered == nullptr) {
        cppk2_conversion_free(out);
        return nullptr;
    }
    return out;
}

template <typename T>
int32_t Deliver(Outcome<Conversion> outcome, T **out, cppk2_error *error) {
    if (!outcome.ok()) {
        return Fail(error, outcome.error().code, outcome.error().detail);
    }
    cppk2_conversion *conversion = ToC(outcome.value());
    if (conversion == nullptr) {
        return Fail(error, Code::Internal, "out of memory while building the result");
    }
    *out = conversion;
    return CPPK2_OK;
}

bool ValidOptionsSize(const void *options, size_t structSize, size_t expected) noexcept {
    return options != nullptr && structSize >= expected;
}

} // namespace

extern "C" {

const char *cppk2_version(void) {
    return cpp_kana::v2::version();
}

const char *cppk2_code_text(int32_t code) {
    if (code < CPPK2_OK || code > CPPK2_ERR_INTERNAL) {
        return "unknown error";
    }
    return cpp_kana::v2::codeText(static_cast<Code>(code));
}

int32_t cppk2_romaji_options_init(cppk2_romaji_options *options, size_t struct_size) {
    if (!ValidOptionsSize(options, struct_size, sizeof(cppk2_romaji_options))) {
        return CPPK2_ERR_INVALID_ARGUMENT;
    }
    const RomajiOptions defaults;
    std::memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(cppk2_romaji_options);
    options->sokuon = static_cast<int32_t>(defaults.sokuon);
    options->hatsuon = static_cast<int32_t>(defaults.hatsuon);
    options->long_vowel = static_cast<int32_t>(defaults.longVowel);
    options->letter_case = static_cast<int32_t>(defaults.letterCase);
    options->separator = static_cast<int32_t>(defaults.separator);
    options->unknown = static_cast<int32_t>(defaults.unknown);
    return CPPK2_OK;
}

int32_t cppk2_kana_options_init(cppk2_kana_options *options, size_t struct_size) {
    if (!ValidOptionsSize(options, struct_size, sizeof(cppk2_kana_options))) {
        return CPPK2_ERR_INVALID_ARGUMENT;
    }
    const KanaOptions defaults;
    std::memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(cppk2_kana_options);
    options->script = static_cast<int32_t>(defaults.script);
    options->unknown = static_cast<int32_t>(defaults.unknown);
    return CPPK2_OK;
}

int32_t cppk2_kana_to_romaji(const char *kana, const cppk2_romaji_options *options, cppk2_conversion **out,
                             cppk2_error *err) {
    // The out parameter is cleared BEFORE any check: a caller that frees *out after a failed call must not
    // be freeing whatever happened to be on its stack. (It is only cleared when the pointer itself is valid.)
    if (out != nullptr) {
        *out = nullptr;
    }
    if (kana == nullptr) {
        return Fail(err, Code::InvalidArgument, "kana must not be NULL");
    }
    if (out == nullptr) {
        return Fail(err, Code::InvalidArgument, "out must not be NULL");
    }
    try {
        RomajiOptions resolved;
        if (options != nullptr) {
            if (options->struct_size < sizeof(cppk2_romaji_options)) {
                return Fail(err, Code::InvalidArgument, "options.struct_size is smaller than this header's sizeof");
            }
            resolved.sokuon = static_cast<cpp_kana::v2::SokuonPolicy>(options->sokuon);
            resolved.hatsuon = static_cast<cpp_kana::v2::HatsuonPolicy>(options->hatsuon);
            resolved.longVowel = static_cast<cpp_kana::v2::LongVowelPolicy>(options->long_vowel);
            resolved.letterCase = static_cast<cpp_kana::v2::CasePolicy>(options->letter_case);
            resolved.separator = static_cast<cpp_kana::v2::SeparatorPolicy>(options->separator);
            resolved.unknown = static_cast<UnknownPolicy>(options->unknown);
        }
        return Deliver(cpp_kana::v2::kanaToRomaji(kana, resolved), out, err);
    } catch (const std::bad_alloc &) {
        return Fail(err, Code::Internal, "out of memory");
    } catch (...) {
        return Fail(err, Code::Internal, "unexpected exception at the C boundary");
    }
}

int32_t cppk2_romaji_to_kana(const char *romaji, const cppk2_kana_options *options, cppk2_conversion **out,
                             cppk2_error *err) {
    // The out parameter is cleared BEFORE any check: a caller that frees *out after a failed call must not
    // be freeing whatever happened to be on its stack. (It is only cleared when the pointer itself is valid.)
    if (out != nullptr) {
        *out = nullptr;
    }
    if (romaji == nullptr) {
        return Fail(err, Code::InvalidArgument, "romaji must not be NULL");
    }
    if (out == nullptr) {
        return Fail(err, Code::InvalidArgument, "out must not be NULL");
    }
    try {
        KanaOptions resolved;
        if (options != nullptr) {
            if (options->struct_size < sizeof(cppk2_kana_options)) {
                return Fail(err, Code::InvalidArgument, "options.struct_size is smaller than this header's sizeof");
            }
            resolved.script = static_cast<Script>(options->script);
            resolved.unknown = static_cast<UnknownPolicy>(options->unknown);
        }
        return Deliver(cpp_kana::v2::romajiToKana(romaji, resolved), out, err);
    } catch (const std::bad_alloc &) {
        return Fail(err, Code::Internal, "out of memory");
    } catch (...) {
        return Fail(err, Code::Internal, "unexpected exception at the C boundary");
    }
}

int32_t cppk2_convert_script(const char *text, int32_t script, int32_t unknown, cppk2_conversion **out,
                             cppk2_error *err) {
    // The out parameter is cleared BEFORE any check: a caller that frees *out after a failed call must not
    // be freeing whatever happened to be on its stack. (It is only cleared when the pointer itself is valid.)
    if (out != nullptr) {
        *out = nullptr;
    }
    if (text == nullptr) {
        return Fail(err, Code::InvalidArgument, "text must not be NULL");
    }
    if (out == nullptr) {
        return Fail(err, Code::InvalidArgument, "out must not be NULL");
    }
    try {
        return Deliver(cpp_kana::v2::convertScript(text, static_cast<Script>(script), static_cast<UnknownPolicy>(unknown)),
                       out, err);
    } catch (const std::bad_alloc &) {
        return Fail(err, Code::Internal, "out of memory");
    } catch (...) {
        return Fail(err, Code::Internal, "unexpected exception at the C boundary");
    }
}

void cppk2_string_free(char *text) {
    std::free(text);
}

void cppk2_conversion_free(cppk2_conversion *conversion) {
    if (conversion == nullptr) {
        return;
    }
    if (conversion->moras != nullptr) {
        for (int32_t i = 0; i < conversion->mora_count; ++i) {
            std::free(const_cast<char *>(conversion->moras[i].text));
            std::free(const_cast<char *>(conversion->moras[i].rendered));
        }
        std::free(conversion->moras);
    }
    std::free(conversion->text);
    std::free(conversion->rendered);
    std::free(conversion);
}

} // extern "C"
