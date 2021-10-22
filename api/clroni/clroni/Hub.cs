using System.ComponentModel;

namespace oni
{
    /// <summary>
    /// ONI-compliant device hub.
    /// </summary>
    public partial class Hub
    {
        /// <summary>
        /// Hub sub-address of a fully qualified <see cref="Device.Address"/>.
        /// </summary>
        [ReadOnly(true)]
        [Description("Hub sub-address of a fully qualified device address.")]
        public byte Address { get; set; }
    }
}
