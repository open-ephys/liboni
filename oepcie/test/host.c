#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../liboepcie/oepcie.h"

volatile oe_ctx ctx = NULL;
oe_device_t *devices = NULL;
volatile int quit = 0;
volatile int display = 0;
int running = 1;

int parse_reg_cmd(const char *cmd, long *values)
{
    //printf("Parsing '%s':\n", cmd);
    char *end;
    int k = 0;
    for (long i = strtol(cmd, &end, 10);
         cmd != end;
         i = strtol(cmd, &end, 10))
    {

        //printf("'%.*s' -> ", (int)(end-p), p);
        cmd = end;
        if (errno == ERANGE){ return -1; }

        values[k++] = i;
        if (k == 3)
            break;
    }

    if (k < 3)
        return -1;

    return 0;
}

void *data_loop(void *vargp)
{
    int rc = 0;
    while (rc == 0 && !quit)  {

        oe_frame_t *frame;
        rc = oe_read_frame(ctx, &frame);

        if (display) {

            printf("-- Read frame --\n");
            printf("\tSample: %" PRIu64 "\n", frame->clock);

            int i;
            for (i = 0; i < frame->num_dev; i++) {

                oe_device_t this_dev = devices[frame->dev_idxs[i]];

                printf("\tDev: %d (%s)\n",
                       frame->dev_idxs[i],
                       oe_device_str(this_dev.id));

                uint8_t *data = (uint8_t *)(frame->data + frame->dev_offs[i]);

                size_t data_sz = this_dev.read_size;

                int i;
                printf("\tData: [");
                for (i = 0; i < data_sz; i += 2)
                    printf("%" PRId16 " ", *(uint16_t *)(data + i));
                printf("]\n");
            }
        }

        oe_destroy_frame(frame);
    }


	return NULL;
}

int main()
{
    // Generate context
    ctx = oe_create_ctx();
    if (!ctx) exit(EXIT_FAILURE);

    // Set stream paths
    const char *config_path = "/tmp/rat128_config";
    const char *sig_path = "/tmp/rat128_signal";
    const char *data_path = "/tmp/rat128_read";
    //const char *config_path = "/dev/xillybus_cmd_mem_32";
    //const char *sig_path = "/dev/xillybus_async_read_8";
    //const char *data_path = "/dev/xillybus_data_read_32";

    oe_set_opt(ctx, OE_CONFIGSTREAMPATH, config_path, strlen(config_path) + 1);
    oe_set_opt(ctx, OE_SIGNALSTREAMPATH, sig_path, strlen(sig_path) + 1);
    oe_set_opt(ctx, OE_READSTREAMPATH, data_path, strlen(data_path) + 1);

    // Initialize context and discover hardware
    assert(oe_init_ctx(ctx) == 0);

    // Examine device map
    oe_size_t num_devs = 0;
    size_t num_devs_sz = sizeof(num_devs);
    oe_get_opt(ctx, OE_NUMDEVICES, &num_devs, &num_devs_sz);

    // Get the device map
    size_t devices_sz = sizeof(oe_device_t) * num_devs;
    devices = (oe_device_t *)realloc(devices, devices_sz);
    if (devices == NULL) { exit(EXIT_FAILURE); }
    oe_get_opt(ctx, OE_DEVICEMAP, devices, &devices_sz);

    // Show device map
    printf("Found the following devices:\n");
    int dev_idx;
    for (dev_idx = 0; dev_idx < num_devs; dev_idx++) {

        const char *dev_str = oe_device_str(devices[dev_idx].id);

        printf("\t%d) ID: %d (%s), Read size:%u\n",
               dev_idx,
               devices[dev_idx].id,
               dev_str,
               devices[dev_idx].read_size);
    }

    oe_size_t frame_size = 0;
    size_t frame_size_sz = sizeof(frame_size);
    oe_get_opt(ctx, OE_READFRAMESIZE, &frame_size, &frame_size_sz);
    printf("Frame size: %u bytes\n", frame_size);

    // Try to write to base clock freq, which is write only
    oe_reg_val_t base_hz = 10e6;
    int rc = oe_set_opt(ctx, OE_SYSCLKHZ, &base_hz, sizeof(oe_reg_val_t));
    assert(rc == OE_EREADONLY && "Successful write to read-only register.");

    size_t clk_val_sz = sizeof(base_hz);
    rc = oe_get_opt(ctx, OE_SYSCLKHZ, &base_hz, &clk_val_sz);
    if (rc) { printf("Error: %s\n", oe_error_str(rc)); }
    assert(!rc && "Register read failure.");

    // Start acquisition
    oe_reg_val_t run = 1;
    rc = oe_set_opt(ctx, OE_RUNNING, &run, sizeof(run));
    if (rc) { printf("Error: %s\n", oe_error_str(rc)); }

    // Generate data thread and continue here config/signal handling in parallel
    pthread_t tid;
    pthread_create(&tid, NULL, data_loop, NULL);

    // Read stdin to start (s) or pause (p)
    int c = 's';
    while (c != 'q') {

        printf("Enter a command and press enter:\n");
        printf("\td - toggle display\n");
        printf("\tp - toggle stream pause\n");
        printf("\tr - enter register command\n");
        printf("\tq - quit\n");
        printf(">>> ");

        char *cmd = NULL;
        size_t cmd_len = 0;
        rc = getline(&cmd, &cmd_len, stdin);
        if (rc == -1) { printf("Error: bad command\n"); continue;}
        c = cmd[0];
        free(cmd);

        if (c == 'p') {
            running = (running == 1) ? 0 : 1;
            oe_reg_val_t run = running;
            rc = oe_set_opt(ctx, OE_RUNNING, &run, sizeof(run));
            if (rc) {
                printf("Error: %s\n", oe_error_str(rc));
            }
            printf("Paused\n");
        } else if (c == 'd') {
            display = (display == 0) ? 1 : 0;
        } else if (c == 'r') {

            printf("Enter dev_idx reg_addr reg_val\n");
            printf(">>> ");

            // Read the command
            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) { printf("Error: bad command\n"); continue;}

            // Parse the command string
            long values[3];
            rc = parse_reg_cmd(buf, values);
            if (rc == -1) { printf("Error: bad command\n"); continue;}
            free(buf);

            size_t dev_idx = (size_t)values[0];
            oe_reg_addr_t addr = (oe_reg_addr_t)values[1];
            oe_reg_val_t val = (oe_reg_val_t)values[2];

            oe_write_reg(ctx, dev_idx, addr, val);

        }

    }

    // Join data and signal threads
    quit = 1;
    pthread_join(tid, NULL);

    // Reset the hardware
    oe_reg_val_t reset = 1;
    rc = oe_set_opt(ctx, OE_RESET, &reset, sizeof(reset));
    if (rc) { printf("Error: %s\n", oe_error_str(rc)); }

    assert(!rc && "Register write failure.");

    // Free dynamic stuff
    oe_destroy_ctx(ctx);
    free(devices);

    return 0;
}
