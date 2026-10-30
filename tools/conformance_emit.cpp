// conformance_emit -- writes conformance/cases.json and tests/data/corpus/expected.tsv from the corpus.
//
// Why the fixture is generated from the library rather than written by hand: the fixture's job is to be
// a LOCK shared by the C++, Python and C# suites (the same input, the same expected bytes), not an
// oracle about what is correct. Writing it from the engine and then pinning its sha256 is what makes
// "the three shells agree" checkable -- a shell that drifts shows up as a mismatch against a file it did
// not edit.
//
// `--check` re-derives everything in memory and compares with what is on disk, so a change to the engine
// that nobody re-pinned is red instead of quietly rewriting the lock.
//
//     cpp-kana2-conformance --corpus <cases.tsv> --dir <repository root> [--check]

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cpp-kana2/core/Convert.h>
#include <cpp-kana2/core/Types.h>

using namespace cpp_kana::v2;

namespace {

struct Case {
    std::string kind; // "kana" or "romaji"
    std::string input;
};

struct Result {
    std::string kind;
    std::string input;
    std::string options;
    std::string rendered;
    int moraCount = 0;
    bool ok = false;
    /// The call's status code (0 = succeeded). Separate from `ok`, which is the conversion's own verdict:
    /// a call can succeed while a mora was kept as an unknown character.
    int status = 0;
};

std::vector<Case> ReadCorpus(const std::string &path) {
    std::vector<Case> cases;
    std::ifstream input(path, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        cases.push_back(Case{line.substr(0, tab), line.substr(tab + 1)});
    }
    return cases;
}

std::string JsonEscape(const std::string &text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (const unsigned char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04X", c);
                out += buffer;
            } else {
                out.push_back(static_cast<char>(c)); // UTF-8 passes through unchanged
            }
            break;
        }
    }
    return out;
}

void Emit(const char *optionsName, const Outcome<Conversion> &outcome, const Case &source,
          std::vector<Result> *results) {
    Result result;
    result.kind = source.kind;
    result.input = source.input;
    result.options = optionsName;
    if (outcome.ok()) {
        result.rendered = outcome.value().rendered();
        result.moraCount = static_cast<int>(outcome.value().moras().size());
        // `ok` is the CONVERSION's own verdict (every mora converted), not the call's: a call can succeed
        // while one mora was kept as an unknown character, and a shell that reported true there would be
        // claiming the input converted.
        result.ok = outcome.value().ok();
        result.status = 0;
    } else {
        result.rendered = std::string("<") + codeText(outcome.error().code) + ">";
        result.moraCount = 0;
        result.ok = false;
        result.status = static_cast<int>(outcome.error().code);
    }
    results->push_back(std::move(result));
}

std::vector<Result> Derive(const std::vector<Case> &cases) {
    std::vector<Result> results;

    RomajiOptions dropSokuon;
    dropSokuon.sokuon = SokuonPolicy::Drop;
    RomajiOptions plainHatsuon;
    plainHatsuon.hatsuon = HatsuonPolicy::Plain;
    RomajiOptions longVowelDrop;
    longVowelDrop.longVowel = LongVowelPolicy::Drop;
    RomajiOptions longVowelDash;
    longVowelDash.longVowel = LongVowelPolicy::KatakanaDash;
    RomajiOptions uppercase;
    uppercase.letterCase = CasePolicy::Upper;
    RomajiOptions compact;
    compact.separator = SeparatorPolicy::None;
    // The failing half of the fixture: with UnknownPolicy::Fail an input that contains a character the table
    // does not know must come back as a status, not as a rendered string. Without this variant every one of the
    // 544 cases succeeded, so the error path of all three language suites was dead code.
    RomajiOptions unknownFail;
    unknownFail.unknown = UnknownPolicy::Fail;
    KanaOptions katakana;
    katakana.script = Script::Katakana;

    for (const Case &source : cases) {
        if (source.kind == "kana") {
            Emit("default", kanaToRomaji(source.input), source, &results);
            Emit("compact", kanaToRomaji(source.input, compact), source, &results);
            Emit("drop_sokuon", kanaToRomaji(source.input, dropSokuon), source, &results);
            Emit("plain_hatsuon", kanaToRomaji(source.input, plainHatsuon), source, &results);
            Emit("drop_long_vowel", kanaToRomaji(source.input, longVowelDrop), source, &results);
            Emit("dash_long_vowel", kanaToRomaji(source.input, longVowelDash), source, &results);
            Emit("uppercase", kanaToRomaji(source.input, uppercase), source, &results);
            Emit("unknown_fail", kanaToRomaji(source.input, unknownFail), source, &results);
            Emit("katakana_script", convertScript(source.input, Script::Katakana), source, &results);
        } else if (source.kind == "romaji") {
            Emit("default", romajiToKana(source.input), source, &results);
            Emit("katakana_output", romajiToKana(source.input, katakana), source, &results);
        }
    }
    return results;
}

std::string RenderJson(const std::vector<Result> &results, const std::string &commit) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema\": 1,\n";
    json << "  \"cases_count\": " << results.size() << ",\n";
    json << "  \"generated_by_commit\": \"" << commit << "\",\n";
    json << "  \"engine_version\": \"" << version() << "\",\n";
    json << "  \"note\": \"Generated by tools/conformance_emit.cpp. A lock, not an oracle: the C++, "
            "Python and C# suites all read these bytes.\",\n";
    json << "  \"cases\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const Result &result = results[i];
        json << "    {\"kind\": \"" << JsonEscape(result.kind) << "\", \"input\": \"" << JsonEscape(result.input)
             << "\", \"options\": \"" << JsonEscape(result.options) << "\", \"rendered\": \""
             << JsonEscape(result.rendered) << "\", \"mora_count\": " << result.moraCount
             << ", \"ok\": " << (result.ok ? "true" : "false") << ", \"status\": " << result.status << "}";
        json << (i + 1 == results.size() ? "\n" : ",\n");
    }
    json << "  ]\n}\n";
    return json.str();
}

std::string RenderTsv(const std::vector<Result> &results) {
    std::ostringstream tsv;
    tsv << "# Expected outputs for tests/data/corpus/cases.tsv -- generated by tools/conformance_emit.cpp.\n";
    tsv << "# Columns: kind <TAB> options <TAB> input <TAB> rendered <TAB> mora_count <TAB> ok <TAB> status\n";
    tsv << "# Regenerate with: cpp-kana2-conformance --corpus tests/data/corpus/cases.tsv --dir . --write\n";
    for (const Result &result : results) {
        tsv << result.kind << '\t' << result.options << '\t' << result.input << '\t' << result.rendered << '\t'
            << result.moraCount << '\t' << (result.ok ? "1" : "0") << '\t' << result.status << '\n';
    }
    return tsv.str();
}

bool WriteOrCompare(const std::string &path, const std::string &content, bool write) {
    if (write) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            // A write that fails silently is how a fixture "updates" while staying stale: the missing
            // conformance/ directory once made --write exit 0 without creating the file.
            std::printf("ERROR: cannot write %s (does the directory exist?)\n", path.c_str());
            return false;
        }
        out << content;
        out.flush();
        if (!out) {
            std::printf("ERROR: write to %s failed part-way\n", path.c_str());
            return false;
        }
        std::printf("wrote %s\n", path.c_str());
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("DRIFT: %s is missing\n", path.c_str());
        return false;
    }
    std::ostringstream existing;
    existing << in.rdbuf();
    if (existing.str() != content) {
        std::printf("DRIFT: %s is out of date\n", path.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    std::string corpus;
    std::string root = ".";
    std::string commitArgument;
    bool write = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--corpus" && i + 1 < argc) {
            corpus = argv[++i];
        } else if (argument == "--dir" && i + 1 < argc) {
            root = argv[++i];
        } else if (argument == "--commit" && i + 1 < argc) {
            commitArgument = argv[++i];
        } else if (argument == "--write") {
            write = true;
        } else if (argument == "--check") {
            write = false;
        } else {
            std::printf("unknown argument: %s\n", argument.c_str());
            return 2;
        }
    }
    if (corpus.empty()) {
        std::printf("usage: cpp-kana2-conformance --corpus <cases.tsv> --dir <root> [--write|--check]\n");
        return 2;
    }

    const std::vector<Case> cases = ReadCorpus(corpus);
    if (cases.empty()) {
        std::printf("ERROR: the corpus at %s loaded 0 cases; refusing to pin an empty fixture\n", corpus.c_str());
        return 1;
    }
    const std::vector<Result> results = Derive(cases);

    // The commit is recorded only when the caller names one (--commit). Baking it in at configure time
    // made every commit invalidate the fixture, which teaches people to regenerate blindly -- the
    // opposite of what a lock is for. The sha256 pin in cases.sha256 is the real anchor; the field is
    // provenance for a human reading the file.
    const std::string commit = commitArgument.empty() ? "unknown" : commitArgument;

    bool ok = true;
    ok &= WriteOrCompare(root + "/conformance/cases.json", RenderJson(results, commit), write);
    ok &= WriteOrCompare(root + "/tests/data/corpus/expected.tsv", RenderTsv(results), write);
    if (!ok) {
        std::printf("regenerate with: cpp-kana2-conformance --corpus tests/data/corpus/cases.tsv --dir . --write\n");
        return 1;
    }
    std::printf("conformance fixture: %zu cases, consistent\n", results.size());
    return 0;
}
