using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace CppKana2
{
    /// <summary>Finds and loads the native library, and says exactly where it looked when it cannot.</summary>
    /// <remarks>
    /// A bare <see cref="DllNotFoundException"/> from P/Invoke names the file but not the search paths, and
    /// on Windows a missing <em>dependency</em> of the library reports the same "module not found". Listing
    /// the directories that were searched is what turns either case into something a user can act on.
    /// </remarks>
    internal static class NativeLoader
    {
        private static bool _registered;

        internal static void EnsureRegistered()
        {
            if (_registered)
            {
                return;
            }
            NativeLibrary.SetDllImportResolver(typeof(NativeLoader).Assembly, Resolve);
            _registered = true;
        }

        private static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (name != NativeMethods.Library)
            {
                return IntPtr.Zero; // not ours: let the runtime decide
            }
            foreach (var directory in SearchDirectories())
            {
                var candidate = Path.Combine(directory, name);
                if (File.Exists(candidate))
                {
                    if (NativeLibrary.TryLoad(candidate, out var handle))
                    {
                        return handle;
                    }
                    throw new CppKana2Exception(
                        $"found {candidate} but could not load it. On Windows this usually means a dependent "
                        + "DLL of the library itself is missing, not that the file is.");
                }
            }
            throw new CppKana2Exception(
                $"could not find {name}. Searched:\n  " + string.Join("\n  ", SearchDirectories())
                + "\nBuild the native library and stage it next to the assembly, or set CPPK2_NATIVE_DIR.");
        }

        private static string[] SearchDirectories()
        {
            var baseDirectory = AppContext.BaseDirectory;
            var rid = Rid();
            return new[]
            {
                baseDirectory,
                Path.Combine(baseDirectory, "runtimes", rid, "native"),
                Path.Combine(baseDirectory, "native"),
            };
        }

        /// <summary>The runtime identifier the current process would use, for the runtimes/&lt;rid&gt; layout.</summary>
        internal static string Rid()
        {
            var os = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win"
                : RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "osx"
                : "linux";
            var architecture = RuntimeInformation.ProcessArchitecture switch
            {
                Architecture.X64 => "x64",
                Architecture.X86 => "x86",
                Architecture.Arm64 => "arm64",
                Architecture.Arm => "arm",
                _ => "unknown",
            };
            return $"{os}-{architecture}";
        }
    }
}
