using Microsoft.Win32.SafeHandles;
using System.Runtime.ConstrainedExecution;
using System.Security.Permissions;


namespace oni
{
#if NET7_0_OR_GREATER
    internal class ContextHandle : SafeHandleZeroOrMinusOneIsInvalid
#else
    [SecurityPermission(SecurityAction.InheritanceDemand, UnmanagedCode = true)]
    [SecurityPermission(SecurityAction.Demand, UnmanagedCode = true)]
    internal unsafe class ContextHandle : SafeHandleZeroOrMinusOneIsInvalid
#endif
    {
        public ContextHandle() : base(true) { }

        protected override bool ReleaseHandle()
        {
            return NativeMethods.oni_destroy_ctx(handle) == 0;
        }
    }
}
