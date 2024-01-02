using oni;
using System.Linq;
using System.Reflection;
using System.Text;

namespace ClrOniRepl
{
    static internal class Helpers
    {

        public static string HubTableString(Context ctx)
        {
            var hubs = ctx.DeviceTable.Select(d => ctx.GetHub(d.Value.Address)).Distinct();
            var builder = new StringBuilder();

            foreach (var hub in hubs)
            {
                builder.AppendLine(string.Format("\tHub {0}) {1}",
                                   hub.Address,
                                   hub.Description));
            }

            return builder.ToString();
        }

        public static string DeviceTableString(Context ctx)
        {
            var builder = new StringBuilder();

            foreach (var dev in ctx.DeviceTable.Values)
            {
                builder.AppendLine(string.Format("\t{0}) ID: {1}, Read size: {2}, Write size: {3}, Hub: {4}",
                                  dev.Address,
                                  dev.ID,
                                  dev.ReadSize,
                                  dev.WriteSize,
                                  ctx.GetHub(dev.Address).Description));
            }

            return builder.ToString();
        }
    }
}
