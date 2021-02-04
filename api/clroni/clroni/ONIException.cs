using System;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace oni
{
    /// <summary>
    /// Exception wrapper for error codes returned by the native ONI API.
    /// </summary>
    [Serializable]
    public class ONIException : Exception
    {
        /// <summary>
        /// The underlying ONI API error code.
        /// </summary>
        public readonly int Number;

        protected ONIException() { }

        /// <summary>
        /// Initializes a new <see cref="oni.ONIException"/>
        /// </summary>
        /// <param name="errnum">The error code returned by the native ONI API.</param>
        internal ONIException(int errnum)
        {
            Number = errnum;
        }

        /// <summary>
        /// Get a human-readable version of the error code produced by the native ONI API.
        /// </summary>
        public override string Message =>
            Marshal.PtrToStringAnsi(NativeMethods.oni_error_str(Number));

        protected ONIException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        { }
    }
}
