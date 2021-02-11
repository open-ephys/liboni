using System.ComponentModel;

namespace oni
{
    /// <summary>
    /// ONI-compliant device hub.
    /// </summary>
    public partial class Hub
    {
        /// <summary>
        /// Hub sub-address of a fully qualified <seealso cref="Device.Address"/>.
        /// </summary>
        [ReadOnly(true)]
        public byte Address { get; set; }
    }
}
