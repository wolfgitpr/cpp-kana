# Conformance fixture

`cases.json` is the **lock** the C++, Python and C# suites all read: the same input, the same expected
bytes. It is not an oracle about what is correct — `docs/V1_DIFFERENCES.md` and the hand-written test
cases are where the *decisions* live. This file exists so that the three language shells cannot drift
apart silently, and so that a change in the engine is visible as a diff here.

## Who writes it

`tools/conformance_emit.cpp`, built as `cpp-kana2-conformance` when `CPP_KANA2_BUILD_TESTS=ON`. It reads
`tests/data/corpus/cases.tsv`, runs each case through the public API under a fixed set of option
variants, and writes:

- `conformance/cases.json` — for the Python and C# suites;
- `tests/data/corpus/expected.tsv` — for the C++ suite (same data, TSV, no JSON parser needed in C++).

```bash
cmake --preset default && cmake --build --preset default
.tmp/build-default/bin/cpp-kana2-conformance \
    --corpus tests/data/corpus/cases.tsv --dir . --write     # re-pin after an intended change
.tmp/build-default/bin/cpp-kana2-conformance \
    --corpus tests/data/corpus/cases.tsv --dir . --check     # what ctest runs
```

`--check` re-derives the fixture and compares it with the tracked bytes; a drift is exit 1 and names the
file. That is registered as the `ConformanceFixture` ctest case, so an engine change that nobody re-pinned
fails the suite instead of quietly rewriting the lock.

## Shape

```json
{
  "schema": 1,
  "cases_count": 604,
  "generated_by_commit": "<sha of the tree that produced it, or "unknown">",
  "engine_version": "2.0.0",
  "note": "...",
  "cases": [
    {"kind": "kana|romaji", "input": "...", "options": "default|compact|...",
     "rendered": "...", "mora_count": 4, "ok": true, "status": 0}
  ]
}
```

`mora_count` is part of the fixture on purpose: a rendering that bytes-match but segments differently is
a different answer for a caller that walks the mora list, and only a count catches it cheaply.

The per-case sha256 of this file (`cases.sha256`) is verified by `tools/check_conformance.py`; the fixture
itself is pinned by `.gitattributes` (`text eol=lf`) so a CRLF checkout cannot change its bytes.
