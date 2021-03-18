using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security;

namespace oni
{
    /// <summary>
    /// ONI-compliant extension context options (beyond those that are meaningfully
    /// interpreted by the ONI API) and are simply forwarded to hardware to control
    /// implementation-specific functions.
    /// </summary>
    public enum ONIXOption : int
    {
        /// <summary>
        /// Toggle the hub port between raw pass-through mode and device demultiplexing
        /// mode.
        /// </summary>
        PORTFUNC = 0,
    }

    // Provides oni.Device with ONIX-specific extensions.
    public partial struct Device
    {
        /// <summary>
        /// Retrieve a human readable description of the <see cref="Device.ID"/>.
        /// </summary>
        public string Description => Marshal.PtrToStringAnsi(NativeMethods.onix_device_str(ID));
    }

    public partial class Hub
    {
        /// <summary>
        /// ONIX hub hardware ID (e.g. host or headstage type).
        /// </summary>
        [ReadOnly(true)]
        public int HardwareID { get; set; }

        /// <summary>
        /// ONIX hub firmware version.
        /// </summary>
        [ReadOnly(true)]
        public uint FirmwareVersion { get; set; }

        /// <summary>
        /// ONIX hub local clock rate in Hz.
        /// </summary>
        [ReadOnly(true)]
        public uint ClockHz { get; set; }

        /// <summary>
        /// ONIX hub transmission delay with respect to hub 0 with a given
        /// <see cref="Context"/>.
        /// </summary>
        [ReadOnly(true)]
        public uint DelayNanoSeconds { get; set; }

        /// <summary>
        /// Retrieve a human readable description of the <see cref="Hub.HardwareID"/>.
        /// </summary>
        public string Description => Marshal.PtrToStringAnsi(NativeMethods.onix_hub_str(HardwareID));
    }

    public unsafe partial class Context : IDisposable
    {
        // ONIX hardware uses device 254 within each hub for the hub manager
        private const uint HUB_MGR_ADDRESS = 254;

        private enum HubRegister : uint
        {
            HARDWAREID = 0, // Hub hardware ID
            FIRMWAREVER = 2, // Hub firmware version
            CLKRATEHZ = 4, // Hub clock rate in Hz
            DELAYNS = 5, // Hub to host transmission delay in nanoseconds
        }

        /// <summary>
        /// Retrieve the hub that a given <see cref="Device"/> resides in.
        /// </summary>
        /// <param name="device_address">The fully specified <see cref="Device.Address"/>
        /// to retrieve the hub for.</param>
        /// <returns>A <see cref="Hub"/>containing hub metadata.</returns>
        public Hub GetHub(uint device_address)
        {
            var hub_address = device_address & 0x0000FF00;
            return new Hub()
            {
                Address = (byte)(hub_address >> 8),
                HardwareID = (int)ReadRegister(HUB_MGR_ADDRESS + hub_address, (uint)HubRegister.HARDWAREID),
                FirmwareVersion = ReadRegister(HUB_MGR_ADDRESS + hub_address, (uint)HubRegister.FIRMWAREVER),
                ClockHz = ReadRegister(HUB_MGR_ADDRESS + hub_address, (uint)HubRegister.CLKRATEHZ),
                DelayNanoSeconds = ReadRegister(HUB_MGR_ADDRESS + hub_address, (uint)HubRegister.DELAYNS)
            };
        }

        // TODO: Needed or useful?
        //public Dictionary<uint, Device> HubDevicetTable(byte hub_address)
        //{
        //    // TODO
        //}
    }

    // ONIX-specific API extension functions.
    [SuppressUnmanagedCodeSecurity] // NB: Call into native code without incurring the performance loss of a run-time security check when doing so
    public static partial class NativeMethods
    {
        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern IntPtr onix_device_str(int id);

        [DllImport(LibraryName, CallingConvention = CCCdecl)]
        internal static extern IntPtr onix_hub_str(int id);
    }
}
