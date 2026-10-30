#include "Internal.h"

#include <cstddef>

#include "../data/Tables.h"
#include "../infra/Unicode.h"

namespace cpp_kana::v2::internal
{

    std::vector<std::u32string> SegmentMorae(std::u32string_view normalized) {
        std::vector<std::u32string> morae;
        morae.reserve(normalized.size());

        const std::size_t maxLookahead = data::MaxKanaLength();
        std::size_t i = 0;
        while (i < normalized.size()) {
            // Longest match against the table, bounded by the table's own longest key. A base kana plus one
            // small kana (きゃ) is the only shape that needs the extra code point today, but the bound comes
            // from the data rather than from a constant, so a longer entry (ゔぁ) stays segmentable.
            std::size_t consumed = 1;
            for (std::size_t length = maxLookahead; length >= 2; --length) {
                if (i + length > normalized.size()) {
                    continue;
                }
                const std::u32string candidate(normalized.substr(i, length));
                if (data::FindMora(infra::ToUtf8(candidate)) != nullptr) {
                    consumed = length;
                    break;
                }
            }
            morae.emplace_back(normalized.substr(i, consumed));
            i += consumed;
        }
        return morae;
    }

} // namespace cpp_kana::v2::internal
