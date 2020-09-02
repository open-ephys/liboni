namespace oni.lib
{
    using System;
    using System.Runtime.InteropServices;

    public enum ONIXOption : int
    {
        PORTFUNC = 0,
    }

    // Provide device_t with ONIX-specific decorations
    public partial struct device_t
    {
        public override string ToString() =>
            $@" 0x{idx:X} : {Marshal.PtrToStringAnsi(NativeMethods.onix_device_str(id))}, Read size: {read_size}, Write Size: {write_size}";

        public string Description()
        {
            return Marshal.PtrToStringAnsi(NativeMethods.onix_device_str(id));
        }
    }

    // ONIX-specific extension functions
    public static partial class NativeMethods
    {
        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        public static extern IntPtr onix_device_str(int id);
    }
}
