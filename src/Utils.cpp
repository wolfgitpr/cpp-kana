#include "Utils.h"

#include <iostream>
#include <unordered_set>


namespace Kana
{
    bool isLetter(char32_t c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool isHanzi(char32_t c) {
        return c >= 0x4e00 && c <= 0x9fa5;
    }

    bool isKana(char32_t c) {
        return (c >= 0x3040 && c <= 0x309F) || (c >= 0x30A0 && c <= 0x30FF);
    }

    bool isDigit(char32_t c) {
        return c >= '0' && c <= '9';
    }

    bool isSpace(char32_t c) {
        return c == ' ';
    }

    bool isSpecialKana(char32_t c) {
        static const std::unordered_set<char32_t> specialKana = {
            U'ャ', U'ュ', U'ョ', U'ゃ', U'ゅ', U'ょ',
            U'ァ', U'ィ', U'ゥ', U'ェ', U'ォ', U'ぁ', U'ぃ', U'ぅ', U'ぇ', U'ぉ'
        };
        return specialKana.find(c) != specialKana.end();
    }

    u32strVec splitString(const u32str &input) {
        u32strVec res;
        int pos = 0;
        while (pos < input.length()) {
            const auto currentChar = input[pos];
            if (isLetter(currentChar)) {
                const int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }
                res.push_back(input.substr(start, pos - start));
            } else if (isHanzi(currentChar) || isDigit(currentChar) || !isSpace(currentChar)) {
                res.push_back(input.substr(pos, 1));
                pos++;
            } else if (isKana(currentChar)) {
                const int length = (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                res.push_back(input.substr(pos, length));
                pos += length;
            } else {
                pos++;
            }
        }
        return res;
    }

}
