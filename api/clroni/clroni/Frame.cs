using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices;
using System.Security.Permissions;

namespace oni
{
    /// <summary>
    /// Managed wrapper for an ONI-compliant data frame implementation. Produced by calls
    /// to <see cref="Context.ReadFrame"/> .
    /// </summary>
    [SecurityPermission(SecurityAction.InheritanceDemand, UnmanagedCode = true)]
    [SecurityPermission(SecurityAction.Demand, UnmanagedCode = true)]
    public unsafe class Frame : SafeHandleZeroOrMinusOneIsInvalid
    {

        [StructLayout(LayoutKind.Sequential)]
        unsafe struct frame_t
        {
            public readonly ulong time;  // Frame time in terms of acq. clock
            public readonly uint dev_idx; // Array of device indices in frame
            public readonly uint data_sz; // Size in bytes of data buffer
            public readonly byte* data; // Multi-device raw data block
        }

        internal Frame(IntPtr handle)
        : base(true)
        {
            SetHandle(handle);
            GC.AddMemoryPressure(((frame_t*)handle.ToPointer())->data_sz);
        }

        /// <summary>
        /// Executes the code required to free native resources held by the <see cref="Frame"/>.
        /// </summary>
        /// <returns>True if the handle is released successfully (always the case
        /// in this implementation)</returns>
        [ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
        protected override bool ReleaseHandle()
        {
            GC.RemoveMemoryPressure(((frame_t*)handle.ToPointer())->data_sz);
            NativeMethods.oni_destroy_frame(handle);
            return true;
        }

        /// <summary>
        /// Retrieve a managed copy of the <see cref="Frame"/> data.
        /// </summary>
        /// <returns>Array containing the <see cref="Frame"/> data.</returns>
        public T[] Data<T>() where T : unmanaged
        {
            var frame = (frame_t*)handle.ToPointer();
            var output = new T[frame->data_sz / sizeof(T)];

            fixed (T* output_ptr = output)
            {
                // TODO: When we can use .NET 5.0
                // Unsafe.CopyBlock(output_ptr, frame->data, frame->data_sz);

                for (int i = 0; i < output.Length; i++)
                {
                    output_ptr[i] = ((T*)frame->data)[i];
                }
            }

            return output;
        }

        /// <summary>
        /// Retrieve the host acquisition clock counter value at frame creation.
        /// See <see cref="Context.AcquisitionClockHz"/> and
        /// <see cref="Context.ResetFrameClock"/>.
        /// </summary>
        public ulong Clock => ((frame_t*)handle.ToPointer())->time;

        /// <summary>
        /// The fully qualified <see cref="Device.Address"/> of the device within
        /// the current <see cref="Context.DeviceTable"/> that produced this frame.
        /// </summary>
        public uint DeviceAddress => ((frame_t*)handle.ToPointer())->dev_idx;

        /// <summary>
        /// Get the payload data size in bytes. See <see cref="Frame.Data{T}"/>.
        /// </summary>
        public uint DataSize => ((frame_t*)handle.ToPointer())->data_sz;
    }
}
