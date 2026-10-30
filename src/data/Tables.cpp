#include "Tables.h"

namespace cpp_kana::v2::data
{
    namespace
    {

#include "TableData.inc"

        constexpr std::size_t kCount = sizeof(kEntries) / sizeof(kEntries[0]);

        bool MatchesAlias(std::string_view aliases, std::string_view romaji) noexcept
        {
            while (!aliases.empty())
            {
                const std::size_t comma = aliases.find(',');
                if (aliases.substr(0, comma) == romaji)
                {
                    return true;
                }
                if (comma == std::string_view::npos)
                {
                    return false;
                }
                aliases.remove_prefix(comma + 1);
            }
            return false;
        }

    } // namespace

    std::size_t MoraCount() noexcept
    {
        return kCount;
    }

    const MoraEntry *MoraAt(std::size_t index) noexcept
    {
        return index < kCount ? &kEntries[index] : nullptr;
    }

    const MoraEntry *FindMora(std::string_view kana) noexcept
    {
        for (const MoraEntry &entry : kEntries)
        {
            if (entry.kana == kana)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    const MoraEntry *FindRomaji(std::string_view romaji) noexcept
    {
        // Three passes, in this order, and the order is the contract:
        //   1. a primary entry's canonical spelling  -- "ji" is じ, never ぢ;
        //   2. a secondary entry's canonical spelling -- reachable only when no primary claims it;
        //   3. an alias                               -- "zi" is じ, "di" is ぢ.
        // v1 answered from a single unordered_map whose initialiser silently kept the first duplicate key,
        // so "ji" returned ぢ. Fixed passes make the answer a property of the table instead of a property
        // of the order somebody happened to write the literal in.
        for (const MoraEntry &entry : kEntries)
        {
            if (entry.secondary == 0 && entry.romaji == romaji)
            {
                return &entry;
            }
        }
        for (const MoraEntry &entry : kEntries)
        {
            if (entry.secondary != 0 && entry.romaji == romaji)
            {
                return &entry;
            }
        }
        for (const MoraEntry &entry : kEntries)
        {
            if (MatchesAlias(entry.aliases, romaji))
            {
                return &entry;
            }
        }
        return nullptr;
    }

    std::size_t MaxKanaLength() noexcept
    {
        std::size_t longest = 1;
        for (const MoraEntry &entry : kEntries)
        {
            std::size_t codePoints = 0;
            for (const char c : entry.kana)
            {
                if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U)
                {
                    ++codePoints;
                }
            }
            if (codePoints > longest)
            {
                longest = codePoints;
            }
        }
        return longest;
    }

    std::size_t MaxRomajiLength() noexcept
    {
        std::size_t longest = 1;
        for (const MoraEntry &entry : kEntries)
        {
            if (entry.romaji.size() > longest)
            {
                longest = entry.romaji.size();
            }
            std::string_view remaining = entry.aliases;
            while (!remaining.empty())
            {
                const std::size_t comma = remaining.find(',');
                const std::string_view alias = remaining.substr(0, comma);
                if (alias.size() > longest)
                {
                    longest = alias.size();
                }
                if (comma == std::string_view::npos)
                {
                    break;
                }
                remaining.remove_prefix(comma + 1);
            }
        }
        return longest;
    }

} // namespace cpp_kana::v2::data
