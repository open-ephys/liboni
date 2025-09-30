using oni;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices;
using System.Text;

namespace oni
{
    public static partial class NativeMethods
    {
        private const CallingConvention CCCdecl = CallingConvention.Cdecl;
        internal static extern void oni_version(out int major, out int minor, out int patch);

        [DllImport(LibraryName, CallingConvention = CCCdecl, CharSet = CharSet.Ansi)]
        internal static extern ContextHandle oni_create_ctx(string driver_name);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_init_ctx(ContextHandle ctx, int host_idx);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        [ReliabilityContract(Consistency.WillNotCorruptState, Cer.MayFail)]
        internal static extern int oni_destroy_ctx(IntPtr ctx);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_get_opt(ContextHandle ctx, int option, IntPtr val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_get_opt(ContextHandle ctx, int option, StringBuilder val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_set_opt(ContextHandle ctx, int option, IntPtr val, int size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_set_opt(ContextHandle ctx, int option, string val, int size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern IntPtr oni_get_driver_info(ContextHandle ctx);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_get_driver_opt(ContextHandle ctx, int option, IntPtr val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_get_driver_opt(ContextHandle ctx, int option, StringBuilder val, IntPtr size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_set_driver_opt(ContextHandle ctx, int option, IntPtr val, int size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_set_driver_opt(ContextHandle ctx, int option, string val, int size);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_read_reg(ContextHandle ctx, uint dev_idx, uint addr, IntPtr val);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern int oni_write_reg(ContextHandle ctx, uint dev_idx, uint addr, uint val);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_read_frame(ContextHandle ctx, out Frame frame);

        [DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        internal static extern int oni_read_frame(ContextHandle ctx, out IntPtr frame);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_create_frame(ContextHandle ctx, out Frame frame, uint dev_idx, IntPtr data, uint data_sz);

        //[DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        //internal static extern int oni_write_frame(ContextHandle ctx, Frame frame);

        [DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        internal static extern int oni_create_frame(ContextHandle ctx, out IntPtr frame, uint dev_idx, IntPtr data, uint data_sz);

        [DllImport(LibraryName, CallingConvention = CCCdecl, SetLastError = true)]
        internal static extern int oni_write_frame(ContextHandle ctx, IntPtr frame);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        [ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
        internal static extern void oni_destroy_frame(IntPtr frame);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern IntPtr oni_error_str(int err);
    }
}
