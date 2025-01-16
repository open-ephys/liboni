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
            var hubs = ctx.DeviceTable.Where(d => d.Value.ID != 0)
                .Select(d => ctx
                .GetHub(d.Value.Address))
                .GroupBy(h => h.Address)
                .Select(g => g
                .First());

            var builder = new StringBuilder();

            foreach (var hub in hubs )
            {
                builder.AppendLine($"\tHub { hub.Address}) {hub.Description}");
            }

            return builder.ToString();
        }

        public static string DeviceTableString(Context ctx)
        {
            var builder = new StringBuilder();

            foreach (var dev in ctx.DeviceTable.Values)
            {
                builder.AppendLine($"\t{dev.Address}) ID: {dev.ID}, Read size: {dev.ReadSize}, Write size: {dev.WriteSize}, {dev.Description}");
            }

            return builder.ToString();
        }
    }
}
