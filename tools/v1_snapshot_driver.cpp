// v1 snapshot driver -- the tool docs/V1_DIFFERENCES.md is written from. It is tracked, but it is NOT part of
// the build: it compiles against a v1 checkout (git worktree add .tmp/v1-src main) and its only job is to
// reproduce the v1 numbers in that document.
//
// Reads tests/data/corpus/cases.tsv, runs every case through the v1 public API and prints one TSV
// line per (direction, case, operation). The output is the baseline that docs/V1_DIFFERENCES.md is
// written from; it is not a promise about v2 behaviour.
//
// Build (MinGW, from the repository root):
//     g++ -std=c++17 -O1 -I.tmp/v1-src/include tools/v1_snapshot_driver.cpp .tmp/v1-src/src/*.cpp \
//         -o .tmp/v1_snapshot_driver.exe

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <cpp-kana/Kana.h>

namespace {

std::string Escaped(const std::string &text) {
    std::string out;
    for (const unsigned char c : text) {
        if (c == '\t') {
            out += "\\t";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0); // v1 can fault; unbuffered output names the case
    const std::string path = (argc > 1) ? argv[1] : "tests/data/corpus/cases.tsv";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cannot open " << path << "\n";
        return 2;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        const std::string direction = line.substr(0, tab);
        const std::string text = line.substr(tab + 1);

        if (direction == "kana") {
            const auto plain = Kana::kanaToRomaji(text, Kana::Error::Default, false).toStdStr();
            const auto sokuon = Kana::kanaToRomaji(text, Kana::Error::Default, true).toStdStr();
            const auto kata = Kana::convertKana(text, Kana::KanaType::Katakana);
            std::cout << "kana\t" << Escaped(text) << "\tkanaToRomaji\t" << Escaped(plain) << "\n";
            std::cout << "kana\t" << Escaped(text) << "\tkanaToRomaji_sokuon\t" << Escaped(sokuon) << "\n";
            std::cout << "kana\t" << Escaped(text) << "\tconvertKana_katakana\t" << Escaped(kata) << "\n";
        } else if (direction == "romaji") {
            const auto hira = Kana::romajiToKana(text, Kana::Error::Default, Kana::KanaType::Hiragana).toStdStr();
            const auto kata = Kana::romajiToKana(text, Kana::Error::Default, Kana::KanaType::Katakana).toStdStr();
            std::cout << "romaji\t" << Escaped(text) << "\tromajiToKana_hiragana\t" << Escaped(hira) << "\n";
            std::cout << "romaji\t" << Escaped(text) << "\tromajiToKana_katakana\t" << Escaped(kata) << "\n";
        }
    }
    return 0;
}
