#include <iostream>

#include <cpp-kana/kana.h>

int main(int argc, char *argv[]) {
    const std::string raw1 = "これでもやれるだけ飞ばしてきたんだよ";
    const auto res1 = Kana::kanaToRomaji(raw1, Kana::Error::Default, true).toStdStr();
    std::cout << "test1: Kana->romaji (Default)" << std::endl << raw1 << " -> " << res1 << std::endl;

    const auto res2 = Kana::romajiToKana(res1, Kana::Error::Default, Kana::KanaType::Hiragana).toStdStr();
    std::cout << "test2: romaji->Hiragana" << std::endl << res1 << " -> " << res2 << std::endl;

    const auto res3 = Kana::convertKana(res2, Kana::KanaType::Katakana);
    std::cout << "test3: Hiragana->Katakana" << std::endl << res2 << " -> " << res3 << std::endl;

    const std::string raw2 = "こん好にちは";
    const auto res4 = Kana::kanaToRomaji(raw2, Kana::Error::Ignore, true).toStdStr();
    std::cout << "test4: Kana->romaji (Ignore)" << std::endl << raw2 << " -> " << res4 << std::endl;

    return 0;
}
