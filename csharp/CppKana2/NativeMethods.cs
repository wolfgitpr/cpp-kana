using System;
using System.Runtime.InteropServices;

namespace CppKana2
{
    /// <summary>The C ABI, transcribed. One entry per <c>CPPK2_API</c> declaration in c_api.h.</summary>
    /// <remarks>
    /// <c>ExactSpelling = true</c> and <c>CallingConvention = Cdecl</c> are both explicit: the default
    /// calling convention on Windows is stdcall, which would corrupt the stack on the first call.
    /// </remarks>
    internal static class NativeMethods
    {
        internal const string Library = BuildInfo.NativeLibraryName;

        [StructLayout(LayoutKind.Sequential)]
        internal struct MoraNative
        {
            public IntPtr Text;
            public IntPtr Rendered;
            public int Kind;
            public int Code;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ConversionNative
        {
            public IntPtr Text;
            public IntPtr Rendered;
            public IntPtr Moras;
            public int MoraCount;
            public int Ok;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct ErrorNative
        {
            public int Code;
            public int Reserved0;
            public IntPtr Detail;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct RomajiOptionsNative
        {
            public UIntPtr StructSize;
            public int Sokuon;
            public int Hatsuon;
            public int LongVowel;
            public int LetterCase;
            public int Separator;
            public int Unknown;
            // Two reserved fields so that the tail padding of the 40-byte layout is claimed: otherwise a new
            // int field would leave sizeof unchanged and defeat the struct_size handshake.
            public int Reserved1;
            public int Reserved2;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct KanaOptionsNative
        {
            public UIntPtr StructSize;
            public int Script;
            public int Unknown;
            // Two reserved fields so that the tail padding of the 40-byte layout is claimed: otherwise a new
            // int field would leave sizeof unchanged and defeat the struct_size handshake.
            public int Reserved1;
            public int Reserved2;
        }

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr cppk2_version();

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr cppk2_code_text(int code);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int cppk2_romaji_options_init(ref RomajiOptionsNative options, UIntPtr structSize);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int cppk2_kana_options_init(ref KanaOptionsNative options, UIntPtr structSize);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int cppk2_kana_to_romaji(IntPtr kana, IntPtr options, out IntPtr conversion,
                                                        ref ErrorNative error);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int cppk2_romaji_to_kana(IntPtr romaji, IntPtr options, out IntPtr conversion,
                                                        ref ErrorNative error);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int cppk2_convert_script(IntPtr text, int script, int unknown, out IntPtr conversion,
                                                        ref ErrorNative error);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void cppk2_conversion_free(IntPtr conversion);

        [DllImport(Library, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void cppk2_string_free(IntPtr text);
    }
}
