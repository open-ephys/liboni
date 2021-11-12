using System.ComponentModel;
using System.Runtime.InteropServices;

namespace oni
{
    /// <summary>
    /// Managed wrapper for the native API's oni_device_t.
    /// </summary>
    [Description("Open neuro interface device.")]
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
        [Description("Fully-qualified device address.")]
        public readonly uint Address;

        /// <summary>
        /// <see cref="Device"/> identifier
        /// </summary>
        [Description("Device identifier.")]
        public readonly int ID;

        /// <summary>
        /// <see cref="Device"/> gateware version
        /// </summary>
        [Description("Device gateware version.")]
        public readonly uint Version;

        /// <summary>
        /// Device to host <see cref="Frame"/> read size
        /// </summary>
        [Description("Device to host frame read size.")]
        public readonly uint ReadSize;

        /// <summary>
        /// Host to device <see cref="Frame"/> write size.
        /// </summary>
        [Description("Host to device frame write size.")]
        public readonly uint WriteSize;
    }
}
