using System;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace oni
{
    [Serializable]
    public class ONIException : Exception
    {
        public readonly int Number;

        protected ONIException() { }

        public ONIException(int errnum)
        {
            Number = errnum;
        }

        public override string ToString()
        {
            return Marshal.PtrToStringAnsi(lib.NativeMethods.oni_error_str(Number));
        }

        public override string Message
        {
            get
            {
                return ToString();
            }
        }

        protected ONIException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        { }
    }
}
