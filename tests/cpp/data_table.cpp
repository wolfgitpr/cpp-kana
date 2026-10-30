#include "harness.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cpp-kana2/core/Convert.h>

using namespace cpp_kana::v2;

namespace
{

    struct Row {
        std::string kana;
        std::string romaji;
        std::vector<std::string> aliases;
        int kind = 0;
        bool secondary = false;
    };

    /// Reads data/moras.tsv -- the same file tools/gen_moras.py writes and src/data/TableData.inc is
    /// generated from. Testing against the DATA rather than against a copy of it is the point: a table
    /// edit that breaks the round trip fails here without anyone remembering to update a fixture.
    std::vector<Row> ReadTable() {
        std::vector<Row> rows;
        const std::string path = std::string(CPP_KANA2_SOURCE_DIR) + "/data/moras.tsv";
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            harness::ReportFailure(__FILE__, __LINE__, "cannot open " + path);
            return rows;
        }
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty() || line.front() == '#') {
                continue;
            }
            std::vector<std::string> fields;
            std::string field;
            std::istringstream stream(line);
            while (std::getline(stream, field, '\t')) {
                fields.push_back(field);
            }
            if (fields.size() != 5) {
                harness::ReportFailure(__FILE__, __LINE__, "malformed row: " + line);
                continue;
            }
            Row row;
            row.kana = fields[0];
            row.romaji = fields[1];
            row.kind = std::stoi(fields[3]);
            row.secondary = fields[4] == "1";
            std::istringstream aliases(fields[2]);
            std::string alias;
            while (std::getline(aliases, alias, ',')) {
                if (!alias.empty()) {
                    row.aliases.push_back(alias);
                }
            }
            rows.push_back(row);
        }
        // A fixture that loads nothing must abort every assertion that reads it, not pass vacuously.
        CHECK(rows.size() > 100);
        return rows;
    }

    std::string Compact(const Outcome<Conversion> &outcome) {
        return outcome.ok() ? outcome.value().rendered("") : std::string("<") + codeText(outcome.error().code) + ">";
    }

} // namespace

TEST_CASE("every table row renders to its own romaji") {
    for (const Row &row : ReadTable()) {
        CHECK_EQ(Compact(kanaToRomaji(row.kana)), row.romaji);
    }
}

TEST_CASE("every primary table row survives a round trip") {
    for (const Row &row : ReadTable()) {
        if (row.secondary) {
            continue; // ぢ/づ/うぉ share their spelling with a primary kana by design
        }
        CHECK_EQ(Compact(romajiToKana(row.romaji)), row.kana);
    }
}

TEST_CASE("every accepted alias reaches its own kana") {
    for (const Row &row : ReadTable()) {
        for (const std::string &alias : row.aliases) {
            CHECK_EQ(Compact(romajiToKana(alias)), row.kana);
        }
    }
}

TEST_CASE("the table has no duplicate kana and no duplicate primary romaji") {
    std::vector<std::string> kana;
    std::vector<std::string> romaji;
    for (const Row &row : ReadTable()) {
        for (const std::string &seen : kana) {
            if (seen == row.kana) {
                harness::ReportFailure(__FILE__, __LINE__, "duplicate kana " + row.kana);
            }
        }
        if (!row.secondary) {
            for (const std::string &seen : romaji) {
                if (seen == row.romaji) {
                    harness::ReportFailure(__FILE__, __LINE__, "duplicate primary romaji " + row.romaji);
                }
            }
            romaji.push_back(row.romaji);
        }
        kana.push_back(row.kana);
    }
}

TEST_CASE("katakana input renders the same as hiragana input") {
    for (const Row &row : ReadTable()) {
        const auto asKatakana = convertScript(row.kana, Script::Katakana);
        CHECK(asKatakana.ok());
        if (asKatakana.ok()) {
            CHECK_EQ(Compact(kanaToRomaji(asKatakana.value().rendered(""))), row.romaji);
        }
    }
}
