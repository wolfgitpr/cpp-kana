#include "harness.h"

#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Types.h>
#include <cpp-kana2/core/Utf8.h>
#include <cpp-kana2/core/Version.h>

using namespace cpp_kana::v2;

namespace
{

    std::string Rendered(const Outcome<Conversion> &outcome) {
        return outcome.ok() ? outcome.value().rendered() : std::string("<") + codeText(outcome.error().code) + ">";
    }

    /// Compact rendering: the morae with no separator. The default separator is a space (the shape v1
    /// printed), so the interesting assertions ask for "" explicitly rather than depending on a policy.
    std::string Compact(const Outcome<Conversion> &outcome) {
        return outcome.ok() ? outcome.value().rendered("") : std::string("<") + codeText(outcome.error().code) + ">";
    }

    std::string Kana(const std::string &romaji) { return Compact(romajiToKana(romaji)); }

    std::string Romaji(const std::string &kana) { return Compact(kanaToRomaji(kana)); }

} // namespace

// --- version chain --------------------------------------------------------------------------------

TEST_CASE("version() agrees with the compile-time macros") {
    CHECK_EQ(std::string(version()), std::string(CPP_KANA2_VERSION_STRING));
    CHECK(CPP_KANA2_VERSION_AT_LEAST(2, 0, 0));
    CHECK(!CPP_KANA2_VERSION_AT_LEAST(2, 1, 0));
}

// --- UTF-8 boundary -------------------------------------------------------------------------------

TEST_CASE("UTF-8 round-trips, including non-BMP") {
    const std::string text = "あア漢\U0001F600"; // a 4-byte UTF-8 sequence after two kana and a kanji
    const auto decoded = utf8ToCodepoints(text);
    CHECK(decoded.ok());
    if (decoded.ok()) {
        CHECK_EQ(decoded.value().size(), static_cast<std::size_t>(4));
        const auto encoded = codepointsToUtf8(decoded.value());
        CHECK(encoded.ok());
        if (encoded.ok()) {
            CHECK_EQ(encoded.value(), text);
        }
    }
}

TEST_CASE("invalid UTF-8 is a value, not an exception") {
    // Truncated 3-byte sequence: v1 threw std::invalid_argument out of its decoder.
    const auto truncated = utf8ToCodepoints("\xE3\x81");
    CHECK(!truncated.ok());
    CHECK_EQ(truncated.error().code, Code::InvalidUtf8);

    // Overlong encoding of '/' (0xC0 0xAF).
    const auto overlong = utf8ToCodepoints("\xC0\xAF");
    CHECK(!overlong.ok());
    CHECK_EQ(overlong.error().code, Code::InvalidUtf8);

    // A surrogate encoded as UTF-8.
    const auto surrogate = utf8ToCodepoints("\xED\xA0\x80");
    CHECK(!surrogate.ok());
    CHECK_EQ(surrogate.error().code, Code::InvalidUtf8);

    CHECK(!isValidUtf8("\xFF"));
    CHECK(isValidUtf8("かな"));
}

TEST_CASE("empty input is a legal empty result") {
    const auto converted = kanaToRomaji("");
    CHECK(converted.ok());
    if (converted.ok()) {
        CHECK(converted.value().moras().empty());
        CHECK_EQ(converted.value().rendered(), std::string(""));
    }
}

// --- script conversion (does not need the mora table) ---------------------------------------------

TEST_CASE("convertScript folds half-width katakana, which used to crash") {
    // v1 faulted with 0xC0000005 here: no kana tokens, then `res.size() - 1` wrapped.
    CHECK_EQ(Rendered(convertScript("ｶﾀｶﾅ", Script::Hiragana)), std::string("かたかな"));
    CHECK_EQ(Rendered(convertScript("かたかな", Script::Katakana)), std::string("カタカナ"));
    CHECK_EQ(Rendered(convertScript("ｶﾞｯｺｳ", Script::Hiragana)), std::string("がっこう"));
}

TEST_CASE("convertScript leaves non-kana alone") {
    CHECK_EQ(Rendered(convertScript("abc漢", Script::Katakana)), std::string("abc漢"));
}

// --- kana -> romaji, Hepburn ----------------------------------------------------------------------

TEST_CASE("kana to romaji: the cases v1 got wrong") {
    CHECK_EQ(Romaji("こんにちは"), std::string("konnichiha"));
    CHECK_EQ(Romaji("がっこう"), std::string("gakkou"));   // v1: "ga cl ko u"
    CHECK_EQ(Romaji("きって"), std::string("kitte"));      // v1: "ki cl te"
    CHECK_EQ(Romaji("まっちゃ"), std::string("matcha"));   // v1: "ma cl cha"
    CHECK_EQ(Romaji("しんよう"), std::string("shin'you"));
    CHECK_EQ(Romaji("コーヒー"), std::string("koohii"));   // v1 dropped both long vowel marks
    CHECK_EQ(Romaji("とうきょう"), std::string("toukyou"));
    CHECK_EQ(Romaji("ｶﾀｶﾅ"), std::string("katakana"));     // v1 faulted with 0xC0000005
    CHECK_EQ(Romaji("か゛く"), std::string("gaku"));         // v1: "ka ゛ ku"
    CHECK_EQ(Romaji("ゐ"), std::string("i"));              // archaic, marked MoraKind::Archaic
}

TEST_CASE("kana to romaji: sokuon and hatsuon policies") {
    RomajiOptions drop;
    drop.sokuon = SokuonPolicy::Drop;
    CHECK_EQ(Compact(kanaToRomaji("がっこう", drop)), std::string("gakou"));

    RomajiOptions plain;
    plain.hatsuon = HatsuonPolicy::Plain;
    CHECK_EQ(Compact(kanaToRomaji("しんよう", plain)), std::string("shinyou"));

    RomajiOptions dashed;
    dashed.longVowel = LongVowelPolicy::KatakanaDash;
    CHECK_EQ(Compact(kanaToRomaji("コーヒー", dashed)), std::string("ko-hi-"));

    RomajiOptions dropped;
    dropped.longVowel = LongVowelPolicy::Drop;
    CHECK_EQ(Compact(kanaToRomaji("コーヒー", dropped)), std::string("kohi"));
}

TEST_CASE("kana to romaji: case and separator policies") {
    // Compact is the default; the per-mora space-separated view is still one option away.
    CHECK_EQ(Romaji("こんにちは"), std::string("konnichiha"));
    RomajiOptions spaced;
    spaced.separator = SeparatorPolicy::Space;
    CHECK_EQ(Rendered(kanaToRomaji("こんにちは", spaced)), std::string("ko n ni chi ha"));

    RomajiOptions upper;
    upper.letterCase = CasePolicy::Upper;
    CHECK_EQ(Compact(kanaToRomaji("こんにちは", upper)), std::string("KONNICHIHA"));

    RomajiOptions capitalise;
    capitalise.letterCase = CasePolicy::Capitalize;
    CHECK_EQ(Compact(kanaToRomaji("こんにちは", capitalise)), std::string("Konnichiha"));
}

TEST_CASE("kana to romaji: unknown characters are marked, not guessed") {
    const auto converted = kanaToRomaji("こん好にちは");
    CHECK(converted.ok());
    if (converted.ok()) {
        CHECK_EQ(converted.value().rendered(""), std::string("kon好nichiha"));
        CHECK(!converted.value().ok()); // a kept kanji is a non-ok mora, even though the call succeeded
    }

    RomajiOptions ignore;
    ignore.unknown = UnknownPolicy::Ignore;
    CHECK_EQ(Compact(kanaToRomaji("こん好にちは", ignore)), std::string("konnichiha"));

    RomajiOptions fail;
    fail.unknown = UnknownPolicy::Fail;
    const auto failed = kanaToRomaji("こん好にちは", fail);
    CHECK(!failed.ok());
    CHECK_EQ(failed.error().code, Code::UnsupportedCharacter);
}

// --- romaji -> kana -------------------------------------------------------------------------------

TEST_CASE("romaji to kana: syllables, sokuon, hatsuon") {
    CHECK_EQ(Kana("konnichiha"), std::string("こんにちは"));
    CHECK_EQ(Kana("kitte"), std::string("きって"));
    CHECK_EQ(Kana("matcha"), std::string("まっちゃ"));
    CHECK_EQ(Kana("gakkou"), std::string("がっこう"));
    CHECK_EQ(Kana("shin'you"), std::string("しんよう"));
    CHECK_EQ(Kana("toukyou"), std::string("とうきょう"));
    CHECK_EQ(Kana("nn"), std::string("ん"));
    CHECK_EQ(Kana("n"), std::string("ん"));
}

TEST_CASE("romaji to kana: canonical spellings beat aliases") {
    // v1 returned ぢ for "ji" because its initialiser kept the first duplicate key of the table.
    CHECK_EQ(Kana("ji"), std::string("じ"));
    CHECK_EQ(Kana("zu"), std::string("ず"));
    CHECK_EQ(Kana("wo"), std::string("を"));
    // Hepburn's letter spellings win where they collide with a Kunrei alias, so the round trip from a
    // rendered てぃ comes back as てぃ rather than as ち.
    CHECK_EQ(Kana("ti"), std::string("てぃ"));
    CHECK_EQ(Kana("tu"), std::string("とぅ"));
    CHECK_EQ(Kana("di"), std::string("でぃ"));
    CHECK_EQ(Kana("du"), std::string("どぅ"));
    CHECK_EQ(Kana("chi"), std::string("ち"));
    CHECK_EQ(Kana("tsu"), std::string("つ"));
    // ... and the aliases no kana claims canonically stay reachable.
    CHECK_EQ(Kana("dji"), std::string("ぢ"));
    CHECK_EQ(Kana("dzu"), std::string("づ"));
    CHECK_EQ(Kana("sya"), std::string("しゃ"));
    CHECK_EQ(Kana("zi"), std::string("じ"));
    CHECK_EQ(Kana("si"), std::string("し"));
    CHECK_EQ(Kana("hu"), std::string("ふ"));
}

TEST_CASE("romaji to kana: katakana output") {
    KanaOptions options;
    options.script = Script::Katakana;
    CHECK_EQ(Compact(romajiToKana("katakana", options)), std::string("カタカナ"));
}

// --- error model ----------------------------------------------------------------------------------

TEST_CASE("asking for the wrong side of an Outcome throws") {
    const auto failed = utf8ToCodepoints("\xE3\x81");
    bool threw = false;
    try {
        (void)failed.value();
    }
    catch (const ErrorException &error) {
        threw = true;
        CHECK_EQ(error.error().code, Code::InvalidUtf8);
    }
    CHECK(threw);
}

TEST_CASE("codeText is total") {
    CHECK_EQ(std::string(codeText(Code::Ok)), std::string("ok"));
    CHECK_EQ(std::string(codeText(Code::InvalidUtf8)), std::string("invalid UTF-8"));
    CHECK_EQ(std::string(codeText(static_cast<Code>(99))), std::string("unknown error"));
}

int main() {
    for (const auto &testCase : harness::Cases()) {
        harness::CurrentCase() = testCase.name;
        testCase.body();
    }
    if (harness::Failures() != 0) {
        std::printf("%d failure(s)\n", harness::Failures());
        return 1;
    }
    std::printf("all %zu test cases passed\n", harness::Cases().size());
    return 0;
}
