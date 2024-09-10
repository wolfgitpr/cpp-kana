# cpp-kana

## Intro

cpp-kana is a lightweight library for converting Japanese kana to romaji and vice versa.

It is written in C++ and uses a simple algorithm to perform the conversion.

## Usage

```c++
const std::string raw1 = "これでもやれるだけ飞ばしてきたんだよ";
const RomajiResVector res1 = Kana::kanaToRomaji(raw1, Kana::Error::Default, true);
std::cout << "test1: Kana->romaji (Default)" << std::endl << raw1 << " -> " << res1.toStdStr() << std::endl;

const KanaResVector res2 = Kana::romajiToKana(res1, Kana::Error::Default, Kana::KanaType::Hiragana);
std::cout << "test2: romaji->Hiragana" << std::endl << res1.toStdStr() << " -> " << res2.toStdStr() << std::endl;

const std::string res3 = Kana::convertKana(res2.toStdStr(), Kana::KanaType::Katakana);
std::cout << "test3: Hiragana->Katakana" << std::endl << res2 << " -> " << res3 << std::endl;
```

## Doc

```c++
//  include/KanaRes.h
struct RomajiRes {
    std::string kana;
    std::string romaji;
    bool error = true; //  Whether the conversion failed.
};

class RomajiResVector : public std::vector<RomajiRes> {
public:
    // Convert KanaResVector to std::vector<std::string>
    inline std::vector<std::string> toStdVector();

    // Convert KanaResVector to std::string with delimiter
    inline std::string toStdStr(const std::string &delimiter = " ");
};

//  include/Kana.h
enum class Error {
  // Keep original characters
  Default = 0,
  // Ignore this character (do not export)
  Ignore = 1
};

/*
    @param kanaStr : raw utf-8 std::string.
    @param errorType : Ignore words that have failed conversion. Default: Keep original.
    @param doubleWrittenSokuon : Use double-written sokuon. Default: false.
    @return RomajiResVector.
*/
RomajiResVector kanaToRomaji(const std::string &kanaStr, Error error = Error::Default, const bool &doubleWrittenSokuon = false);

/*
    @param hans : raw utf-8 std::string vector, each element of the vector is a word.
    ...
    @return RomajiResVector.
*/
RomajiResVector kanaToRomaji(const std::vector<std::string> &kanaList, const bool &doubleWrittenSokuon = false);

/*
    @param romajiStr : raw utf-8 std::string.
    @param errorType : Ignore words that have failed conversion. Default: Keep original.
    @param kanaType : Convert to Hiragana or Katakana. Default: Hiragana.
    @return KanaResVector.
*/
KanaResVector romajiToKana(const std::string &romajiStr, Error error = Error::Default,
                                 KanaType kanaType = KanaType::Hiragana);

/*
    @param romajiList : raw utf-8 std::string vector, each element of the vector is a word.
    ...
    @return KanaResVector.
*/
KanaResVector romajiToKana(const std::vector<std::string> &romajiList, Error error = Error::Default,
                             KanaType kanaType = KanaType::Hiragana);
                                 
/*
    @param kanaStr : raw utf-8 std::string.
    @param kanaType : Convert to Hiragana or Katakana. Default: Hiragana.
    @return std::string.
*/
std::string convertKana(const std::string &kanaStr, KanaType kanaType = KanaType::Hiragana);
```