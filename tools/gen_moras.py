#!/usr/bin/env python3
"""Writes the mora table: data/moras.tsv (for review) and src/data/TableData.inc (for the library).

Why a generator instead of a hand-written table
-----------------------------------------------
v1's table was two hand-written `unordered_map` initialisers of 176 entries each, and it shipped four
duplicate keys (wa, o, ji, zu) that silently kept the first spelling: `romajiToKana("ji")` returned ぢ,
not じ. A table this regular is better DERIVED from rows than typed, and the derived artefact can be
checked mechanically (no duplicate kana, no duplicate canonical romaji, every value lower-case ASCII).

Two outputs, on purpose:
  * data/moras.tsv       -- tracked, reviewable, and what tools/check_data_tables.py re-derives from;
  * src/data/TableData.inc -- C++ initialiser, included by src/data/Tables.cpp.

`--check` re-generates in memory and compares against the tracked files; a drift is exit 1. That is how
"the generator and the committed table agree" becomes a gate instead of an intention.

Run:
    python tools/gen_moras.py            # rewrite both files
    python tools/gen_moras.py --check    # verify, change nothing
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TSV_PATH = ROOT / "data" / "moras.tsv"
INC_PATH = ROOT / "src" / "data" / "TableData.inc"

# MoraKind values, mirrored from include/cpp-kana2/core/Mora.h. Kept as plain integers here because the
# generator must not depend on the C++ headers being parseable.
KIND_SYLLABLE = 0
KIND_ARCHAIC = 5

# (kana, romaji) rows. Each row is written in gojuon order so the file reads like the table it is.
BASE_ROWS = [
    "あ い う え お|a i u e o",
    "か き く け こ|ka ki ku ke ko",
    "が ぎ ぐ げ ご|ga gi gu ge go",
    "さ し す せ そ|sa shi su se so",
    "ざ じ ず ぜ ぞ|za ji zu ze zo",
    "た ち つ て と|ta chi tsu te to",
    # ぢ/づ are homophones of じ/ず, and Hepburn romanises both pairs the same way. That is not a mistake
    # in the notation, it is what the notation does -- so the table says so instead of inventing dji/dzu:
    # the MERGE dict below marks ぢ as secondary and names じ as the kana a reverse lookup returns.
    "だ ぢ づ で ど|da ji zu de do",
    "な に ぬ ね の|na ni nu ne no",
    "は ひ ふ へ ほ|ha hi fu he ho",
    "ば び ぶ べ ぼ|ba bi bu be bo",
    "ぱ ぴ ぷ ぺ ぽ|pa pi pu pe po",
    "ま み む め も|ma mi mu me mo",
    "や ゆ よ|ya yu yo",
    "ら り る れ ろ|ra ri ru re ro",
    "わ|wa",
    # を is "wo" as a letter and "o" as the object particle. The canonical column takes "wo" so that
    # this table has one value per kana; "o" stays accepted input, and the canonical-first rule in
    # data::FindRomaji is what keeps "o" meaning お.
    "を|wo",
    # ゐ and ゑ are NOT in this table: they are folded to i/e by the renderer, which marks them
    # MoraKind::Archaic. Putting them here would give two kana the same canonical romaji (ゐ/i, ゑ/e),
    # and the reverse direction would have no way to tell which one was meant -- the same ambiguity
    # v1 shipped for じ/ぢ.
]

# Yōon: base kana + small ya/yu/yo.
YOON_ROWS = [
    "きゃ きゅ きょ|kya kyu kyo",
    "ぎゃ ぎゅ ぎょ|gya gyu gyo",
    "しゃ しゅ しょ|sha shu sho",
    "じゃ じゅ じょ|ja ju jo",
    "ちゃ ちゅ ちょ|cha chu cho",
    "ぢゃ ぢゅ ぢょ|ja ju jo",
    "にゃ にゅ にょ|nya nyu nyo",
    "ひゃ ひゅ ひょ|hya hyu hyo",
    "びゃ びゅ びょ|bya byu byo",
    "ぴゃ ぴゅ ぴょ|pya pyu pyo",
    "みゃ みゅ みょ|mya myu myo",
    "りゃ りゅ りょ|rya ryu ryo",
]

# Combinations that exist for foreign words (and for a few native ones). All canonical Hepburn.
FOREIGN_ROWS = [
    "ふぁ ふぃ ふぇ ふぉ|fa fi fe fo",
    "うぃ うぇ うぉ|wi we wo",
    "ゔぁ ゔぃ ゔ ゔぇ ゔぉ|va vi vu ve vo",
    "てぃ てゅ|ti tyu",
    "でぃ でゅ|di dyu",
    "とぅ どぅ|tu du",
    "つぁ つぃ つぇ つぉ|tsa tsi tse tso",
    "しぇ じぇ ちぇ|she je che",
    "いぇ|ye",
    "きぇ ぎぇ|kye gye",
    "にぇ ひぇ びぇ ぴぇ みぇ りぇ|nye hye bye pye mye rye",
    "くぁ くぃ くぇ くぉ|kwa kwi kwe kwo",
    "ぐぁ ぐぃ ぐぇ ぐぉ|gwa gwi gwe gwo",
    "すぁ すぃ すぇ すぉ|swa swi swe swo",
    "ずぁ ずぃ ずぇ ずぉ|zwa zwi zwe zwo",
    "ぬぁ ぬぃ ぬぇ ぬぉ|nwa nwi nwe nwo",
    "ぶぁ ぶぃ ぶぇ ぶぉ|bwa bwi bwe bwo",
    "ぷぁ ぷぃ ぷぇ ぷぉ|pwa pwi pwe pwo",
    "むぁ むぃ むぇ むぉ|mwa mwi mwe mwo",
    "るぁ るぃ るぇ るぉ|rwa rwi rwe rwo",
]

# Accepted INPUT spellings that are not the canonical output: Kunrei-shiki and the forms a Japanese
# IME produces. They only affect romajiToKana; kanaToRomaji always writes the canonical column.
#
# The canonical-first rule in data::FindRomaji is what makes ち win for "ti" (ち's alias) while てぃ keeps
# its canonical "ti": an alias never shadows another entry's canonical spelling.
ALIASES = {
    "し": "si",
    "ふ": "hu",
    "じ": "zi",
    "ぢ": "dji",
    "づ": "dzu",
    "しゃ": "sya",
    "しゅ": "syu",
    "しょ": "syo",
    "じゃ": "zya,jya",
    "じゅ": "zyu,jyu",
    "じょ": "zyo,jyo",
    "ぢゃ": "dja",
    "ぢゅ": "dju",
    "ぢょ": "djo",
    # ちゃ/ちゅ/ちょ have no Kunrei aliases: "tya"/"tyu"/"tyo" are the canonical spellings of てゃ/てゅ,
    # and an alias may not shadow another entry's canonical spelling.
}

ARCHAIC = {"ゐ", "ゑ"}

# Kana whose romanisation is shared with another kana, i.e. the reverse direction cannot tell them apart.
# Map: secondary kana -> the kana a reverse lookup returns for that spelling.
#
# This is the whole of the じ/ぢ and ず/づ story, stated as data instead of as an accident: v1 had these as
# duplicate map keys and the C++ initialiser silently kept the first one, so ``romajiToKana("ji")``
# returned ぢ. Here the merge is declared, checked (the spellings must be identical), and the reverse
# direction has a deterministic answer.
MERGE = {
    "ぢ": "じ",
    "づ": "ず",
    "ぢゃ": "じゃ",
    "ぢゅ": "じゅ",
    "ぢょ": "じょ",
    "うぉ": "を",
}


def build() -> list[tuple[str, str, str, int, int]]:
    entries: list[tuple[str, str, str, int, int]] = []
    seen_kana: dict[str, str] = {}
    canonical: dict[str, str] = {}

    for block in (BASE_ROWS, YOON_ROWS, FOREIGN_ROWS):
        for row in block:
            kana_part, romaji_part = row.split("|")
            kana_list = kana_part.split()
            romaji_list = romaji_part.split()
            if len(kana_list) != len(romaji_list):
                raise SystemExit(f"row length mismatch: {row}")
            for kana, romaji in zip(kana_list, romaji_list):
                if kana in seen_kana:
                    raise SystemExit(f"duplicate kana {kana!r} (also written for {seen_kana[kana]!r})")
                if romaji != romaji.lower() or not romaji.isascii() or not romaji.isalpha():
                    raise SystemExit(f"romaji {romaji!r} for {kana!r} must be lower-case ASCII letters")
                seen_kana[kana] = romaji
                if kana not in MERGE:
                    if romaji in canonical:
                        raise SystemExit(
                            f"duplicate canonical romaji {romaji!r} for {kana!r} and {canonical[romaji]!r}; "
                            f"declare one of them in MERGE")
                    canonical[romaji] = kana
                kind = KIND_ARCHAIC if kana in ARCHAIC else KIND_SYLLABLE
                entries.append((kana, romaji, ALIASES.get(kana, ""), kind, 1 if kana in MERGE else 0))

    # A merged kana must spell exactly what its primary spells, and both must be in the table.
    for secondary, primary in MERGE.items():
        if secondary not in seen_kana:
            raise SystemExit(f"MERGE names {secondary!r}, which is not in the table")
        if primary not in seen_kana:
            raise SystemExit(f"MERGE target {primary!r} is not in the table")
        if seen_kana[secondary] != seen_kana[primary]:
            raise SystemExit(
                f"MERGE {secondary!r} claims {seen_kana[secondary]!r} but {primary!r} is {seen_kana[primary]!r}")

    # An alias may not be any entry's canonical spelling.
    #
    # This is the rule that keeps the reverse direction unambiguous. Without it ち would want "ti" as an
    # alias while てぃ already owns "ti" as its canonical Hepburn spelling, and which one "ti" meant would
    # depend on lookup order -- the exact shape of v1's defect. The consequence is stated plainly in
    # docs/V1_DIFFERENCES.md: input is Hepburn, aliases only add spellings no kana claims canonically.
    seen_alias: dict[str, str] = {}
    all_canonical = {romaji for _kana, romaji, _aliases, _kind, _secondary in entries}
    for kana, romaji, aliases, _, _secondary in entries:
        for alias in filter(None, aliases.split(",")):
            if not (alias.isascii() and alias.isalpha() and alias == alias.lower()):
                raise SystemExit(f"alias {alias!r} of {kana!r} must be lower-case ASCII letters")
            if alias == romaji:
                raise SystemExit(f"alias {alias!r} of {kana!r} repeats its own canonical romaji")
            if alias in all_canonical:
                raise SystemExit(
                    f"alias {alias!r} of {kana!r} is the canonical romaji of another kana; "
                    f"the reverse direction would become order-dependent")
            if alias in seen_alias and seen_alias[alias] != kana:
                raise SystemExit(f"alias {alias!r} is claimed by both {kana!r} and {seen_alias[alias]!r}")
            seen_alias[alias] = kana
    return entries


def render_tsv(entries: list[tuple[str, str, str, int, int]]) -> str:
    lines = [
        "# The mora table -- generated by tools/gen_moras.py, do not edit by hand.",
        "#",
        "# Columns: kana <TAB> romaji <TAB> input aliases (comma separated) <TAB> MoraKind <TAB> secondary",
        "#",
        "# `romaji` is what kanaToRomaji writes (Hepburn). `input aliases` are the extra spellings",
        "# romajiToKana accepts (Kunrei-shiki and IME forms). `secondary` = 1 means this kana shares its",
        "# romaji with another one (ぢ with じ, うぉ with を): the reverse direction returns the primary,",
        "# which is declared in tools/gen_moras.py's MERGE dict and checked for identical spelling.",
        "#",
        "# tools/check_data_tables.py re-derives this file and refuses a tree where it has drifted.",
    ]
    for kana, romaji, aliases, kind, secondary in entries:
        lines.append(f"{kana}\t{romaji}\t{aliases}\t{kind}\t{secondary}")
    return "\n".join(lines) + "\n"


def c_escape(text: str) -> str:
    out = []
    for byte in text.encode("utf-8"):
        if byte == 0x5C:
            out.append("\\\\")
        elif byte == 0x22:
            out.append('\\"')
        elif 0x20 <= byte < 0x7F:
            out.append(chr(byte))
        else:
            out.append(f"\\x{byte:02X}")
    return "".join(out)


def render_inc(entries: list[tuple[str, str, str, int, int]]) -> str:
    lines = [
        "// GENERATED by tools/gen_moras.py from data/moras.tsv -- do not edit by hand.",
        "//",
        "// One row per mora. Kept as a plain array so Tables.cpp owns the lookup logic and the table owns",
        "// the data; tools/check_data_tables.py refuses a tree where this file and data/moras.tsv disagree.",
        "// Byte escapes are used because the table is UTF-8 and the build must not depend on the source",
        "// encoding of a non-ASCII literal (MSVC without /utf-8 would mangle it).",
        "",
        "constexpr MoraEntry kEntries[] = {",
    ]
    for kana, romaji, aliases, kind, secondary in entries:
        lines.append(f'    {{"{c_escape(kana)}", "{c_escape(romaji)}", "{c_escape(aliases)}", {kind}, {secondary}}},')
    lines.append("};")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify the tracked files, change nothing")
    args = parser.parse_args()

    entries = build()
    tsv = render_tsv(entries)
    inc = render_inc(entries)

    if args.check:
        problems = []
        if not TSV_PATH.exists() or TSV_PATH.read_text(encoding="utf-8") != tsv:
            problems.append(f"{TSV_PATH.relative_to(ROOT)} is out of date")
        if not INC_PATH.exists() or INC_PATH.read_text(encoding="utf-8") != inc:
            problems.append(f"{INC_PATH.relative_to(ROOT)} is out of date")
        if problems:
            for problem in problems:
                print(f"DRIFT: {problem}", file=sys.stderr)
            print("run: python tools/gen_moras.py", file=sys.stderr)
            return 1
        print(f"mora table: {len(entries)} entries, generated files up to date")
        return 0

    TSV_PATH.parent.mkdir(parents=True, exist_ok=True)
    INC_PATH.parent.mkdir(parents=True, exist_ok=True)
    # newline="\n" on purpose: .gitattributes marks data/** as -text, so the bytes committed are the
    # bytes written, and a CRLF checkout would otherwise change the table's hash.
    with TSV_PATH.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(tsv)
    with INC_PATH.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(inc)
    print(f"mora table: {len(entries)} entries written to {TSV_PATH.relative_to(ROOT)} and {INC_PATH.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
