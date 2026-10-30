#include "Unicode.h"

namespace cpp_kana::v2::infra
{
    namespace
    {

        constexpr char32_t kHiraganaStart = 0x3041;
        constexpr char32_t kHiraganaEnd = 0x309F;
        constexpr char32_t kKatakanaStart = 0x30A1;
        constexpr char32_t kKatakanaEnd = 0x30FF;
        constexpr char32_t kHalfwidthStart = 0xFF61;
        constexpr char32_t kHalfwidthEnd = 0xFF9F;
        // The offset between the two kana blocks is not uniform across the ranges (ヷヸヹヺ sit at 0x30F7 and
        // have no 0x30xx partner), so the mapping is a table, not arithmetic.
        struct Pair {
            char32_t hiragana;
            char32_t katakana;
        };

        constexpr Pair kKanaPairs[] = {
            {0x3041, 0x30A1}, {0x3042, 0x30A2}, {0x3043, 0x30A3}, {0x3044, 0x30A4}, {0x3045, 0x30A5}, {0x3046, 0x30A6},
            {0x3047, 0x30A7}, {0x3048, 0x30A8}, {0x3049, 0x30A9}, {0x304A, 0x30AA}, {0x304B, 0x30AB}, {0x304C, 0x30AC},
            {0x304D, 0x30AD}, {0x304E, 0x30AE}, {0x304F, 0x30AF}, {0x3050, 0x30B0}, {0x3051, 0x30B1}, {0x3052, 0x30B2},
            {0x3053, 0x30B3}, {0x3054, 0x30B4}, {0x3055, 0x30B5}, {0x3056, 0x30B6}, {0x3057, 0x30B7}, {0x3058, 0x30B8},
            {0x3059, 0x30B9}, {0x305A, 0x30BA}, {0x305B, 0x30BB}, {0x305C, 0x30BC}, {0x305D, 0x30BD}, {0x305E, 0x30BE},
            {0x305F, 0x30BF}, {0x3060, 0x30C0}, {0x3061, 0x30C1}, {0x3062, 0x30C2}, {0x3063, 0x30C3}, {0x3064, 0x30C4},
            {0x3065, 0x30C5}, {0x3066, 0x30C6}, {0x3067, 0x30C7}, {0x3068, 0x30C8}, {0x3069, 0x30C9}, {0x306A, 0x30CA},
            {0x306B, 0x30CB}, {0x306C, 0x30CC}, {0x306D, 0x30CD}, {0x306E, 0x30CE}, {0x306F, 0x30CF}, {0x3070, 0x30D0},
            {0x3071, 0x30D1}, {0x3072, 0x30D2}, {0x3073, 0x30D3}, {0x3074, 0x30D4}, {0x3075, 0x30D5}, {0x3076, 0x30D6},
            {0x3077, 0x30D7}, {0x3078, 0x30D8}, {0x3079, 0x30D9}, {0x307A, 0x30DA}, {0x307B, 0x30DB}, {0x307C, 0x30DC},
            {0x307D, 0x30DD}, {0x307E, 0x30DE}, {0x307F, 0x30DF}, {0x3080, 0x30E0}, {0x3081, 0x30E1}, {0x3082, 0x30E2},
            {0x3083, 0x30E3}, {0x3084, 0x30E4}, {0x3085, 0x30E5}, {0x3086, 0x30E6}, {0x3087, 0x30E7}, {0x3088, 0x30E8},
            {0x3089, 0x30E9}, {0x308A, 0x30EA}, {0x308B, 0x30EB}, {0x308C, 0x30EC}, {0x308D, 0x30ED}, {0x308E, 0x30EE},
            {0x308F, 0x30EF}, {0x3090, 0x30F0}, {0x3091, 0x30F1}, {0x3092, 0x30F2}, {0x3093, 0x30F3}, {0x3094, 0x30F4},
            {0x3095, 0x30F5}, {0x3096, 0x30F6},
            // No hiragana counterparts in the two blocks; the katakana side is listed so the iteration marks can
            // be recognised as kana. ヷ(30F7) ヸ(30F8) ヹ(30F9) ヺ(30FA) ヽ(30FD) ヾ(30FE)
        };

        constexpr char32_t ToHiragana(char32_t codePoint) noexcept {
            for (const Pair &pair : kKanaPairs) {
                if (pair.katakana == codePoint) {
                    return pair.hiragana;
                }
            }
            switch (codePoint) {
            case 0x30F7: // ヷ -- no hiragana counterpart; the va/vi/ve/vo row is katakana-only
            case 0x30F8: // ヸ
            case 0x30F9: // ヹ
            case 0x30FA: // ヺ
                return codePoint;
            case 0x30FC: // ー has no hiragana form and must stay a long vowel mark
                return 0x30FC;
            default:
                return codePoint;
            }
        }

        constexpr char32_t ToKatakana(char32_t codePoint) noexcept {
            for (const Pair &pair : kKanaPairs) {
                if (pair.hiragana == codePoint) {
                    return pair.katakana;
                }
            }
            if (codePoint == 0x3094) {
                return 0x30F4;
            }
            return codePoint;
        }

        // Half-width katakana. The block is a compact 63-character table with gaps for the voicing marks; the
        // mapping is written out because the offsets are not uniform (ｦ is 0xFF66, ｱ starts at 0xFF71).
        constexpr char32_t kHalfwidthTable[] = {
            0x3002, // ｡
            0x300C, // ｢
            0x300D, // ｣
            0x3001, // ､
            0x30FB, // ･
            0x30F2, // ｦ
            0x30A1, // ｧ
            0x30A3, // ｨ
            0x30A5, // ｩ
            0x30A7, // ｪ
            0x30A9, // ｫ
            0x30E3, // ｬ
            0x30E5, // ｭ
            0x30E7, // ｮ
            0x30C3, // ｯ
            0x30FC, // ｰ
            0x30A2, // ｱ
            0x30A4, // ｲ
            0x30A6, // ｳ
            0x30A8, // ｴ
            0x30AA, // ｵ
            0x30AB, // ｶ
            0x30AD, // ｷ
            0x30AF, // ｸ
            0x30B1, // ｹ
            0x30B3, // ｺ
            0x30B5, // ｻ
            0x30B7, // ｼ
            0x30B9, // ｽ
            0x30BB, // ｾ
            0x30BD, // ｿ
            0x30BF, // ﾀ
            0x30C1, // ﾁ
            0x30C4, // ﾂ
            0x30C6, // ﾃ
            0x30C8, // ﾄ
            0x30CA, // ﾅ
            0x30CB, // ﾆ
            0x30CC, // ﾇ
            0x30CD, // ﾈ
            0x30CE, // ﾉ
            0x30CF, // ﾊ
            0x30D2, // ﾋ
            0x30D5, // ﾌ
            0x30D8, // ﾍ
            0x30DB, // ﾎ
            0x30DE, // ﾏ
            0x30DF, // ﾐ
            0x30E0, // ﾑ
            0x30E1, // ﾒ
            0x30E2, // ﾓ
            0x30E4, // ﾔ
            0x30E6, // ﾕ
            0x30E8, // ﾖ
            0x30E9, // ﾗ
            0x30EA, // ﾘ
            0x30EB, // ﾙ
            0x30EC, // ﾚ
            0x30ED, // ﾛ
            0x30EF, // ﾜ
            0x30F3, // ﾝ
            // 0xFF9E (ﾞ) and 0xFF9F (ﾟ) are voicing marks, handled by the caller, so they are not in this table.
        };

    } // namespace

    CharClass Classify(char32_t codePoint) noexcept {
        if (codePoint >= kHiraganaStart && codePoint <= kHiraganaEnd) {
            if (codePoint == 0x3063) { // っ
                return CharClass::Sokuon;
            }
            if (codePoint == 0x3099 || codePoint == 0x309A || codePoint == 0x309B || codePoint == 0x309C) {
                return CharClass::VoicingMark;
            }
            return CharClass::Hiragana;
        }
        if (codePoint >= kKatakanaStart && codePoint <= kKatakanaEnd) {
            if (codePoint == 0x30C3) { // ッ
                return CharClass::Sokuon;
            }
            if (codePoint == 0x30FC) { // ー
                return CharClass::LongVowelMark;
            }
            return CharClass::Katakana;
        }
        if (codePoint >= kHalfwidthStart && codePoint <= kHalfwidthEnd) {
            if (codePoint == 0xFF9E || codePoint == 0xFF9F) {
                return CharClass::VoicingMark;
            }
            if (codePoint == 0xFF6F) { // ｯ
                return CharClass::Sokuon;
            }
            if (codePoint == 0xFF70) { // ｰ
                return CharClass::LongVowelMark;
            }
            return CharClass::HalfwidthKatakana;
        }
        if ((codePoint >= 'a' && codePoint <= 'z') || (codePoint >= 'A' && codePoint <= 'Z')) {
            return CharClass::LatinLetter;
        }
        return CharClass::Other;
    }

    
    char32_t FoldHalfwidth(char32_t codePoint) noexcept {
        if (codePoint < 0xFF61 || codePoint > 0xFF9D) {
            return codePoint;
        }
        const std::size_t index = static_cast<std::size_t>(codePoint - 0xFF61);
        if (index < sizeof(kHalfwidthTable) / sizeof(kHalfwidthTable[0])) {
            return kHalfwidthTable[index];
        }
        // 0xFF9E/0xFF9F are the voicing marks; the caller applies them.
        return codePoint;
    }

    char32_t KatakanaToHiragana(char32_t codePoint) noexcept { return ToHiragana(codePoint); }

    char32_t HiraganaToKatakana(char32_t codePoint) noexcept { return ToKatakana(codePoint); }

    char32_t ApplyVoicing(char32_t base,
                          bool semiVoiced) noexcept { // Voiced pairs are exactly the rows where the base is one of
                                                      // かさたは plus the う (ゔ) special case.
        if (semiVoiced) {
            switch (base) {
            case 0x306F:
                return 0x3071; // は -> ぱ
            case 0x3072:
                return 0x3074; // ひ -> ぴ
            case 0x3075:
                return 0x3077; // ふ -> ぷ
            case 0x3078:
                return 0x307A; // へ -> ぺ
            case 0x307B:
                return 0x307D; // ほ -> ぽ
            case 0x30CF:
                return 0x30D1; // ハ -> パ
            case 0x30D2:
                return 0x30D4; // ヒ -> ピ
            case 0x30D5:
                return 0x30D7; // フ -> プ
            case 0x30D8:
                return 0x30DA; // ヘ -> ペ
            case 0x30DB:
                return 0x30DD; // ホ -> ポ
            default:
                return 0;
            }
        }
        switch (base) {
        case 0x3046:
            return 0x3094; // う -> ゔ
        case 0x304B:
            return 0x304C;
        case 0x304D:
            return 0x304E;
        case 0x304F:
            return 0x3050;
        case 0x3051:
            return 0x3052;
        case 0x3053:
            return 0x3054;
        case 0x3055:
            return 0x3056;
        case 0x3057:
            return 0x3058;
        case 0x3059:
            return 0x305A;
        case 0x305B:
            return 0x305C;
        case 0x305D:
            return 0x305E;
        case 0x305F:
            return 0x3060;
        case 0x3061:
            return 0x3062;
        case 0x3064:
            return 0x3065;
        case 0x3066:
            return 0x3067;
        case 0x3068:
            return 0x3069;
        case 0x306F:
            return 0x3070;
        case 0x3072:
            return 0x3073;
        case 0x3075:
            return 0x3076;
        case 0x3078:
            return 0x3079;
        case 0x307B:
            return 0x307C;
        case 0x30A6:
            return 0x30F4;
        case 0x30AB:
            return 0x30AC;
        case 0x30AD:
            return 0x30AE;
        case 0x30AF:
            return 0x30B0;
        case 0x30B1:
            return 0x30B2;
        case 0x30B3:
            return 0x30B4;
        case 0x30B5:
            return 0x30B6;
        case 0x30B7:
            return 0x30B8;
        case 0x30B9:
            return 0x30BA;
        case 0x30BB:
            return 0x30BC;
        case 0x30BD:
            return 0x30BE;
        case 0x30BF:
            return 0x30C0;
        case 0x30C1:
            return 0x30C2;
        case 0x30C4:
            return 0x30C5;
        case 0x30C6:
            return 0x30C7;
        case 0x30C8:
            return 0x30C9;
        case 0x30CF:
            return 0x30D0;
        case 0x30D2:
            return 0x30D3;
        case 0x30D5:
            return 0x30D6;
        case 0x30D8:
            return 0x30D9;
        case 0x30DB:
            return 0x30DC;
        case 0x30EF:
            return 0x30F7; // ワ -> ヷ
        case 0x30F0:
            return 0x30F8; // ヰ -> ヸ
        case 0x30F1:
            return 0x30F9; // ヱ -> ヹ
        case 0x30F2:
            return 0x30FA; // ヲ -> ヺ
        default:
            return 0;
        }
    }

    std::string ToUtf8(std::u32string_view codepoints) {
        std::string out;
        out.reserve(codepoints.size() * 3);
        for (const char32_t codePoint : codepoints) {
            if (codePoint < 0x80) {
                out.push_back(static_cast<char>(codePoint));
            } else if (codePoint < 0x800) {
                out.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            } else if (codePoint < 0x10000) {
                out.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            } else {
                out.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }
        return out;
    }

    std::u32string FromUtf8(std::string_view utf8) {
        std::u32string out;
        out.reserve(utf8.size());
        std::size_t i = 0;
        while (i < utf8.size()) {
            const auto lead = static_cast<unsigned char>(utf8[i]);
            char32_t codePoint = 0;
            std::size_t extra = 0;
            if (lead < 0x80) {
                codePoint = lead;
            } else if ((lead & 0xE0U) == 0xC0U) {
                codePoint = lead & 0x1FU;
                extra = 1;
            } else if ((lead & 0xF0U) == 0xE0U) {
                codePoint = lead & 0x0FU;
                extra = 2;
            } else {
                codePoint = lead & 0x07U;
                extra = 3;
            }
            for (std::size_t k = 1; k <= extra && i + k < utf8.size(); ++k) {
                codePoint = (codePoint << 6U) | (static_cast<unsigned char>(utf8[i + k]) & 0x3FU);
            }
            out.push_back(codePoint);
            i += extra + 1;
        }
        return out;
    }

} // namespace cpp_kana::v2::infra
