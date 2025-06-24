using Microsoft.Win32.SafeHandles;

namespace oni
{
    internal class ContextHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        internal ContextHandle() : base(true) { }

        protected override bool ReleaseHandle()
        {
            return NativeMethods.oni_destroy_ctx(handle) == 0;
        }
    }
}
