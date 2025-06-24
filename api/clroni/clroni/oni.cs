using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security;

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

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial void oni_version(out int major, out int minor, out int patch);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ContextHandle oni_create_ctx(string driver_name);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_init_ctx(ContextHandle ctx, int host_idx);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_destroy_ctx(IntPtr ctx);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_get_opt(ContextHandle ctx, int option, IntPtr val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int oni_get_opt(ContextHandle ctx, int option, [Out] char[] val, IntPtr size);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_set_opt(ContextHandle ctx, int option, IntPtr val, int size);

        [LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_set_opt(ContextHandle ctx, int option, string val, int size);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial IntPtr oni_get_driver_info(ContextHandle ctx);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_get_driver_opt(ContextHandle ctx, int option, IntPtr val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int oni_get_driver_opt(ContextHandle ctx, int option, [Out] char[] val, IntPtr size);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_set_driver_opt(ContextHandle ctx, int option, IntPtr val, int size);

        [LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_set_driver_opt(ContextHandle ctx, int option, string val, int size);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_read_reg(ContextHandle ctx, uint dev_idx, uint addr, IntPtr val);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_write_reg(ContextHandle ctx, uint dev_idx, uint addr, uint val);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_read_frame(ContextHandle ctx, out Frame frame);

        [LibraryImport(LibraryName, SetLastError = true)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_read_frame(ContextHandle ctx, out IntPtr frame);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_create_frame(ContextHandle ctx, out Frame frame, uint dev_idx, IntPtr data, uint data_sz);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_write_frame(ContextHandle ctx, Frame frame);

        [LibraryImport(LibraryName, SetLastError = true)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_create_frame(ContextHandle ctx, out IntPtr frame, uint dev_idx, IntPtr data, uint data_sz);

        [LibraryImport(LibraryName, SetLastError = true)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial int oni_write_frame(ContextHandle ctx, IntPtr frame);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial void oni_destroy_frame(IntPtr frame);

        [LibraryImport(LibraryName)]
        [UnmanagedCallConv(CallConvs = new Type[] { typeof(CallConvCdecl) })]
        internal static partial IntPtr oni_error_str(int err);
    }
}
