using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security.Permissions;
using System.Text;

namespace oni
{
    public unsafe class Context : IDisposable
    {
        // NB: These need to be redeclared unfortunately
        public enum Option : int
        {
            DEVICETABLE = 0,
            NUMDEVICES,
            RUNNING,
            RESET,
            SYSCLKHZ,
            ACQCLKHZ,
            RESETACQCOUNTER,
            HWADDRESS,
            MAXREADFRAMESIZE,
            MAXWRITEFRAMESIZE,
            BLOCKREADSIZE,
            BLOCKWRITESIZE,
            CUSTOMBEGIN,
        }

        // ONIX hardware uses device 254 within each hub for the hub manager
        private const int HUB_MGR_ADDRESS = 254;

        // Constructor initialized
        public readonly uint SystemClockHz;
        public readonly uint AcquisitionClockHz;
        public readonly uint MaxReadFrameSize;
        public readonly uint MaxWriteFrameSize;
        public readonly Dictionary<uint, device_t> DeviceTable;

        // The safe handle used for API interaction
        private readonly ContextHandle ctx;

        public Context(string driver, int index)
        {
            // Create context
            ctx = NativeMethods.oni_create_ctx(driver);

            if (ctx.IsInvalid)
            {
                throw new InvalidProgramException(string.Format("Failed to create an " +
                    "acquisition context for the specified driver: {0}.", driver));
            }

            var rc = NativeMethods.oni_init_ctx(ctx, index);
            if (rc != 0) { throw new ONIException(rc); }

            // Get context metadata
            SystemClockHz = (uint)GetIntOption((int)Option.SYSCLKHZ);
            AcquisitionClockHz = (uint)GetIntOption((int)Option.ACQCLKHZ);
            MaxReadFrameSize = (uint)GetIntOption((int)Option.MAXREADFRAMESIZE);
            MaxWriteFrameSize = (uint)GetIntOption((int)Option.MAXWRITEFRAMESIZE);

            // Populate device table
            int num_devs = GetIntOption((int)Option.NUMDEVICES);
            DeviceTable = new Dictionary<uint, device_t>(num_devs);
            int size_dev = Marshal.SizeOf(new device_t());
            int size = size_dev * num_devs; // bytes required to read table

            var table = GetOption((int)Option.DEVICETABLE, size);

            for (int i = 0; i < num_devs; i++)
            {
                var d = (device_t)Marshal.PtrToStructure(table, typeof(device_t));
                DeviceTable.Add(d.idx, d);
                table = new IntPtr((long)table + size_dev);
            }
        }

        // GetOption
        private IntPtr GetOption(int option, int size, bool drv_opt = false)
        {
            // NB: If I don't do all this wacky stuff, the size
            // parameter ends up being wrong for 64-bit compilation.
            var sz = Marshal.AllocHGlobal(IntPtr.Size);
            if (IntPtr.Size == 4)
            {
                Marshal.WriteInt32(sz, size);
            }
            else
            {
                Marshal.WriteInt64(sz, size);
            }

            var value = Marshal.AllocHGlobal(size);

            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_get_opt(ctx, option, value, sz);
            }
            else
            {
                rc = NativeMethods.oni_get_driver_opt(ctx, option, value, sz);
            }

            if (rc != 0) { throw new ONIException(rc); }
            return value;
        }

        // Int32 GetOption
        private int GetIntOption(int option, bool drv_opt = false)
        {
            var value = GetOption(option, Marshal.SizeOf(typeof(int)), drv_opt);
            return Marshal.ReadInt32(value);
        }

        // String GetOption
        private string GetStringOption(int option, bool drv_opt = false)
        {
            var sz = Marshal.AllocHGlobal(IntPtr.Size);
            if (IntPtr.Size == 4)
            {
                Marshal.WriteInt32(sz, 1000);
            }
            else
            {
                Marshal.WriteInt64(sz, 1000);
            }

            var str = new StringBuilder(1000);

            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_get_opt(ctx, option, str, sz);
            }
            else
            {
                rc = NativeMethods.oni_get_driver_opt(ctx, option, str, sz);
            }

            if (rc != 0) { throw new ONIException(rc); }
            return str.ToString();
        }

        // Int32 SetOption
        private void SetIntOption(int opt, int value, bool drv_opt = false)
        {
            var val = Marshal.AllocHGlobal(IntPtr.Size);
            if (IntPtr.Size == 4)
            {
                Marshal.WriteInt32(val, value);
            }
            else
            {
                Marshal.WriteInt64(val, value);
            }

            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_set_opt(ctx, opt, val, 4);
            }
            else
            {
                rc = NativeMethods.oni_set_driver_opt(ctx, opt, val, 4);
            }

            if (rc != 0) { throw new ONIException(rc); }
        }

        // String SetOption
        private void SetStringOption(int opt, string value, bool drv_opt)
        {
            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_set_opt(ctx, opt, value, value.Length + 1);
            }
            else
            {
                rc = NativeMethods.oni_set_driver_opt(ctx, opt, value, value.Length + 1);
            }

            if (rc != 0) { throw new ONIException(rc); }
        }

        public void SetCustomOption(int option, int value)
        {
            SetIntOption((int)Option.CUSTOMBEGIN + option, value);
        }

        public int GetCustomOption(int option)
        {
            return GetIntOption((int)Option.CUSTOMBEGIN + option);
        }

        public void Start(bool reset_frame_clock = true)
        {
            if (reset_frame_clock)
            {
                SetIntOption((int)Option.RESETACQCOUNTER, 2);
            }
            else
            {
                SetIntOption((int)Option.RUNNING, 1);
            }
        }

        public void Stop()
        {
            SetIntOption((int)Option.RUNNING, 0);
        }

        public void ResetFrameClock()
        {
            SetIntOption((int)Option.RESETACQCOUNTER, 1);
        }

        public bool Running
        {
            get
            {
                return GetIntOption((int)Option.RUNNING) > 0;
            }
        }

        public int HardwareAddress
        {
            get
            {
                return GetIntOption((int)Option.HWADDRESS);
            }
            set
            {
                SetIntOption((int)Option.HWADDRESS, value);
            }
        }

        public int BlockReadSize
        {
            get
            {
                return GetIntOption((int)Option.BLOCKREADSIZE);
            }
            set
            {
                SetIntOption((int)Option.BLOCKREADSIZE, value);
            }
        }

        public int BlockWriteSize
        {
            get
            {
                return GetIntOption((int)Option.BLOCKWRITESIZE);
            }
            set
            {
                SetIntOption((int)Option.BLOCKWRITESIZE, value);
            }
        }

        public uint ReadRegister(uint? dev_index, uint register_address)
        {
            if (dev_index == null)
            {
                throw new ArgumentNullException("dev_index", "Attempt to read register from invalid device.");
            }

            var val = Marshal.AllocHGlobal(4);
            int rc = NativeMethods.oni_read_reg(ctx, (uint)dev_index, register_address, val);
            if (rc != 0) { throw new ONIException(rc); }
            return (uint)Marshal.ReadInt32(val);
        }

        public void WriteRegister(uint? dev_index, uint register_address, uint value)
        {
            if (dev_index == null)
            {
                throw new ArgumentNullException("dev_index", "Attempt to write to register of invalid device.");
            }

            int rc = NativeMethods.oni_write_reg(ctx, (uint)dev_index, register_address, value);
            if (rc != 0) { throw new ONIException(rc); }
        }

        public Frame ReadFrame()
        {
            int rc = NativeMethods.oni_read_frame(ctx, out Frame frame);
            if (rc < 0) { throw new ONIException(rc); }
            return frame;
        }

        public void Write<T>(uint dev_idx, T data) where T : struct
        {
            Write(dev_idx, new T[] { data });
        }

        public void Write<T>(uint dev_idx, T[] data) where T : struct
        {
            var num_bytes = Buffer.ByteLength(data);
            var buffer = new byte[num_bytes];
            Buffer.BlockCopy(data, 0, buffer, 0, num_bytes);

            fixed (byte* p = buffer)
            {
                int rc = NativeMethods.oni_create_frame(ctx, out Frame frame, dev_idx, (IntPtr)p, (uint)num_bytes);
                if (rc < 0) { throw new ONIException(rc); }

                rc = NativeMethods.oni_write_frame(ctx, frame);
                if (rc < 0) { throw new ONIException(rc); }
            }
        }

        public void Write(uint dev_idx, IntPtr data, int data_size)
        {
            int rc = NativeMethods.oni_create_frame(ctx, out Frame frame, dev_idx, data, (uint)data_size);
            if (rc < 0) { throw new ONIException(rc); }

            rc = NativeMethods.oni_write_frame(ctx, frame);
            if (rc < 0) { throw new ONIException(rc); }
        }

        public uint? HubDataClock(uint hub_idx)
        {
            return ReadRegister(HUB_MGR_ADDRESS + hub_idx, 4);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        [SecurityPermission(SecurityAction.Demand, UnmanagedCode = true)]
        protected virtual void Dispose(bool disposing)
        {

            if (ctx != null && !ctx.IsInvalid)
            {
                ctx.Dispose();
            }
        }
    }
}
