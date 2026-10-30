using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;

namespace CppKana2
{
    /// <summary>Where the produced kana go.</summary>
    public enum Script
    {
        Hiragana = 0,
        Katakana = 1,
        AsIs = 2,
    }

    /// <summary>What a unit of the result was.</summary>
    public enum MoraKind
    {
        Syllable = 0,
        Sokuon = 1,
        Hatsuon = 2,
        LongVowel = 3,
        Other = 4,
        Archaic = 5,
        Romaji = 6,
    }

    public enum SokuonPolicy { Geminate = 0, Drop = 1 }

    public enum HatsuonPolicy { Apostrophe = 0, Plain = 1 }

    public enum LongVowelPolicy { Repeat = 0, Drop = 1, KatakanaDash = 2 }

    public enum CasePolicy { Lower = 0, Upper = 1, Capitalize = 2 }

    public enum SeparatorPolicy { Space = 0, None = 1 }

    public enum UnknownPolicy { Keep = 0, Ignore = 1, Fail = 2 }

    /// <summary>Mirrors <c>cppk2_romaji_options</c>; defaults are the library's own.</summary>
    public sealed class RomajiOptions
    {
        public SokuonPolicy Sokuon { get; set; } = SokuonPolicy.Geminate;
        public HatsuonPolicy Hatsuon { get; set; } = HatsuonPolicy.Apostrophe;
        public LongVowelPolicy LongVowel { get; set; } = LongVowelPolicy.Repeat;
        public CasePolicy LetterCase { get; set; } = CasePolicy.Lower;
        public SeparatorPolicy Separator { get; set; } = SeparatorPolicy.None;
        public UnknownPolicy Unknown { get; set; } = UnknownPolicy.Keep;

        internal NativeMethods.RomajiOptionsNative ToNative()
        {
            var native = new NativeMethods.RomajiOptionsNative();
            native.StructSize = (UIntPtr)Marshal.SizeOf<NativeMethods.RomajiOptionsNative>();
            var status = NativeMethods.cppk2_romaji_options_init(ref native, native.StructSize);
            if (status != Status.Ok)
            {
                throw new CppKana2Exception("cppk2_romaji_options_init failed", status);
            }
            native.Sokuon = (int)Sokuon;
            native.Hatsuon = (int)Hatsuon;
            native.LongVowel = (int)LongVowel;
            native.LetterCase = (int)LetterCase;
            native.Separator = (int)Separator;
            native.Unknown = (int)Unknown;
            return native;
        }
    }

    /// <summary>Mirrors <c>cppk2_kana_options</c>.</summary>
    public sealed class KanaOptions
    {
        public Script Script { get; set; } = Script.Hiragana;
        public UnknownPolicy Unknown { get; set; } = UnknownPolicy.Keep;

        internal NativeMethods.KanaOptionsNative ToNative()
        {
            var native = new NativeMethods.KanaOptionsNative();
            native.StructSize = (UIntPtr)Marshal.SizeOf<NativeMethods.KanaOptionsNative>();
            var status = NativeMethods.cppk2_kana_options_init(ref native, native.StructSize);
            if (status != Status.Ok)
            {
                throw new CppKana2Exception("cppk2_kana_options_init failed", status);
            }
            native.Script = (int)Script;
            native.Unknown = (int)Unknown;
            return native;
        }
    }

    /// <summary>One unit of a conversion, copied out of the native structure.</summary>
    public sealed class Mora
    {
        internal Mora(string text, string rendered, MoraKind kind, int code)
        {
            Text = text;
            Rendered = rendered;
            Kind = kind;
            Code = code;
        }

        public string Text { get; }
        public string Rendered { get; }
        public MoraKind Kind { get; }
        /// <summary>The library's status for this unit: 0 when it converted.</summary>
        public int Code { get; }
        public bool Ok => Code == Status.Ok;

        public override string ToString() => $"{Text} -> {Rendered} ({Kind})";
    }

    /// <summary>The result of one conversion: the rendered string plus the per-unit breakdown.</summary>
    /// <remarks>
    /// The native memory is copied out and released before this object is handed to the caller, so there is
    /// nothing to dispose and no way to hold a pointer into freed memory.
    /// </remarks>
    public sealed class Conversion
    {
        internal Conversion(IntPtr native)
        {
            if (native == IntPtr.Zero)
            {
                throw new CppKana2Exception("the library returned a null conversion");
            }
            try
            {
                var conversion = Marshal.PtrToStructure<NativeMethods.ConversionNative>(native);
                var moras = new List<Mora>(conversion.MoraCount);
                for (var i = 0; i < conversion.MoraCount; i++)
                {
                    var address = IntPtr.Add(conversion.Moras, i * Marshal.SizeOf<NativeMethods.MoraNative>());
                    var mora = Marshal.PtrToStructure<NativeMethods.MoraNative>(address);
                    moras.Add(new Mora(Utf8.FromPtr(mora.Text), Utf8.FromPtr(mora.Rendered), (MoraKind)mora.Kind,
                                       mora.Code));
                }
                Moras = new ReadOnlyCollection<Mora>(moras);
                Text = Utf8.FromPtr(conversion.Text);
                Rendered = Utf8.FromPtr(conversion.Rendered);
                Ok = conversion.Ok != 0;
            }
            finally
            {
                NativeMethods.cppk2_conversion_free(native);
            }
        }

        public string Text { get; }
        public string Rendered { get; }
        public IReadOnlyList<Mora> Moras { get; }
        /// <summary>True when every unit converted. A call can succeed while this is false (kept unknown characters).</summary>
        public bool Ok { get; }

        public override string ToString() => Rendered;
    }

    /// <summary>The four entry points, mirroring the C ABI.</summary>
    public static class Kana
    {
        static Kana()
        {
            NativeLoader.EnsureRegistered();
        }

        /// <summary>Japanese kana (any script, including half-width) to Hepburn romaji.</summary>
        public static Conversion KanaToRomaji(string text, RomajiOptions? options = null)
        {
            var input = Utf8.Alloc(text ?? throw new InvalidArgumentError("text must not be null"));
            try
            {
                var error = new NativeMethods.ErrorNative();
                IntPtr conversion;
                int status;
                if (options is null)
                {
                    status = NativeMethods.cppk2_kana_to_romaji(input, IntPtr.Zero, out conversion, ref error);
                }
                else
                {
                    var native = options.ToNative();
                    var optionsPointer = Marshal.AllocHGlobal(Marshal.SizeOf<NativeMethods.RomajiOptionsNative>());
                    try
                    {
                        Marshal.StructureToPtr(native, optionsPointer, false);
                        status = NativeMethods.cppk2_kana_to_romaji(input, optionsPointer, out conversion, ref error);
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(optionsPointer);
                    }
                }
                Errors.Throw(status, ref error);
                return new Conversion(conversion);
            }
            finally
            {
                Utf8.Free(input);
            }
        }

        /// <summary>Hepburn romaji (plus Kunrei/IME aliases) to kana.</summary>
        public static Conversion RomajiToKana(string text, KanaOptions? options = null)
        {
            var input = Utf8.Alloc(text ?? throw new InvalidArgumentError("text must not be null"));
            try
            {
                var error = new NativeMethods.ErrorNative();
                IntPtr conversion;
                int status;
                if (options is null)
                {
                    status = NativeMethods.cppk2_romaji_to_kana(input, IntPtr.Zero, out conversion, ref error);
                }
                else
                {
                    var native = options.ToNative();
                    var optionsPointer = Marshal.AllocHGlobal(Marshal.SizeOf<NativeMethods.KanaOptionsNative>());
                    try
                    {
                        Marshal.StructureToPtr(native, optionsPointer, false);
                        status = NativeMethods.cppk2_romaji_to_kana(input, optionsPointer, out conversion, ref error);
                    }
                    finally
                    {
                        Marshal.FreeHGlobal(optionsPointer);
                    }
                }
                Errors.Throw(status, ref error);
                return new Conversion(conversion);
            }
            finally
            {
                Utf8.Free(input);
            }
        }

        /// <summary>Hiragana to katakana and back, normalising half-width input first.</summary>
        public static Conversion ConvertScript(string text, Script target = Script.Hiragana,
                                               UnknownPolicy unknown = UnknownPolicy.Keep)
        {
            var input = Utf8.Alloc(text ?? throw new InvalidArgumentError("text must not be null"));
            try
            {
                var error = new NativeMethods.ErrorNative();
                var status = NativeMethods.cppk2_convert_script(input, (int)target, (int)unknown, out var conversion,
                                                                ref error);
                Errors.Throw(status, ref error);
                return new Conversion(conversion);
            }
            finally
            {
                Utf8.Free(input);
            }
        }

        /// <summary>The native library's package version.</summary>
        public static string Version => Utf8.FromPtr(NativeMethods.cppk2_version());

        /// <summary>The library's wording for a status code.</summary>
        public static string CodeText(int status) => Errors.CodeText(status);
    }
}
