using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security.Permissions;
using System.Text;


namespace oni
{
    /// <summary>
    /// Open Neuro Interface (ONI) compliant acquisition context.
    /// </summary>
    public unsafe partial class Context : IDisposable
    {
        // NB: These need to be redeclared unfortunately
        private enum Option : int
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

        // The safe handle used for API interaction
        private readonly ContextHandle ctx;


        /// <summary>
        /// Information about the hardware device driver that is being used the by the context.
        /// </summary>
        public DriverInfo Driver { get; private set; }

        // Constructor initialized
        /// <summary>
        /// Host system clock frequency in Hz. This describes the frequency of
        /// the clock governing the host hardware.
        /// </summary>
        public uint SystemClockHz { get; private set; }

        /// <summary>
        /// Host system acquisition clock frequency in Hz, derived from <see cref="SystemClockHz"/>.
        /// This describes the frequency of the clock used to drive the acquisition
        /// counter which is used timestamp data frames.
        /// </summary>
        public uint AcquisitionClockHz { get; private set; }

        /// <summary>
        /// The maximal size of a frame produced by a call to <see cref="ReadFrame"/>
        /// in bytes. This number is the maximum sized frame that can be produced
        /// across every device within the device table that generates read data.
        /// </summary>
        public uint MaxReadFrameSize { get; private set; }

        /// <summary>
        /// The maximal size of consumed by a call to <see cref="Write"/> in bytes.
        /// This number is the maximum sized frame that can be consumed across every
        /// device within the device table that accepts write data.
        /// </summary>
        public uint MaxWriteFrameSize { get; private set; }

        /// <summary>
        /// ONI specified device table containing the full device hierarchy
        /// governed by this acquisition context. This <see cref="IDictionary"/>
        /// maps a fully-qualified <see cref="Device.Address"/> to a
        /// <see cref="Device"/> instance.
        /// </summary>
        public Dictionary<uint, Device> DeviceTable { get; private set; }

        /// <summary>
        /// Initializes a new instance of <see cref="Context"/> with the
        /// specified hardware translation driver and host hardware index.
        /// </summary>
        /// <param name="driver">A string specifying the device driver used by
        /// the context to control hardware. This string corresponds a compiled
        /// implementation of onidriver.h that has the name onidriver_&lt;drv_name&gt;.so/dll.</param>
        /// <param name="index">An index specifying the physical location within
        /// the host that the host hardware resides. A value of -1 will use the
        /// default location.</param>
        /// <exception cref="System.InvalidProgramException">Thrown when the
        /// specified driver cannot be found or is invalid.</exception>
        /// <exception cref="ONIException">Thrown when there is an error during
        /// hardware initialization (e.g. an invalid device table).</exception>
        public Context(string driver, int index = -1)
        {
            // Create context
            ctx = NativeMethods.oni_create_ctx(driver);

            if (ctx.IsInvalid)
            {
                throw new InvalidProgramException(string.Format("Failed to create an " +
                    "acquisition context for the specified driver: {0}.", string.IsNullOrEmpty(driver) ? "None" : driver));
            }

            try
            {
                var rc = NativeMethods.oni_init_ctx(ctx, index);
                if (rc != 0) { throw new ONIException(rc); }

                Driver = new DriverInfo(NativeMethods.oni_get_driver_info(ctx));

                PopulateDeviceTable();
            }
            catch
            {
                // NB: If initialization failed, caller can't dispose so do it here
                ctx.Dispose();
                throw;
            }
        }

        // Populate device table
        private void PopulateDeviceTable()
        {
            // Get context metadata
            SystemClockHz = (uint)GetIntOption((int)Option.SYSCLKHZ);
            AcquisitionClockHz = (uint)GetIntOption((int)Option.ACQCLKHZ);
            MaxReadFrameSize = (uint)GetIntOption((int)Option.MAXREADFRAMESIZE);
            MaxWriteFrameSize = (uint)GetIntOption((int)Option.MAXWRITEFRAMESIZE);

            int num_devs = GetIntOption((int)Option.NUMDEVICES);
            DeviceTable = new Dictionary<uint, Device>(num_devs);
            int size_dev = Marshal.SizeOf(new Device());
            int size = size_dev * num_devs; // bytes required to read table

            var table = Marshal.AllocHGlobal(size);
            try
            {
                GetOption((int)Option.DEVICETABLE, table, size);

                for (int i = 0; i < num_devs; i++)
                {
                    var entry = new IntPtr((long)table + i * size_dev);
                    var d = (Device)Marshal.PtrToStructure(entry, typeof(Device));
                    DeviceTable.Add(d.Address, d);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(table);
            }
        }

        /// <summary>
        /// Issues a reset command to the hardware and refreshes the device table.
        /// </summary>
        public void Refresh()
        {
            SetIntOption((int)Option.RESET, 1);
            PopulateDeviceTable();
        }

        // NB: The option and register helpers pass pointers to stack variables rather than
        // allocating native memory, which they would otherwise need to free on every call.
        // Option sizes are size_t, so they are passed as a pointer to a pointer-sized value.

        // GetOption
        private void GetOption(int option, IntPtr value, int size, bool drv_opt = false)
        {
            var sz = (IntPtr)size;

            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_get_opt(ctx, option, value, (IntPtr)(&sz));
            }
            else
            {
                rc = NativeMethods.oni_get_driver_opt(ctx, option, value, (IntPtr)(&sz));
            }

            if (rc != 0) { throw new ONIException(rc); }
        }

        // Int32 GetOption
        private int GetIntOption(int option, bool drv_opt = false)
        {
            int value;
            GetOption(option, (IntPtr)(&value), sizeof(int), drv_opt);
            return value;
        }

        // String GetOption
        private string GetStringOption(int option, bool drv_opt = false)
        {
            const int BufferSize = 1000;

            var sz = (IntPtr)BufferSize;

#if NET7_0_OR_GREATER
            var str = new char[BufferSize];
#else
            var str = new StringBuilder(BufferSize);
#endif

            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_get_opt(ctx, option, str, (IntPtr)(&sz));
            }
            else
            {
                rc = NativeMethods.oni_get_driver_opt(ctx, option, str, (IntPtr)(&sz));
            }

            if (rc != 0) { throw new ONIException(rc); }
            return str.ToString();
        }

        // Int32 SetOption
        private void SetIntOption(int opt, int value, bool drv_opt = false)
        {
            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_set_opt(ctx, opt, (IntPtr)(&value), sizeof(int));
            }
            else
            {
                rc = NativeMethods.oni_set_driver_opt(ctx, opt, (IntPtr)(&value), sizeof(int));
            }

            if (rc != 0) { throw new ONIException(rc); }
        }

        // String SetOption
        private void SetStringOption(int opt, string value, bool drv_opt)
        {
            int rc;
            if (!drv_opt)
            {
                rc = NativeMethods.oni_set_opt(ctx, opt, value, (nuint)(value.Length + 1));
            }
            else
            {
                rc = NativeMethods.oni_set_driver_opt(ctx, opt, value, (nuint)(value.Length + 1));
            }

            if (rc != 0) { throw new ONIException(rc); }
        }
        /// <summary>
        /// Set an the value of implementation-specific context option (those
        /// beyond ONI-specified options).
        /// </summary>
        /// <param name="option">Implementation-specific option to set.</param>
        /// <param name="value">Option value</param>
        public void SetCustomOption(int option, int value)
        {
            SetIntOption((int)Option.CUSTOMBEGIN + option, value);
        }
        /// <summary>
        /// Retrieve the value of an implementation-specific context option.
        /// </summary>
        /// <param name="option">Context option to retrieve.</param>
        /// <returns>Value of the context option.</returns>
        /// <exception cref="ONIException"> Thrown when option is invalid.
        /// </exception>
        public int GetCustomOption(int option)
        {
            return GetIntOption((int)Option.CUSTOMBEGIN + option);
        }

        /// <summary>
        /// Start data acquisition.
        /// </summary>
        /// <param name="reset_frame_clock">If true, the acquisition clock counter
        /// is reset synchronously to the start of data acquisition in hardware (see
        /// <see cref="AcquisitionClockHz"/>). This is equivalent to
        /// calling this function and <see cref="ResetFrameClock"/> at precisely
        /// the same moment, which is impossible in software.</param>
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

        /// <summary>
        /// Stop data acquisition.
        /// </summary>
        public void Stop()
        {
            SetIntOption((int)Option.RUNNING, 0);
        }

        /// <summary>
        /// Reset the acquisition clock counter to 0
        /// (<see cref="AcquisitionClockHz"/>).
        /// </summary>
        public void ResetFrameClock()
        {
            SetIntOption((int)Option.RESETACQCOUNTER, 1);
        }

        /// <summary>
        /// Indicates whether or not data acquisition is running.
        /// </summary>
        public bool Running
        {
            get
            {
                return GetIntOption((int)Option.RUNNING) > 0;
            }
        }

        /// <summary>
        /// The address of the host hardware within the acquisition computer.
        /// Determines the synchronization role of the hardware in multi-host
        /// systems.
        /// </summary>
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

        /// <summary>
        /// Number of bytes read during each driver access to the high-bandwidth
        /// read channel using <see cref="Context.ReadFrame"/>. This option
        /// allows control over a fundamental trade-off between closed-loop
        /// response time and overall bandwidth. The minimum (default) value will
        /// provide the lowest response latency. Larger values will reduce syscall
        /// frequency and may improve processing performance for high-bandwidth
        /// data sources. This minimum size of this option is determined by
        /// <see cref="Context.MaxReadFrameSize"/>.
        /// </summary>
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

        /// <summary>
        /// Number of bytes pre-allocated for calls to <see cref="Write{T}(uint, T)"/>,
        /// <see cref="Write{T}(uint, T[])"/>, and <see cref="Write(uint, IntPtr, int)"/>.
        /// A larger size will reduce the average amount of dynamic memory
        /// allocation system calls but increase the cost of each of those calls.
        /// The minimum size of this option is determined by
        /// <see cref="Context.MaxWriteFrameSize"/>.
        /// </summary>
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

        /// <summary>
        /// Read the value of a configuration register from a specific device
        /// within the current <see cref="oni.Context.DeviceTable"/>. This can
        /// be used to verify the success of calls to <see cref="Context.ReadRegister(uint, uint)"/>
        /// or to obtain state information about devices managed by the current
        /// acquisition context. Register specifications (addresses, read- and
        /// write-access, and descriptions are provided on the ONI-device datasheet).
        /// </summary>
        /// <param name="dev_index">Fully-qualified device index within the
        /// <see cref="Context.DeviceTable"/></param>
        /// <param name="register_address">Address of register to be read.</param>
        /// <returns>Value of the register.</returns>
        /// <exception cref="ONIException"> Thrown if the device and/or register
        /// address in invalid or if register is write only.</exception>
        public uint ReadRegister(uint dev_index, uint register_address)
        {
            uint value;
            int rc = NativeMethods.oni_read_reg(ctx, dev_index, register_address, (IntPtr)(&value));
            if (rc != 0) { throw new ONIException(rc); }
            return value;
        }

        /// <summary>
        /// Change the value of a configuration register from specific devices
        /// within the current <see cref="Context.DeviceTable"/>.
        /// Register specifications (addresses, read- and write-access,
        /// acceptable values, and descriptions are provided on the ONI device
        /// datasheet).
        /// </summary>
        /// <param name="dev_index">Fully-qualified device index within the
        /// <see cref="Context.DeviceTable"/></param>
        /// <param name="register_address">Address of register to write to.</param>
        /// <param name="value">Value to write to the register.</param>
        /// <exception cref="ONIException"> Thrown if the device and/or register
        /// address in invalid or if register is read only.</exception>
        public void WriteRegister(uint dev_index, uint register_address, uint value)
        {
            int rc = NativeMethods.oni_write_reg(ctx, dev_index, register_address, value);
            if (rc != 0) { throw new ONIException(rc); }
        }

        /// <summary>
        /// Read a device data frame from the high-bandwidth data input channel.
        /// This call will block until either enough data available on the stream
        /// to construct an underlying block buffer (see <see cref="Context.BlockReadSize"/>).
        /// This function is zero-copy.
        /// </summary>
        /// <returns>A device data frame.</returns>
        /// <exception cref="ONIException"> Thrown if there is an error reading
        /// a frame.</exception>
        public Frame ReadFrame()
        {
            int rc = NativeMethods.oni_read_frame(ctx, out IntPtr data);
            if (rc < 0) { throw new ONIException(rc); }
            return new Frame(data);
        }

        /// <summary>
        /// Write a single value to a particular device within the
        /// <see cref="Context.DeviceTable"/> using the high-bandwidth
        /// output channel.
        /// </summary>
        /// <typeparam name="T">Type of the value to be written. Must be an
        /// unmanaged type.</typeparam>
        /// <param name="dev_index">Fully-qualified device index within the
        /// <see cref="oni.Context.DeviceTable"/></param>
        /// <param name="value">Value to write to the device.</param>
        /// <exception cref="ONIException">Throw if data is an invalid size
        /// or the selected device does not accept write data.</exception>
        public void Write<T>(uint dev_index, T value) where T : unmanaged
        {
            Write(dev_index, (IntPtr)(&value), sizeof(T));
        }

        /// <summary>
        /// Write an array to a particular device within the
        /// <see cref="Context.DeviceTable"/> using the high-bandwidth
        /// output channel.
        /// </summary>
        /// <typeparam name="T">Type of the value to be written. Must be an
        /// unmanaged type.</typeparam>
        /// <param name="dev_index">Fully-qualified device index within the
        /// <see cref="Context.DeviceTable"/></param>
        /// <param name="data">Data array to write to the device.</param>
        /// <exception cref="ONIException">Throw if data array
        /// is an invalid size or the selected device does not accept write
        /// data.</exception>
        public void Write<T>(uint dev_index, T[] data) where T : unmanaged
        {
            // NB: Buffer.ByteLength only accepts arrays of primitive types
            var num_bytes = data.Length * sizeof(T);
            IntPtr frame;
            int rc;

            // NB: oni_create_frame copies this data
            fixed (T* p = data)
            {
                rc = NativeMethods.oni_create_frame(ctx, out frame, dev_index, (IntPtr)p, (nuint)num_bytes);
                if (rc < 0) { throw new ONIException(rc); }
            }

            try
            {
                rc = NativeMethods.oni_write_frame(ctx, frame);
                if (rc < 0) { throw new ONIException(rc); }
            }
            finally
            {
                NativeMethods.oni_destroy_frame(frame);
            }
        }

        /// <summary>
        /// Write data at an <see cref="IntPtr"/> to a particular device within the
        /// <see cref="Context.DeviceTable"/> using the high-bandwidth
        /// output channel.
        /// </summary>
        /// <param name="dev_index">Fully-qualified device index within the
        /// <see cref="Context.DeviceTable"/></param>
        /// <param name="data">Pointer to data to write to the device.</param>
        /// <param name="data_size">Size of data pointed to by <paramref name="data"/>
        /// in bytes.</param>
        /// <exception cref="ONIException">Throw if <paramref name="data_size"/>
        /// is an invalid size or the selected device does not accept write
        /// data.</exception>
        public void Write(uint dev_index, IntPtr data, int data_size)
        {
            int rc = NativeMethods.oni_create_frame(ctx, out IntPtr frame, dev_index, data, (nuint)data_size);
            if (rc < 0) { throw new ONIException(rc); }

            try
            {
                rc = NativeMethods.oni_write_frame(ctx, frame);
                if (rc < 0) { throw new ONIException(rc); }
            }
            finally
            {
                NativeMethods.oni_destroy_frame(frame);
            }
        }

        /// <summary>
        /// Dispose this <see cref="Context">Context</see>.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Dispose this <see cref="Context">Context</see>. This override is required
        /// by IDisposable.
        /// </summary>
        /// <param name="disposing"></param>
#if NET7_0_OR_GREATER
        protected virtual void Dispose(bool disposing)
#else
        [SecurityPermission(SecurityAction.Demand, UnmanagedCode = true)]
        protected virtual void Dispose(bool disposing)
#endif
        {

            if (ctx != null && !ctx.IsInvalid)
            {
                ctx.Dispose();
            }
        }
    }
}
