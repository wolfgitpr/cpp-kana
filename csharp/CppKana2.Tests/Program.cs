using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using CppKana2;

namespace CppKana2.Tests
{
    /// <summary>
    /// The C# suite. No test framework: a list of named checks, a failure counter, and a non-zero exit
    /// code. The conformance half replays the same conformance/cases.json the C++ and Python suites use.
    /// </summary>
    internal static class Program
    {
        private static int _failures;
        private static string _current = "";

        private static void Check(string name, Action body)
        {
            _current = name;
            try
            {
                body();
            }
            catch (Exception error)
            {
                Fail($"threw {error.GetType().Name}: {error.Message}");
            }
        }

        private static void Fail(string message)
        {
            _failures++;
            Console.WriteLine($"FAIL {_current}: {message}");
        }

        private static void True(bool condition, string what)
        {
            if (!condition)
            {
                Fail($"expected {what}");
            }
        }

        private static void Equal<T>(T actual, T expected, string what)
        {
            if (!EqualityComparer<T>.Default.Equals(actual, expected))
            {
                Fail($"{what}: got {actual}, want {expected}");
            }
        }

        private static int Main()
        {
            Check("version chain", () =>
            {
                var version = Kana.Version;
                True(version.StartsWith("2."), "a 2.x version from the native library");
                // The managed assembly carries the same version: both come from the root CMakeLists.txt
                // (the native one through CPP_KANA2_VERSION_STRING, the managed one through
                // csharp/tools/emit_build_info.py).
                var assemblyVersion = typeof(Kana).Assembly.GetName().Version?.ToString(3);
                True(assemblyVersion == "2.0.0", $"assembly version 2.0.0, got {assemblyVersion}");
            });

            Check("code text", () =>
            {
                Equal(Kana.CodeText(Status.Ok), "ok", "code 0");
                Equal(Kana.CodeText(Status.InvalidUtf8), "invalid UTF-8", "code 2");
                Equal(Kana.CodeText(9999), "unknown error", "unknown code");
            });

            Check("kana to romaji", () =>
            {
                var conversion = Kana.KanaToRomaji("がっこう");
                Equal(conversion.Rendered, "gakkou", "rendered");
                True(conversion.Ok, "conversion ok");
                Equal(conversion.Moras.Count, 4, "mora count");
                Equal(conversion.Moras[1].Kind, MoraKind.Sokuon, "sokuon kind");
                Equal(conversion.Moras[1].Rendered, "k", "sokuon rendered");
            });

            Check("romaji to kana", () =>
            {
                Equal(Kana.RomajiToKana("konnichiha").Rendered, "こんにちは", "rendered");
                Equal(Kana.RomajiToKana("kitte", new KanaOptions { Script = Script.Katakana }).Rendered, "キッテ",
                      "katakana output");
            });

            Check("script conversion", () =>
            {
                Equal(Kana.ConvertScript("ｶﾞｯｺｳ").Rendered, "がっこう", "half-width to hiragana");
            });

            Check("unknown characters are marked", () =>
            {
                var kept = Kana.KanaToRomaji("こん好にちは");
                Equal(kept.Rendered, "kon好nichiha", "kept");
                True(!kept.Ok, "conversion not ok");
                True(kept.Moras.Any(mora => mora.Code == Status.UnsupportedCharacter), "a marked mora");
                Equal(Kana.KanaToRomaji("こん好にちは", new RomajiOptions { Unknown = UnknownPolicy.Ignore }).Rendered,
                      "konnichiha", "ignored");
                True(Throws<UnsupportedCharacterError>(
                         () => Kana.KanaToRomaji("こん好にちは", new RomajiOptions { Unknown = UnknownPolicy.Fail })),
                     "fail policy raises");
            });

            Check("input validation", () =>
            {
                True(Throws<InvalidArgumentError>(() => Kana.KanaToRomaji("あ\0い")), "embedded NUL is refused");
                True(Throws<InvalidArgumentError>(() => Kana.KanaToRomaji(null!)), "null is refused");
            });

            Check("empty input", () =>
            {
                var conversion = Kana.KanaToRomaji("");
                Equal(conversion.Rendered, "", "rendered");
                Equal(conversion.Moras.Count, 0, "mora count");
            });

            Check("conformance fixture", RunConformance);

            if (_failures != 0)
            {
                Console.WriteLine($"{_failures} failure(s)");
                return 1;
            }
            Console.WriteLine("all C# checks passed");
            return 0;
        }

        private static bool Throws<T>(Action action) where T : Exception
        {
            try
            {
                action();
                return false;
            }
            catch (T)
            {
                return true;
            }
        }

        private static void RunConformance()
        {
            var root = RepositoryRoot();
            var path = Path.Combine(root, "conformance", "cases.json");
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var cases = document.RootElement.GetProperty("cases");
            var mismatches = new List<string>();

            foreach (var entry in cases.EnumerateArray())
            {
                var kind = entry.GetProperty("kind").GetString() ?? "";
                var input = entry.GetProperty("input").GetString() ?? "";
                var options = entry.GetProperty("options").GetString() ?? "";
                var rendered = entry.GetProperty("rendered").GetString() ?? "";
                var moraCount = entry.GetProperty("mora_count").GetInt32();
                var ok = entry.GetProperty("ok").GetBoolean();
                var status = entry.GetProperty("status").GetInt32();

                Conversion conversion;
                try
                {
                    conversion = Run(kind, options, input);
                }
                catch (CppKana2Exception error)
                {
                    if (status == 0)
                    {
                        mismatches.Add($"{kind}/{options} {input}: threw {error.Message} but the fixture expects status 0");
                    }
                    else if (error.Code != status)
                    {
                        // The code matters, not just "it threw": a case that must fail with
                        // UnsupportedCharacter passing because it failed with InvalidArgument is not a pass.
                        mismatches.Add($"{kind}/{options} {input}: threw code {error.Code}, fixture says {status}");
                    }
                    continue;
                }
                if (status != 0)
                {
                    mismatches.Add($"{kind}/{options} {input}: the fixture expects status {status} but the call succeeded");
                    continue;
                }
                if (conversion.Rendered != rendered)
                {
                    mismatches.Add($"{kind}/{options} {input}: rendered {conversion.Rendered} != {rendered}");
                }
                else if (conversion.Moras.Count != moraCount)
                {
                    mismatches.Add($"{kind}/{options} {input}: {conversion.Moras.Count} moras != {moraCount}");
                }
                else if (conversion.Ok != ok)
                {
                    mismatches.Add($"{kind}/{options} {input}: ok={conversion.Ok} != {ok}");
                }
            }

            if (mismatches.Count != 0)
            {
                Fail($"{mismatches.Count} of {cases.GetArrayLength()} cases disagree:\n  "
                     + string.Join("\n  ", mismatches.Take(20)));
            }
        }

        private static Conversion Run(string kind, string options, string input)
        {
            if (kind == "romaji")
            {
                return options == "katakana_output"
                    ? Kana.RomajiToKana(input, new KanaOptions { Script = Script.Katakana })
                    : Kana.RomajiToKana(input);
            }
            if (options == "katakana_script")
            {
                return Kana.ConvertScript(input, Script.Katakana);
            }
            var romaji = new RomajiOptions();
            switch (options)
            {
                case "compact":
                    romaji.Separator = SeparatorPolicy.None;
                    break;
                case "drop_sokuon":
                    romaji.Sokuon = SokuonPolicy.Drop;
                    break;
                case "plain_hatsuon":
                    romaji.Hatsuon = HatsuonPolicy.Plain;
                    break;
                case "drop_long_vowel":
                    romaji.LongVowel = LongVowelPolicy.Drop;
                    break;
                case "dash_long_vowel":
                    romaji.LongVowel = LongVowelPolicy.KatakanaDash;
                    break;
                case "uppercase":
                    romaji.LetterCase = CasePolicy.Upper;
                    break;
                case "unknown_fail":
                    // The failing half of the fixture: this option makes a call with an unknown character
                    // return a status instead of a rendered string.
                    romaji.Unknown = UnknownPolicy.Fail;
                    break;
            }
            return Kana.KanaToRomaji(input, romaji);
        }

        /// <summary>Walks up from the test assembly until the repository root (CMakeLists.txt) is found.</summary>
        private static string RepositoryRoot()
        {
            var directory = new DirectoryInfo(AppContext.BaseDirectory);
            while (directory is not null && !File.Exists(Path.Combine(directory.FullName, "CMakeLists.txt")))
            {
                directory = directory.Parent;
            }
            if (directory is null)
            {
                throw new CppKana2Exception("could not find the repository root (no CMakeLists.txt above the test assembly)");
            }
            return directory.FullName;
        }
    }
}
