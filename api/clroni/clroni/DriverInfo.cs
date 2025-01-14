using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace oni
{
    /// <summary>
    /// Managed wrapper for an ONI-compliant driver translator information structure.
    /// </summary>
    public unsafe class DriverInfo
    {

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        unsafe struct driver_info_t
        {
            public readonly char* name;
            public readonly int major;
            public readonly int minor;
            public readonly int patch;
            public readonly char* pre_release;
        }

        internal DriverInfo(IntPtr driverInfo)
        {
            var p = (driver_info_t*)driverInfo.ToPointer();
            Name = Marshal.PtrToStringAnsi((IntPtr)p->name);
            Major = p->major;
            Minor = p->minor;
            Patch = p->patch;
            PreRelease = Marshal.PtrToStringAnsi((IntPtr)p->pre_release);
        }

        /// <summary>
        /// Device driver translator name
        /// </summary>
        [Description("Driver name.")]
        public readonly string Name;

        /// <summary>
        /// Device driver translator name
        /// </summary>
        [Description("Driver translator major version.")]
        public readonly int Major;

        /// <summary>
        /// Device driver translator name
        /// </summary>
        [Description("Driver translator minor version.")]
        public readonly int Minor;

        /// <summary>
        /// Device driver translator name
        /// </summary>
        [Description("Driver translator patch number.")]
        public readonly int Patch;

        /// <summary>
        /// Device driver translator name
        /// </summary>
        [Description("Driver translator pre-release string.")]
        public readonly string PreRelease;
    }
}
