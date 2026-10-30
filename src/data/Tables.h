#ifndef CPP_KANA2_DATA_TABLES_H
#define CPP_KANA2_DATA_TABLES_H

#include <cstddef>
#include <string>
#include <string_view>

namespace cpp_kana::v2::data
{

    // The mora table, compiled in. Its single source of truth is data/moras.tsv: tools/gen_moras.py writes
    // src/data/TableData.inc from it, and tools/check_data_tables.py refuses a tree where the two disagree.
    // Reading the table through this interface (rather than through a map literal inside a conversion
    // function) is what makes the "no duplicate keys" and "covers the inventory" checks possible at all.

    struct MoraEntry {
        /// The kana spelling (hiragana form; katakana is normalised before lookup).
        std::string_view kana;
        /// The Hepburn romanisation.
        std::string_view romaji;
        /// Extra accepted spellings for the reverse direction, comma-separated, empty when there are none.
        std::string_view aliases;
        /// One of MoraKind's integer values, as written in the table.
        int kind;
        /// 1 when this kana shares its romanisation with another one (ぢ with じ, うぉ with を). A reverse
        /// lookup prefers the primary, which is what makes "ji" mean じ instead of ぢ.
        int secondary;
    };

    std::size_t MoraCount() noexcept;
    const MoraEntry *MoraAt(std::size_t index) noexcept;

    /// The entry for a kana spelling, or nullptr when the table does not know it. `kana` must already be
    /// normalised to hiragana.
    const MoraEntry *FindMora(std::string_view kana) noexcept;

    /// The entry whose canonical romanisation or one of whose aliases is exactly `romaji`, or nullptr.
    const MoraEntry *FindRomaji(std::string_view romaji) noexcept;

    /// The longest kana key in the table, in code points (segmenter lookahead bound).
    std::size_t MaxKanaLength() noexcept;

    /// The longest romanisation or alias in the table, in bytes (parser lookahead bound).
    std::size_t MaxRomajiLength() noexcept;

} // namespace cpp_kana::v2::data

#endif // CPP_KANA2_DATA_TABLES_H
