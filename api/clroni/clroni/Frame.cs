using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices;
using System.Security.Permissions;

namespace oni
{
    // Make managed version of oni_frame_t
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct frame_t
    {
        public readonly ulong time;  // Frame time in terms of acq. clock
        public readonly uint dev_idx; // Array of device indices in frame
        public readonly uint data_sz; // Size in bytes of data buffer
        public readonly byte* data; // Multi-device raw data block
    }

    [SecurityPermission(SecurityAction.InheritanceDemand, UnmanagedCode = true)]
    [SecurityPermission(SecurityAction.Demand, UnmanagedCode = true)]
    public unsafe class Frame : SafeHandleZeroOrMinusOneIsInvalid
    {
        protected Frame()
        : base(true)
        {
        }

        // Specialization, because its the most used
        public ushort[] DataU16()
        {
            var frame = (frame_t*)handle.ToPointer();
            ushort[] output = new ushort[frame->data_sz / 2];

            // Make sure the array won't be moved around by the GC
            var outp = GCHandle.Alloc(output, GCHandleType.Pinned);
            var destination = (ushort*)outp.AddrOfPinnedObject().ToPointer();

            // There are faster ways to do this, particularly by using wider types or by
            // handling special lengths.
            for (int i = 0; i < output.Length; i++)
            {
                destination[i] = ((ushort*)frame->data)[i];
            }

            outp.Free();
            return output;
        }

        // Ideally, I would like this to be a "Span" into the existing, allocated frame
        // Now, there are _two_ deep copies happening here as far as I can tell which is ridiculous
        public T[] Data<T>() where T : struct
        {
            var frame = (frame_t*)handle.ToPointer();

            // Get the read size and offset for this device
            var num_bytes = frame->data_sz;

            var buffer = new byte[num_bytes];
            var output = new T[num_bytes / Marshal.SizeOf(default(T))];

            // TODO: Seems like we should be able to copy directly into output or not copy at all!
            Marshal.Copy((IntPtr)frame->data, buffer, 0, (int)num_bytes);
            Buffer.BlockCopy(buffer, 0, output, 0, (int)num_bytes);
            return output;
        }

        [ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
        protected override bool ReleaseHandle()
        {
            NativeMethods.oni_destroy_frame(handle);
            return true;
        }

        // Frame time
        public ulong Clock()
        {
            return ((frame_t*)handle.ToPointer())->time;
        }

        // Device with data in this frame
        public uint DeviceIndex()
        {
            return ((frame_t*)handle.ToPointer())->dev_idx;
        }
    }
}
