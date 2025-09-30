using System;
using System.Runtime.InteropServices;
using System.Security;
using System.Text;

namespace oni
{
    /// <summary>
    /// Common language runtime bindings to liboni C library. The functionality
    /// these bindings is exposed through higher-order types: <seealso cref="Context"/>,
    /// <seealso cref="Hub"/>, <seealso cref="Device"/>, <seealso cref="Frame"/>,
    /// and <seealso cref="ONIException"/>.
    /// </summary>
    [SuppressUnmanagedCodeSecurity] // NB: Call into native code without incurring the performance loss of a run-time security check when doing so
    public static partial class NativeMethods
    {
        /// <summary>
        /// <see href="https://semver.org/">Semantic version</see> of this library.
        /// </summary>
        public static readonly Version LibraryVersion;

        const string LibraryName = "liboni";

        static NativeMethods()
        {
            // Set once LibraryVersion to version()
            oni_version(out int major, out int minor, out int patch);
            LibraryVersion = new Version(major, minor, patch);

            // Make sure it is supported
            if (major < 4)
            {
                throw VersionNotSupported(null, ">= v4.0.0");
            }
        }

        private static NotSupportedException VersionNotSupported(string methodName, string requiredVersion)
        {
            return new NotSupportedException(
                    string.Format(
                        "{0}liboni version not supported. Required version {1}",
                        methodName == null ? string.Empty : methodName + ": ",
                        requiredVersion));
        }
    }
}
