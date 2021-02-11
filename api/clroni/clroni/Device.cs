using System.Runtime.InteropServices;

namespace oni
{
    /// <summary>
    /// Managed wrapper for the native API's oni_device_t.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public partial struct Device
    {
        /// <summary>
        /// Fully-qualified <b>RSV.RSV.HUB.IDX</b> <see cref="Context.DeviceTable"/>
        /// device table address.
        /// - <b>RSV</b>: 8-bit unsigned integer (reserved)
        /// - <b>HUB</b>: 8-bit unsigned integer indicating the hub index
        /// - <b>IDX</b>: 8-bit unsigned integer indicating the device index
        /// </summary>
        public readonly uint Address;

        /// <summary>
        /// <see cref="Device"/> ID
        /// </summary>
        public readonly int ID;

        /// <summary>
        /// <see cref="Device"/> firmware version
        /// </summary>
        public readonly uint Version;

        /// <summary>
        /// Input <see cref="Frame"/> read size
        /// </summary>
        public readonly uint ReadSize;

        /// <summary>
        /// Output <see cref="Frame"/> write size.
        /// </summary>
        public readonly uint WriteSize;
    }
}
