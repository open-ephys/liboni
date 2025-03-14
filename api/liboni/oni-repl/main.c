#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>

#include "oni.h"
#include "onix.h"
#include "oelogo.h"
#include "ketopt.h"

// Version macros for compile-time program version
// NB: see https://semver.org/
#define ONI_REPL_VERSION_MAJOR 1
#define ONI_REPL_VERSION_MINOR 0
#define ONI_REPL_VERSION_PATCH 4

#define DEFAULT_BLK_READ_BYTES 2048
#define DEFAULT_BLK_WRITE_BYTES 2048

// Turn on simple feedback loop for real-time testing?
// #define FEEDBACKLOOP

// Turn on RT optimization
#define RT

// Windows- and UNIX-specific includes etc
#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "liboni")
#include <stdio.h>
#include <stdlib.h>
#else
#include <unistd.h>
#include <pthread.h>
#define Sleep(x) usleep((x)*1000)
#endif

// Options
volatile int display = 0;
unsigned int num_frames_to_display = 0;
int display_every_n = 1000;
unsigned int device_idx_filter = 0;
int device_idx_filter_en = 0;
int dump = 0;
char *print_fmt = "%04X ";

// Global state
volatile int quit = 0;
volatile oni_ctx ctx = NULL;
oni_size_t num_devs = 0;
oni_device_t *devices = NULL;
int running = 1;
char *dump_path;
FILE **dump_files;

#ifdef _WIN32
HANDLE read_thread;
HANDLE write_thread;
#else
pthread_t read_tid;
pthread_t write_tid;
#endif

// Parser for reading and writing device registers
int parse_reg_cmd(const char *cmd, long *values, int len)
{
    char *end;
    int k = 0;
    for (long i = strtol(cmd, &end, 10);
         cmd != end;
         i = strtol(cmd, &end, 10))
    {
        cmd = end;
        if (errno == ERANGE){ return -1; }

        values[k++] = i;
        if (k == len)
            break;
    }

    if (k < len)
        return -1;

    return 0;
}

// Write register initialization file
int write_reg_file(FILE *file) {

    char buf[1000];

    while (fgets(buf, 1000, file) != NULL) {

        // Parse the command string
        long values[3];
        int rc = parse_reg_cmd(buf, values, 3);

        if (rc == -1) {
            printf("Error: %s is an invalid line\n", buf);
            continue;
        }

        size_t dev_idx = (size_t)values[0];
        oni_size_t addr = (oni_size_t)values[1];
        oni_size_t val = (oni_size_t)values[2];

        rc = oni_write_reg(ctx, dev_idx, addr, val);
        if (rc) printf("%s\n", oni_error_str(rc));
    }

    return 0;
}

// Simple & slow device lookup
int find_dev(oni_dev_idx_t idx)
{
    for (size_t i = 0; i < num_devs; i++)
        if (devices[i].idx == idx)
            return i;

    return -1;
}

int16_t last_sample = 32767;
uint32_t out_count = 0;

#ifdef _WIN32
DWORD WINAPI read_loop(LPVOID lpParam)
{
#else
void *read_loop(void *vargp)
{
    (void)vargp;
#endif

    unsigned long counter = 0;
    unsigned long print_count = 0;
    unsigned long this_cnt = 0;

#ifdef FEEDBACKLOOP
    // Pre-allocate write frame
    oni_frame_t *w_frame = NULL;
    oni_create_frame(ctx, &w_frame, 8, 4);
#endif

    while (!quit)  {

        int rc = 0;
        oni_frame_t *frame = NULL;
        rc = oni_read_frame(ctx, &frame);
        //printf("frame %d\n", frame->dev_idx);
        if (rc < 0) {
            printf("Error: %s\n", oni_error_str(rc));
            quit = 1;
            break;
        }

        int i = find_dev(frame->dev_idx);
        if (i == -1) goto next;

        if (dump && devices[i].id != ONIX_NULL) {
            fwrite(frame->data, 1, frame->data_sz, dump_files[i]);
        }

        if (display
            && (display_every_n <= 1 || counter % display_every_n == 0)
            && (num_frames_to_display == 0 || print_count < num_frames_to_display)
            && (!device_idx_filter_en || devices[i].idx == device_idx_filter)
            ) {

            oni_device_t this_dev = devices[i];

            this_cnt++;
            printf("\t[%lu] Dev: %u (%s) \n",
                frame->time,
                frame->dev_idx,
                onix_device_str(this_dev.id));

            size_t i;
            printf("\tData: [");
            for (i = 0; i < frame->data_sz; i += 2)
                printf(print_fmt, *(uint16_t *)(frame->data + i));
            printf("]\n");

            print_count++;
        }

#ifdef FEEDBACKLOOP
        // Feedback loop test
         if (frame->dev_idx == 7) {

            int16_t sample = *(int16_t *)(frame->data + 10);

            if (sample - last_sample > 500) {

                memcpy(w_frame->data, &out_count, 4);
                out_count++;

                int rc = oni_write_frame(ctx, w_frame);
                if (rc < 0) { printf("Error: %s\n", oni_error_str(rc)); }

            }

            last_sample = sample;
        }
#endif


next:
        counter++;
        oni_destroy_frame(frame);
    }

#ifdef FEEDBACKLOOP
    oni_destroy_frame(w_frame);
#endif

    return NULL;
}

//#ifdef _WIN32
//DWORD WINAPI write_loop(LPVOID lpParam)
//#else
//void *write_loop(void *vargp)
//#endif
//{
//    // Pre-allocate write frame
//    // TODO: hardcoded dev_idx not good
//    oni_frame_t *w_frame = NULL;
//    int rc = oni_create_frame(ctx, &w_frame, 6, &out_count, sizeof(out_count));
//    if (rc < 0) {
//        printf("Error: %s\n", oni_error_str(rc));
//        goto error;
//    }
//
//    // Loop count
//    // uint32_t count = 0;
//
//    // Cycle through writable devices and write counter to their data
//    while (!quit) {
//
//
//        int rc = oni_write_frame(ctx, w_frame);
//        if (rc < 0) {
//            printf("Error: %s\n", oni_error_str(rc));
//            goto error;
//        }
//
//        memcpy(w_frame->data, &out_count, 4);
//        out_count++;
//
//        // count++;
//
//#ifdef _WIN32
//        Sleep(1);
//#else
//        usleep(1000);
//#endif
//    }
//
//error:
//    oni_destroy_frame(w_frame);
//    return NULL;
//}

static void start_threads()
{
    quit = 0;

    // Generate data read_thread and continue here config/signal handling in parallel
#ifdef _WIN32
    DWORD read_tid;
    read_thread = CreateThread(NULL, 0, read_loop, NULL, 0, &read_tid);

    //DWORD write_tid;
    //write_thread = CreateThread(NULL, 0, write_loop, NULL, 0, &write_tid);

#ifdef RT
    if (!SetThreadPriority(read_thread, THREAD_PRIORITY_TIME_CRITICAL))
        printf("Unable to set read thread priority.\n");
    //if (!SetThreadPriority(write_thread, THREAD_PRIORITY_HIGHEST))
    //    printf("Unable to set read thread priority.\n");
#endif

#else
    pthread_create(&read_tid, NULL, read_loop, NULL);
    //pthread_create(&write_tid, NULL, write_loop, NULL);
#endif
}

static void stop_threads()
{
    // Join data and signal threads
    quit = 1;

#ifdef _WIN32

    WaitForSingleObject(read_thread, 500); // INFINITE);
    CloseHandle(read_thread);

    //WaitForSingleObject(write_thread, 200);
    //CloseHandle(write_thread);
#else
    if (running)
        pthread_join(read_tid, NULL);
    //pthread_join(write_tid, NULL);
#endif

    oni_size_t run = 0;
    int rc = oni_set_opt(ctx, ONI_OPT_RUNNING, &run, sizeof(run));
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
}

void print_version()
{
    int major;
    int minor;
    int patch;
    oni_version(&major, &minor, &patch);
    printf("oni-repl v%d.%d.%d (liboni v%d.%d.%d)\n",
            ONI_REPL_VERSION_MAJOR, ONI_REPL_VERSION_MINOR,
            ONI_REPL_VERSION_PATCH, major, minor, patch);
}

void print_dev_table(oni_device_t *devices, size_t num_devs)
{
    // Show device table
    printf("   +--------------------+-------+-------+-------+-------+---------------------\n");
    printf("   |        \t\t|  \t|Firm.\t|Read\t|Wrt. \t|     \n");
    printf("   |Dev. idx\t\t|ID\t|ver. \t|size\t|size \t|Desc.\n");
    printf("   +--------------------+-------+-------+-------+-------+---------------------\n");

    size_t dev_idx;
    for (dev_idx = 0; dev_idx < num_devs; dev_idx++) {

        const char *dev_str = onix_device_str(devices[dev_idx].id);

        printf("%02zd |%05u: 0x%02x.0x%02x\t|%d\t|%d\t|%u\t|%u\t|%s\n",
               dev_idx,
               devices[dev_idx].idx,
               (uint8_t)(devices[dev_idx].idx >> 8),
               (uint8_t)devices[dev_idx].idx,
               devices[dev_idx].id,
               devices[dev_idx].version,
               devices[dev_idx].read_size,
               devices[dev_idx].write_size,
               dev_str);
    }

    printf("   +--------------------+-------+-------+-------+-------+---------------------\n");
}

void print_hub_info(size_t hub_idx)
{
    oni_reg_val_t hub_hw_id = 0;
    int rc = oni_read_reg(
        ctx, hub_idx + ONIX_HUB_DEV_IDX, ONIX_HUB_HARDWAREID, &hub_hw_id);
    printf("Hub hardware ID: ");
    rc ? printf("%s\n", oni_error_str(rc)) :
         printf("%u, %s\n", hub_hw_id, onix_hub_str(hub_hw_id));

    oni_reg_val_t hub_hw_rev = 0;
    rc = oni_read_reg(
        ctx, hub_idx + ONIX_HUB_DEV_IDX, ONIX_HUB_HARDWAREREV, &hub_hw_rev);
    printf("Hub hardware revision: ");
    rc ? printf("%s\n", oni_error_str(rc)) :
         printf("%u.%u\n", (hub_hw_rev & 0xFF00) >> 8, hub_hw_rev & 0xFF);

    oni_reg_val_t hub_firm_ver = 0;
    rc = oni_read_reg(
        ctx, hub_idx + ONIX_HUB_DEV_IDX, ONIX_HUB_FIRMWAREVER, &hub_firm_ver);
    printf("Hub firmware version: ");
    rc ? printf("%s\n", oni_error_str(rc)) :
         printf("%u.%u\n", (hub_firm_ver & 0xFF00) >> 8, hub_firm_ver & 0xFF);

    oni_reg_val_t hub_clk_hz = 0;
    rc = oni_read_reg(
        ctx, hub_idx + ONIX_HUB_DEV_IDX, ONIX_HUB_CLKRATEHZ, &hub_clk_hz);
    printf("Hub clock frequency (Hz): ");
    rc ? printf("%s\n", oni_error_str(rc)) : printf("%u\n", hub_clk_hz);

    oni_reg_val_t hub_delay_ns = 0;
    rc = oni_read_reg(
        ctx, hub_idx + ONIX_HUB_DEV_IDX, ONIX_HUB_DELAYNS, &hub_delay_ns);
    printf("Hub transmission delay (ns): ");
    rc ? printf("%s\n", oni_error_str(rc)) : printf("%u\n", hub_delay_ns);
}

void update_dev_table() 
{
    // Examine device table
    size_t num_devs_sz = sizeof(num_devs);
    oni_get_opt(ctx, ONI_OPT_NUMDEVICES, &num_devs, &num_devs_sz);

    // Get the device table
    size_t devices_sz = sizeof(oni_device_t) * num_devs;
    devices = (oni_device_t *)realloc(devices, devices_sz);
    if (devices == NULL) {
        exit(EXIT_FAILURE);
    }
    oni_get_opt(ctx, ONI_OPT_DEVICETABLE, devices, &devices_sz);
}

oni_size_t get_i2c_reg_address(unsigned int i2c_dev_addr,
                               unsigned int i2c_reg_addr,
                               unsigned int address_16bit)
{
    oni_size_t address = (i2c_reg_addr << 7) | (i2c_dev_addr & 0x7F);
    address |= address_16bit ? 0x80000000 : 0;
    return address;
}

int main(int argc, char *argv[])
{

    oni_size_t block_read_size = DEFAULT_BLK_READ_BYTES;
    oni_size_t block_write_size = DEFAULT_BLK_WRITE_BYTES;
    int host_idx = -1;
    char *driver;
    char *reg_path = NULL;

    static ko_longopt_t longopts[] = {{"help", ko_no_argument, 301},
                                      {"rbytes", ko_required_argument, 302},
                                      {"wbytes", ko_required_argument, 303},
                                      {"dformat", ko_required_argument, 304},
                                      {"dumppath", ko_required_argument, 305},
                                      {"regpath", ko_required_argument, 306},
                                      {"version", ko_no_argument, 307},
                                      {NULL, 0, 0}};
    ketopt_t opt = KETOPT_INIT;
    int c;
    while ((c = ketopt(&opt, argc, argv, 1, "hvdD:n:i:", longopts)) >= 0) {
        if (c == 'h' || c == 301)
            goto usage;
        if (c == 'v' || c == 307) {
            print_version();
            goto exit;
        } else if (c == 'd')
            display = 1;
        else if (c == 'D') {
            float percent = atoi(opt.arg);
            if (percent <= 0 || percent > 100) {
                printf("Error: invalid value for -D. Pick a value within (0.0, 100.0].\n");
                goto usage;
            }
            display_every_n = (int)(100.0 / percent);
        } else if (c == 'n')
            num_frames_to_display = atoi(opt.arg);
        else if (c == 'i') {
            int idx = atoi(opt.arg);
            device_idx_filter_en = idx < 0 ? 0 : 1;
            device_idx_filter = (size_t)idx;
        } else if (c == 301)
            goto usage;
        else if (c == 302)
            block_read_size = atoi(opt.arg);
        else if (c == 303)
            block_write_size = atoi(opt.arg);
        else if (c == 304) {
            if (strcmp(opt.arg, "hex") == 0)
                print_fmt = "%04X ";
            else if (strcmp(opt.arg, "dec") == 0)
                print_fmt = "%hu ";
            else
                goto usage;
        } else if (c == 305) {
            dump = 1;
            dump_path = opt.arg;
        } else if (c == 306) {
            reg_path = opt.arg;
        } else if (c == '?') {
            printf("Unknown option: -%c\n", opt.opt ? opt.opt : ':');
            goto usage;
        }
        else if (c == ':') {
            printf("Missing argument: -%c\n", opt.opt ? opt.opt : ':');
            goto usage;
        }
    }

    if (argc == opt.ind + 1) {
        driver = argv[opt.ind];
    } else if (argc == opt.ind + 2) {
        driver = argv[opt.ind];
        host_idx = atoi(argv[opt.ind + 1]);
    } else {

usage:
        printf("Usage: %s <driver> [slot] [-d] [-D <value>] [-n <value>] [-i <device index>] [--rbytes=<bytes>] [--wbytes=<bytes>] [--dformat=<hex,dec>] [--dumppath=<path>] [-h,--help] [-v,--version]\n\n", argv[0]);

        printf("\t driver \t\tHardware driver to dynamically link (e.g. riffa, ft600, test, etc.)\n");
        printf("\t slot \t\t\tIndex specifying the physical slot occupied by hardware being controlled. If none is provided, the driver-defined default will be used.\n");
        printf("\t -d \t\t\tDisplay frames. If specified, frames produced by the oni hardware will be printed to the console.\n");
        printf("\t -D <percent> \t\tThe percent of frames printed to the console if frames are displayed. Percent should be a value in (0, 100.0].\n");
        printf("\t -n <count> \t\tDisplay at most count frames. Reset only on program restart. Useful for examining the start of the data stream. If set to 0, then this option is ignored.\n");
        printf("\t -i <index> \t\tOnly display frames from device with specified index value.\n");
        printf("\t --rbytes=<bytes> \tSet block read size in bytes. (default: %d bytes)\n", DEFAULT_BLK_READ_BYTES);
        printf("\t --wbytes=<bytes> \tSet write pre-allocation size in bytes. (default: %d bytes)\n", DEFAULT_BLK_WRITE_BYTES);
        printf("\t --dformat=<hex,dec> \tSet the format of frame data printed to the console to hexidecimal (default) or decimal.\n");
        printf("\t --dumppath=<path> \tPath to folder to dump raw device data. \
If not defined, no data will be written. A flat binary file with name <index>_idx-<id>_id-<datetime>.raw will be created for each device in the device table that produces streaming \
data. The bit-wise frame definition in the ONI device datasheet (as required by the ONI spec) will describe frame data is organized in each file.\n");
        printf("\t --regpath=<path> \tPath to a text file containing a table of the form:\n \
                                    \t dev_addr_0 reg_address reg_value\n \
                                    \t dev_addr_1 reg_address reg_value\n \
                                    \t ...\n \
                                    \t dev_addr_n reg_address reg_value\n \
                 \t\tthat is used to bulk write device registers following context initialization.\n");
        printf("\t --help, -h \t\tDisplay this message and exit.\n");
        printf("\t --version, -v \t\tDisplay version information.\n");

exit:
        exit(1);
    }

    printf(oe_logo_med);

#if defined(_WIN32) && defined(RT)
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
        printf("Failed to set thread priority\n");
        exit(EXIT_FAILURE);
    }
#else
    // TODO
#endif

    // Return code
    int rc = ONI_ESUCCESS;

    // Generate context
    ctx = oni_create_ctx(driver);
    if (!ctx) { printf("Failed to create context\n"); exit(EXIT_FAILURE); }

    // Print the driver translator informaiton
    const oni_driver_info_t *di = oni_get_driver_info(ctx);
    if (di->pre_release == NULL) {
        printf("Loaded driver: %s v%d.%d.%d\n",
               di->name,
               di->major,
               di->minor,
               di->patch);
    } else {
        printf("Loaded driver: %s v%d.%d.%d-%s\n",
               di->name,
               di->major,
               di->minor,
               di->patch,
               di->pre_release);
    }

    // Initialize context and discover hardware
    rc = oni_init_ctx(ctx, host_idx);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    assert(rc == 0);

    //// Set ONIX_FLAG0 to turn on pass-through and issue reset
    //oni_reg_val_t val = 1;
    //rc = oni_set_opt(ctx, ONIX_OPT_PASSTHROUGH, &val, sizeof(val));
    //rc = oni_set_opt(ctx, ONI_OPT_RESET, &val, sizeof(val));

    // Examine device table
    update_dev_table();

    // Dump files, if required
    if (dump) {

        // Current time
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        // Make room for dump files
        dump_files = malloc(num_devs * sizeof(FILE *));

        // Open dump files
        for (size_t i = 0; i < num_devs; i++) {
            if (devices[i].id != ONIX_NULL) {
                char *buffer = malloc(500);
                snprintf(buffer,
                         500,
                         "%s\\%s_idx-%zd_id-%d_%d-%02d-%02d-%02d-%02d-%02d.raw",
                         dump_path,
                         "dev",
                         i,
                         devices[i].id,
                         tm.tm_year + 1900,
                         tm.tm_mon + 1,
                         tm.tm_mday,
                         tm.tm_hour,
                         tm.tm_min,
                         tm.tm_sec);
                dump_files[i] = fopen(buffer, "wb");

                if (dump_files[i] == NULL) {
                    printf("Error opening %s: %s\n", buffer, strerror(errno));
                    free(buffer);
                    goto usage;
                }

                free(buffer);
            }
        }
    }

    // Initialize, registers if required
    if (reg_path != NULL) {

        FILE *reg_file = fopen(reg_path, "r");

        if (reg_file != NULL) {
            rc = write_reg_file(reg_file);
        } else {
            printf("Error: failed to open register initialization file.\n");
        }
    }

reset:
    // Show device table
    print_dev_table(devices, num_devs);

    oni_size_t frame_size = 0;
    size_t frame_size_sz = sizeof(frame_size);
    oni_get_opt(ctx, ONI_OPT_MAXREADFRAMESIZE, &frame_size, &frame_size_sz);
    printf("Max. read frame size: %u bytes\n", frame_size);

    oni_get_opt(ctx, ONI_OPT_MAXWRITEFRAMESIZE, &frame_size, &frame_size_sz);
    printf("Max. write frame size: %u bytes\n", frame_size);

    printf("Setting block read size to: %u bytes\n", block_read_size);
    size_t block_size_sz = sizeof(block_read_size);
    rc = oni_set_opt(ctx, ONI_OPT_BLOCKREADSIZE, &block_read_size, block_size_sz);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    assert(!rc && "Failure to set block read size");

    oni_size_t temp;
    oni_get_opt(ctx, ONI_OPT_BLOCKREADSIZE, &temp, &block_size_sz);
    assert(temp == block_read_size && "Setting block read size was unsucessful.");
    printf("Block read size: %u bytes\n", block_read_size);

    printf("Setting write pre-allocation buffer to: %u bytes\n", block_write_size);
    block_size_sz = sizeof(block_write_size);
    rc = oni_set_opt(ctx, ONI_OPT_BLOCKWRITESIZE, &block_write_size, block_size_sz);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }

    oni_get_opt(ctx, ONI_OPT_BLOCKWRITESIZE, &temp, &block_size_sz);
    assert(temp == block_write_size && "Setting block write pre-allocation size was unsucessful.");
    assert(!rc && "Register read failure.");
    printf("Write pre-allocation size: %u bytes\n", block_write_size);

    oni_size_t reg = (oni_size_t)0;
    size_t reg_sz = sizeof(reg);
    rc = oni_get_opt(ctx, ONI_OPT_SYSCLKHZ, &reg, &reg_sz);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    assert(!rc && "Register read failure.");
    printf("System clock rate: %u Hz\n", reg);

    rc = oni_get_opt(ctx, ONI_OPT_ACQCLKHZ, &reg, &reg_sz);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    assert(!rc && "Register read failure.");
    printf("Frame counter clock rate: %u Hz\n", reg);

    // NB: This option is essentially deprecated
    //reg = 42;
    //rc = oni_set_opt(ctx, ONI_OPT_HWADDRESS, &reg, sizeof(oni_size_t));
    //assert(!rc && "Register write failure.");

    //rc = oni_get_opt(ctx, ONI_OPT_HWADDRESS, &reg, &reg_sz);
    //if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    //assert(!rc && "Register read failure.");
    //printf("Hardware address: 0x%08x\n", reg);

    // Start reading and writing threads
    start_threads();
    Sleep(500);

    rc = oni_get_opt(ctx, ONI_OPT_RUNNING, &reg, &reg_sz);
    if (rc) {printf("Error: %s\n", oni_error_str(rc)); }
    assert(!rc && "Register read failure.");
    printf("Hardware run state: %d\n", reg);
    printf("Resetting acquisition clock and starting hardware run simultaneously...\n");

    // Restart acquisition clock counter and start acquisition simultaneously
    reg = 2;
    rc = oni_set_opt(ctx, ONI_OPT_RESETACQCOUNTER, &reg, sizeof(oni_size_t));
    assert(!rc && "Register write failure.");

    rc = oni_get_opt(ctx, ONI_OPT_RUNNING, &reg, &reg_sz);
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
    assert(!rc && "Register read failure.");
    assert(reg == 1 && "ONI_OPT_RUNNING should be 1.");
    printf("Hardware run state: %d\n", reg);

    // Enter REPL
    printf("Some commands can cause hardware malfunction if issued in the wrong order!\n");
    c = 'x';
    while (c != 'q') {

        printf("Enter a command and press enter:\n");
        printf("\td - toggle frame display\n");
        printf("\tD - change the percent of frames displayed\n");
        printf("\ti - set a filter to display frames only from a particular device\n");
        printf("\tt - print current device table\n");
        printf("\tp - toggle running/pause register\n");
        printf("\tr[m|i[x]] - read from device register.\n");
        printf("\t [m]: managed registers.\n");
        printf("\t [i]: i2c raw registers.\n");
        printf("\t [ix]: 16-bit i2c raw registers.\n");
        printf("\tw[m|i[x]] - write to device register.\n");
        printf("\t [m]: managed registers.\n");
        printf("\t [i]: i2c raw registers.\n");
        printf("\t [ix]: 16-bit i2c raw registers.\n");
        printf("\th - get hub information about a device\n");
        printf("\tH - print all hubs in the current configuration\n");
        printf("\ta - reset the acquisition clock counter\n");
        printf("\tx - issue a hardware reset\n");
        printf("\tq - quit\n");
        printf(">>> ");

        char *cmd = NULL;
        size_t cmd_len = 0;
        char fcmd[3];
        rc = getline(&cmd, &cmd_len, stdin);
        if (rc == -1) { printf("Error: bad command\n"); continue; }
        c = cmd[0];
        for (int i = 0; i < 3; i++)
            fcmd[i] = i < cmd_len ? cmd[i] : '\0';
        free(cmd);

        if (c == 'p') {

            if (running) {
                stop_threads();
                oni_size_t run = 0;
                rc = oni_set_opt(ctx, ONI_OPT_RUNNING, &run, sizeof(run));
                if (rc) {
                    printf("Error: %s\n", oni_error_str(rc));
                }
                running = 0;
                printf("Acquisition Paused\n");
            } 
            else {
                start_threads();
                oni_size_t run = 1;
                rc = oni_set_opt(ctx, ONI_OPT_RUNNING, &run, sizeof(run));
                if (rc) {
                    printf("Error: %s\n", oni_error_str(rc));
                }
                running = 1;
                printf("Acquisition started\n");
            }
        }
        else if (c == 'x') {
            stop_threads();
            oni_size_t reset = 1;
            rc = oni_set_opt(ctx, ONI_OPT_RESET, &reset, sizeof(reset));
            if (rc) { printf("Error: %s\n", oni_error_str(rc)); }
            update_dev_table();
            goto reset;
        }
        else if (c == 'd') {
            display = (display == 0) ? 1 : 0;
        }
        else if (c == 'D') {

            float display_rate = 100.0 * 1.0 / (float)display_every_n;

            printf("Enter the percent of frames to display. 100.0 will display "
                   "every frame. The current rate is %.3f%%.\n",
                   display_rate);
            printf(">>> ");

            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) { printf("Error: bad command\n"); continue;}

            display_rate = atof(buf);

            if (display_rate <= 0 || display_rate > 100) {
                printf("Error: invalid number. Pick a value within (0.0, 100.0].\n");
                continue;
            }

            display_every_n = (int)(100.0 / display_rate);
            printf("Display rate set to %.3f%%\n", display_rate);
        }
        else if (c == 'i') {
            printf("Change the device index display filter.\n");
            printf("Enter the device index you wish to display or -1 for no filter\n");
            printf(">>> ");

            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) { printf("Error: bad command\n"); continue; }

            int idx = atoi(buf);
            device_idx_filter_en = idx < 0 ? 0 : 1;
            device_idx_filter = (size_t)idx;

            if (device_idx_filter_en)
                printf("Only displaying frames from device at index %d\n", device_idx_filter);
            else
                puts("Dislaying frames from all devices");
        }
        else if (c == 't') {
            print_dev_table(devices, num_devs);
        }
        else if (c == 'w') {
            int nargs;
            printf("Write to a device register.\n");
            if (fcmd[1] == 'i') {
                printf("Enter: dev_idx i2c_dev_addr reg_addr reg_val\n");
                nargs = 4;
            } else {
                printf("Enter: dev_idx reg_addr reg_val\n");
                nargs = 3;
            }
            printf(">>> ");

            // Read the command
            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) { printf("Error: bad command\n"); continue; }

            // Parse the command string
            long values[4];
            rc = parse_reg_cmd(buf, values, nargs);
            if (rc == -1) { printf("Error: bad command\n"); continue; }
            free(buf);

            size_t dev_idx = (size_t)values[0];
            oni_size_t addr;
            oni_size_t val;
            if (fcmd[1] == 'i')
            {
                addr = get_i2c_reg_address(values[1],values[2],(fcmd[2] == 'x') ? 1: 0);
                val = (oni_size_t)values[4];
            }
            else if (fcmd[1] == 'm')
            {
                addr = (oni_size_t)(values[1] | (1<<15));
                val = (oni_size_t)values[2];

            } else {
                addr = (oni_size_t)values[1];
                val = (oni_size_t)values[2];
            }

            rc = oni_write_reg(ctx, dev_idx, addr, val);
            printf("%s\n", oni_error_str(rc));
        }
        else if (c == 'r') {
            int nargs;
            printf("Read a device register.\n");
            if (fcmd[1] == 'i') {
                printf("Enter: dev_idx i2c_dev_addr reg_addr\n");
                nargs = 3;
            } else {
                printf("Enter: dev_idx reg_addr\n");
                nargs = 2;
            }
            printf(">>> ");

            // Read the command
            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) { printf("Error: bad command\n"); continue; }

            // Parse the command string
            long values[3];
            rc = parse_reg_cmd(buf, values, nargs);
            if (rc == -1) { printf("Error: bad command\n"); continue; }
            free(buf);

            size_t dev_idx = (size_t)values[0];
            oni_size_t addr;
            if (fcmd[1] == 'i') {
                addr = get_i2c_reg_address(
                    values[1], values[2], (fcmd[2] == 'x') ? 1 : 0);
            } else if (fcmd[1] == 'm') {
                addr = (oni_size_t)(values[1] | (1 << 15));

            } else {
                addr = (oni_size_t)values[1];
            }

            oni_reg_val_t val = 0;
            rc = oni_read_reg(ctx, dev_idx, addr, &val);
            if (!rc) {
                printf("Reg. value: %u\n", val);
            } else {
                printf("%s\n", oni_error_str(rc));
            }
        } else if (c == 'h') {
            printf("Get information about the hub for a given device.\n");
            printf("Enter: dev_idx\n");
            printf(">>> ");

            // Read the command
            char *buf = NULL;
            size_t len = 0;
            rc = getline(&buf, &len, stdin);
            if (rc == -1) {
                printf("Error: bad command.\n");
                continue;
            }

            // Parse the command string
            long values[1];
            rc = parse_reg_cmd(buf, values, 1);
            if (rc == -1) {
                printf("Error: bad command.\n");
                continue;
            }
            free(buf);

            size_t hub_idx = (size_t)values[0] & 0x0000FF00;
            print_hub_info(hub_idx);

        }
        else if (c == 'H') {

            size_t last_hub = 0;
            print_hub_info(0);
            printf("\n");

            for (size_t i = 0; i < num_devs; i++) {
                size_t hub = (devices + i)->idx & 0x0000FF00;
                if (hub != last_hub && (devices + i)->id != ONIX_NULL) {
                    print_hub_info(hub);
                    printf("\n");
                    last_hub = hub;
                }
            }
        }
        else if (c == 'a') {
            reg = 1;
            rc = oni_set_opt(ctx, ONI_OPT_RESETACQCOUNTER, &reg, sizeof(oni_size_t));
            assert(!rc && "Register write failure.");
            printf("Acquisition clock counter reset issued.\n");
        }
    }

    stop_threads();

    if (dump) {
        // Close dump files
        for (size_t i = 0; i < num_devs; i++) {
            if (devices[i].id != ONIX_NULL) {
                fclose(dump_files[i]);
            }
        }
    }

    // Stop hardware
    oni_size_t run = 0 ;
    rc = oni_set_opt(ctx, ONI_OPT_RUNNING, &run, sizeof(run));
    if (rc) { printf("Error: %s\n", oni_error_str(rc)); }

    // Free dynamic stuff
    oni_destroy_ctx(ctx);
    free(devices);

    return 0;
}
