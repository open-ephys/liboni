using System;
using System.Runtime.InteropServices;

namespace oni
{
    public enum ONIXOption : int
    {
        PORTFUNC = 0,
    }

    // Provide oni.Device with ONIX-specific decorations
    public partial struct Device
    {
        /// <summary>
        /// Convert the <see cref="Device"/> a representative string.
        /// </summary>
        /// <returns>String encoding the <see cref="Device"/>.</returns>
        public override string ToString() =>
            $@" 0x{Index:X} : {Marshal.PtrToStringAnsi(NativeMethods.onix_device_str(ID))}, Read size: {ReadSize}, Write Size: {WriteSize}";

        /// <summary>
        /// Retrieve a human readable description of the <see cref="Device.ID"/>.
        /// </summary>
        /// <returns>A human readable description of the <see cref="Device.ID"/></returns>
        public string Description()
        {
            return Marshal.PtrToStringAnsi(NativeMethods.onix_device_str(ID));
        }
    }

    // ONIX-specific API extension functions.
    internal static partial class NativeMethods
    {
        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        public static extern IntPtr onix_device_str(int id);
    }
}
