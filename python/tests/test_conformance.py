"""The same fixture the C++ suite reads: conformance/cases.json.

This is the point of the fixture. The C++ suite pins it through `expected.tsv`, this suite replays the JSON
through the C ABI, and the C# suite does the same through P/Invoke -- three different call paths, one set of
expected bytes. A shell that drifts shows up here as a mismatch against a file it did not edit.
"""

from __future__ import annotations

from cpp_kana2 import (
    CppKanaError,
    KanaOptions,
    RomajiOptions,
    convert_script,
    kana_to_romaji,
    romaji_to_kana,
)

# Option ids written by tools/conformance_emit.cpp. An id in the fixture with no entry here fails
# test_every_fixture_option_is_mapped instead of being skipped.
KANA_OPTION_KWARGS = {
    "default": {},
    "compact": {"separator": "none"},
    "drop_sokuon": {"sokuon": "drop"},
    "plain_hatsuon": {"hatsuon": "plain"},
    "drop_long_vowel": {"long_vowel": "drop"},
    "dash_long_vowel": {"long_vowel": "dash"},
    "uppercase": {"case": "upper"},
    # The failing half: with UnknownPolicy::Fail an input containing a character the table does not know must
    # come back as a status. Before this existed, all 544 cases succeeded and the error path of this suite was
    # never executed.
    "unknown_fail": {"unknown": "fail"},
}

ROMAJI_OPTION_KWARGS = {
    "default": {},
    "katakana_output": {"script": "katakana"},
}


def run_case(case: dict):
    if case["kind"] == "kana":
        if case["options"] == "katakana_script":
            return convert_script(case["input"], "katakana")
        return kana_to_romaji(case["input"], RomajiOptions(**KANA_OPTION_KWARGS[case["options"]]))
    return romaji_to_kana(case["input"], KanaOptions(**ROMAJI_OPTION_KWARGS[case["options"]]))


def test_every_fixture_option_is_mapped(conformance_fixture: dict) -> None:
    missing = [
        f"{case['kind']}/{case['options']}"
        for case in conformance_fixture["cases"]
        if (case["kind"] == "kana" and case["options"] != "katakana_script" and case["options"] not in KANA_OPTION_KWARGS)
        or (case["kind"] == "romaji" and case["options"] not in ROMAJI_OPTION_KWARGS)
    ]
    assert not missing, f"fixture options with no mapping in this shell: {sorted(set(missing))}"


def test_conformance_cases(conformance_fixture: dict) -> None:
    mismatches = []
    for case in conformance_fixture["cases"]:
        label = f"{case['kind']}/{case['options']} {case['input']!r}"
        expected_status = case.get("status", 0)
        if expected_status != 0:
            # A case that must FAIL: the shell has to raise the exception that carries this code. Raising
            # something else -- or returning a conversion -- is a mismatch, which is the point of keeping
            # failing cases in the fixture at all.
            try:
                conversion = run_case(case)
            except CppKanaError as error:
                if error.code != expected_status:
                    mismatches.append(f"{label}: raised code {error.code}, fixture says {expected_status}")
            else:
                mismatches.append(f"{label}: fixture says status {expected_status}, shell returned "
                                  f"{conversion.rendered!r}")
            continue
        try:
            conversion = run_case(case)
        except Exception as error:  # noqa: BLE001 - reported as a mismatch, not swallowed
            mismatches.append(f"{label}: raised {error!r} but the fixture expects success")
            continue
        if conversion.rendered != case["rendered"]:
            mismatches.append(f"{label}: rendered {conversion.rendered!r} != fixture {case['rendered']!r}")
        elif len(conversion.moras) != case["mora_count"]:
            mismatches.append(f"{label}: {len(conversion.moras)} moras != fixture {case['mora_count']}")
        elif conversion.ok != case["ok"]:
            mismatches.append(f"{label}: ok={conversion.ok} != fixture {case['ok']}")
    assert not mismatches, f"{len(mismatches)} of {len(conformance_fixture['cases'])} cases disagree:\n" + "\n".join(
        mismatches[:20]
    )
