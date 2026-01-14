using oni;
using System.Runtime.InteropServices;
using CommandLine;
using CommandLine.Text;

namespace ClrOniRepl
{
    class DataProcessor(Context context, bool display = false, ulong displayEvery = 1000)
    {
        private readonly Context context = context;

        public volatile bool Quit = false;

        public bool Display { get; set; } = display;

        public ulong DisplayEvery { get; set; } = displayEvery;

        public void CaptureData()
        {
            ulong counter = 0;
            // 
            // int rc = 0;
            // while (rc == 0 && !quit)
            // {
            //     ctx.Write(8, (uint)counter);
            //     Thread.SpinWait(100000); // Sleep(1);
            //     counter++;
            // }

            int rc = 0;
            while (rc == 0 && !Quit)
            {
                try
                {
                    var frame = context.ReadFrame();

                    if (counter++ % DisplayEvery == 0)
                    {
                        if (Display)
                        {
                            var dat = frame.GetData<ushort>();
                            var idx = frame.DeviceAddress;
                            Console.WriteLine("\t[{0}] Dev: {1} ({2})", frame.Clock, idx, context.DeviceTable[idx].Description);
                            Console.WriteLine("\t[{0}]", string.Join(", ", dat));
                        }
                    }

                }
                catch (ONIException ex)
                {
                    Console.Error.WriteLine("Data processor failed with the following error: " + ex.ToString());
                    break;
                }
            }
        }
    }

    class Repl
    {
        public class Options
        {
            [Value(0, MetaName = "Driver", Required = true, HelpText = "Device driver string (e.g. riffa, test, etc.).")]
            public string Driver { get; set; }

            [Value(1, MetaName = "Slot", Default = -1, Required = false, HelpText = "Slot index (e.g. 0, 1, 2, etc.). If not specified, the driver-defined default index will be attempted.")]
            public int Slot { get; set; }

            [Option('d', Default = false, Required = false, HelpText = "If specified, frames produced by the oni hardware will be printed to the console.")]
            public bool DisplayFrames { get; set; }

            [Option('D', Default = 0.1, Required = false, HelpText = "The percent of frames printed to the console if frames are displayed. Percent should be a value in (0, 100.0].")]
            public double DisplayPercentage { get; set; }

            [Option("rbytes", Default = 2048, Required = false, HelpText = "Set block read size in bytes.")]
            public int BlockReadSize { get; set; }

            [Option("wbytes", Default = 2048, Required = false, HelpText = "Set write preallocation size in bytes.")]
            public int BlockWriteSize { get; set; }
        }

        static void Main(string[] args)
        {

            Parser.Default.ParseArguments<Options>(args).WithParsed(o =>
            {
                // Introduction
                Console.WriteLine(HeadingInfo.Default);
                Console.WriteLine("Using liboni version: " + NativeMethods.LibraryVersion);

                //bool running = true;

                try
                {
                    using (var ctx = new Context(o.Driver, o.Slot))
                    {
                        Console.WriteLine(string.Format("Loaded driver: {0} v{1}.{2}.{3}-{4}",
                            ctx.Driver.Name, ctx.Driver.Major, ctx.Driver.Minor, ctx.Driver.Patch, ctx.Driver.PreRelease));

                        // Show device table
                        Console.WriteLine("Found the following devices:");
                        Console.Write(Helpers.DeviceTableString(ctx));

                        // See how big max frames are
                        Console.WriteLine("Max read frame size: " + ctx.MaxReadFrameSize);

                        // See how big max frames are
                        Console.WriteLine("Max write frame size: " + ctx.MaxWriteFrameSize);

                        // See the hardware clock
                        Console.WriteLine("System clock frequency: " + ctx.SystemClockHz);

                        // See the hardware address
                        Console.WriteLine("Hardware address: " + ctx.HardwareAddress);

                        // Set read pre-allocation size
                        ctx.BlockReadSize = o.BlockReadSize;
                        ctx.BlockWriteSize = o.BlockWriteSize;

                        // State acquisition and reset acquisition clock counter
                        ctx.Start(true);

                        // Start processor in background
                        if (o.DisplayPercentage <= 0 || o.DisplayPercentage > 100.0) throw new ArgumentException("Invalid frame display rate requested.");
                        var displayEvery = (ulong)(100.0 / o.DisplayPercentage);
                        var processor = new DataProcessor(ctx, o.DisplayFrames, (ulong)(100.0 / o.DisplayPercentage));
                        var procThread = new Thread(new ThreadStart(processor.CaptureData));
                        procThread.Start();

                        int c = '-';

                        while (c != 'q')
                        {
                            try
                            {
                                Console.WriteLine("Enter a command and press enter:");
                                Console.WriteLine("\ta - reset the acquisition clock counter");
                                Console.WriteLine("\td - toggle frame display");
                                Console.WriteLine("\tD - change the percent of frames displayed");
                                Console.WriteLine("\tH - print all hubs in the current configuration");
                                Console.WriteLine("\tp - toggle hardware running state");
                                Console.WriteLine("\tr - enter register command");
                                Console.WriteLine("\tt - print device table");
                                Console.WriteLine("\tx - issue a hardware reset");
                                Console.WriteLine("\tq - quit");
                                Console.Write(">>> ");

                                c = Console.ReadLine()[0];

                                switch (c)
                                {
                                    case 'p':
                                        if (!ctx.Running)
                                        {
                                            ctx.Start();
                                            Console.WriteLine("Acquisition started.");
                                        }
                                        else
                                        {
                                            ctx.Stop();
                                            Console.WriteLine("Acquisition paused.");
                                        }
                                        break;

                                    case 'd':
                                        processor.Display = !processor.Display;
                                        break;

                                    case 'D':
                                        {
                                            var rate = 100.0 * 1.0 / processor.DisplayEvery;
                                            Console.WriteLine(string.Format("Enter the percent of frames to display. 100.0 will display every frame. The current rate is {0}", rate));
                                            Console.Write(">>> ");
                                            var tmp = Console.ReadLine();
                                            var newRate = double.TryParse(tmp, out double r) ? r : throw new ArgumentException("Invalid arguements.");
                                            if (newRate <= 0.0 || newRate > 100.0) throw new ArgumentException("Display rate must be between 0 and 100%.");
                                            processor.DisplayEvery = (ulong)(100.0 / newRate);
                                            break;

                                        }
                                    case 'H':
                                        Console.WriteLine(Helpers.HubTableString(ctx));
                                        break;

                                    case 't':
                                        Console.Write(Helpers.DeviceTableString(ctx));
                                        break;

                                    case 'r':
                                        {
                                            Console.WriteLine("Enter dev_idx reg_addr");
                                            Console.Write(">>> ");
                                            var tmp = Console.ReadLine().Trim().Split(' ').ToList().Select(x => uint.TryParse(x, out uint r) ? r : throw new ArgumentException("Invalid arguements."));
                                            if (tmp.Count() != 2) throw new ArgumentException("Invalid number of arguements.");
                                            Console.WriteLine(ctx.ReadRegister(tmp.ElementAt(0), tmp.ElementAt(1)));

                                            break;
                                        }
                                    case 'w':
                                        {
                                            Console.WriteLine("Enter dev_idx reg_addr reg_value");
                                            Console.Write(">>> ");
                                            var tmp = Console.ReadLine().Trim().Split(' ').ToList().Select(x => uint.TryParse(x, out uint r) ? r : throw new ArgumentException("Invalid arguements."));
                                            if (tmp.Count() != 3) throw new ArgumentException("Invalid number of arguements.");
                                            ctx.WriteRegister(tmp.ElementAt(0), tmp.ElementAt(1), tmp.ElementAt(2));
                                            break;
                                        }
                                    case 'a':
                                        ctx.ResetFrameClock();
                                        break;

                                    case 'x':
                                        processor.Quit = true;
                                        procThread.Join(200);
                                        ctx.Refresh();
                                        Console.Write(Helpers.DeviceTableString(ctx));
                                        ctx.Start(true);
                                        procThread.Start();
                                        break;

                                    default:
                                        continue;
                                }
                            }

                            catch (Exception ex) when (ex is ArgumentException || ex is ONIException)
                            {
                                Console.Error.WriteLine(ex.Message);
                            }
                        }

                        // Join data and signal threads
                        processor.Quit = true;
                        procThread.Join(200);

                    }

                }
                catch (ONIException ex)
                {
                    Console.Error.WriteLine("Host failed with the following error: "
                                            + ex.ToString());
                    Console.Error.WriteLine("Current errno: "
                                            + Marshal.GetLastWin32Error());
                }
            });
        }
    }
}
