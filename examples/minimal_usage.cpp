#include <cstdio>

#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Types.h>

// The shortest useful path through the library: three calls, no configuration, no allocation to free.
//
// Everything falls out of the error model: a failure is a value, so there is no try/catch here, and the
// only way to get bytes out is to ask a successful Outcome for them.

int main() {
    using namespace cpp_kana::v2;

    const auto romaji = kanaToRomaji("こんにちは、きょうはいいてんきですね。");
    if (!romaji.ok()) {
        std::printf("kanaToRomaji failed: %s\n", codeText(romaji.error().code));
        return 1;
    }
    std::printf("kana   -> %s\n", romaji.value().rendered().c_str());

    // Per-mora detail is always available; it is how a caller tells an ordinary syllable from a sokuon,
    // a long vowel, or a character the table does not know.
    for (const Mora &mora : romaji.value().moras()) {
        std::printf("  %-4s -> %-4s (kind %d)\n", mora.text.c_str(), mora.rendered.c_str(),
                    static_cast<int>(mora.kind));
    }

    const auto kana = romajiToKana("konnichiha, kyouha iitenki desune.");
    if (!kana.ok()) {
        std::printf("romajiToKana failed: %s\n", codeText(kana.error().code));
        return 1;
    }
    std::printf("romaji -> %s\n", kana.value().rendered().c_str());

    const auto katakana = convertScript("こんにちは", Script::Katakana);
    std::printf("script -> %s\n", katakana.ok() ? katakana.value().rendered().c_str() : "(failed)");
    return 0;
}
