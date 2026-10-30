using System;
using System.Runtime.InteropServices;
using System.Text;

namespace CppKana2
{
    /// <summary>The library's status codes, and the exceptions they raise.</summary>
    public static class Status
    {
        public const int Ok = 0;
        public const int InvalidArgument = 1;
        public const int InvalidUtf8 = 2;
        public const int UnsupportedCharacter = 3;
        public const int NotReady = 4;
        public const int DataCorrupt = 5;
        public const int NotImplemented = 6;
        public const int Internal = 7;
    }

    /// <summary>Base class for every error the native library reports.</summary>
    public class CppKana2Exception : Exception
    {
        public CppKana2Exception(string message, int code = Status.Internal) : base(message)
        {
            Code = code;
        }

        /// <summary>The native status code, when the exception came from one.</summary>
        public int Code { get; }
    }

    public sealed class InvalidArgumentError : CppKana2Exception
    {
        public InvalidArgumentError(string message) : base(message, Status.InvalidArgument) { }
    }

    public sealed class InvalidUtf8Error : CppKana2Exception
    {
        public InvalidUtf8Error(string message) : base(message, Status.InvalidUtf8) { }
    }

    public sealed class UnsupportedCharacterError : CppKana2Exception
    {
        public UnsupportedCharacterError(string message) : base(message, Status.UnsupportedCharacter) { }
    }

    public sealed class NotReadyError : CppKana2Exception
    {
        public NotReadyError(string message) : base(message, Status.NotReady) { }
    }

    public sealed class DataCorruptError : CppKana2Exception
    {
        public DataCorruptError(string message) : base(message, Status.DataCorrupt) { }
    }

    internal static class Errors
    {
        /// <summary>Takes the detail string out of a native error, releasing it, and raises the matching exception.</summary>
        /// <remarks>
        /// The status is cleared after taking the detail so a second call cannot free it twice; an unknown
        /// code becomes the base class rather than a KeyNotFoundException, because a code this shell has
        /// never seen must not crash the caller.
        /// </remarks>
        internal static void Throw(int status, ref NativeMethods.ErrorNative error)
        {
            if (status == Status.Ok)
            {
                return;
            }
            var detail = TakeDetail(ref error);
            var message = CodeText(status) + (string.IsNullOrEmpty(detail) ? string.Empty : ": " + detail);
            throw status switch
            {
                Status.InvalidArgument => new InvalidArgumentError(message),
                Status.InvalidUtf8 => new InvalidUtf8Error(message),
                Status.UnsupportedCharacter => new UnsupportedCharacterError(message),
                Status.NotReady => new NotReadyError(message),
                Status.DataCorrupt => new DataCorruptError(message),
                _ => new CppKana2Exception(message, status),
            };
        }

        internal static string TakeDetail(ref NativeMethods.ErrorNative error)
        {
            if (error.Detail == IntPtr.Zero)
            {
                return string.Empty;
            }
            try
            {
                return Utf8.FromPtr(error.Detail);
            }
            finally
            {
                NativeMethods.cppk2_string_free(error.Detail);
                error.Detail = IntPtr.Zero;
            }
        }

        /// <summary>
        /// The wording for a status code, from the library. Used by <see cref="Throw"/>; the public
        /// <see cref="Kana.CodeText"/> calls the same native function.
        /// </summary>
        internal static string CodeText(int status) => Utf8.FromPtr(NativeMethods.cppk2_code_text(status));
    }

    /// <summary>UTF-8 marshalling: never <c>LPStr</c>, which is the platform ANSI code page and mangles anything non-ASCII.</summary>
    internal static class Utf8
    {
        internal static string FromPtr(IntPtr pointer)
        {
            if (pointer == IntPtr.Zero)
            {
                return string.Empty;
            }
            return Marshal.PtrToStringUTF8(pointer) ?? string.Empty;
        }

        /// <summary>Allocates a NUL-terminated UTF-8 buffer; the caller frees it with <see cref="Free"/>.</summary>
        internal static IntPtr Alloc(string text)
        {
            if (text.IndexOf('\0') >= 0)
            {
                // The C ABI takes NUL-terminated strings; an embedded NUL would truncate silently.
                throw new InvalidArgumentError("text must not contain U+0000");
            }
            var bytes = Encoding.UTF8.GetBytes(text);
            var buffer = Marshal.AllocHGlobal(bytes.Length + 1);
            Marshal.Copy(bytes, 0, buffer, bytes.Length);
            Marshal.WriteByte(buffer, bytes.Length, 0);
            return buffer;
        }

        internal static void Free(IntPtr pointer)
        {
            if (pointer != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(pointer);
            }
        }
    }
}
