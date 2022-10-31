// This is a very simple ONI-compliant hardware emulator. It has some
// limitations:
//
// 1. ONI_OPT_RUNNING does nothing. To make it work, data would need to be
//    produced on a separate thread and passed through a blocking FIFO
// 2. It only supports a device table with containing devices with a fixed
//    read/write size
// 3. Writing to the device does nothing (data is just ignored)
// 4. The block read size must be a multiple of the frame size, which is
//    currently 16 (Header) + 12 (data) = 28 bytes.

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../onidefs.h"
#include "../../oni.h"
#include "../../onidriver.h"
#include "../../onix.h"
#include "../../liboni-test/testfunc.h"
#include "queue_u8.h"

#define UNUSED(x) (void)(x)

// NB: To save some repetition
#define CTX_CAST const oni_test_ctx ctx = (oni_test_ctx)driver_ctx

#define MIN(a,b) ((a<b) ? a : b)

struct conf_reg {
    uint32_t dev_idx;
    uint32_t reg_addr;
    uint32_t reg_value;
    uint32_t rw;
    uint32_t running;
    uint32_t sysclkhz;
    uint32_t acqclkhz;
    uint32_t hwaddress;
};

typedef struct {
    oni_device_t dev;
    int32_t message;
    uint64_t counter;
    uint32_t hubhwid;
    uint32_t hubfirmver;
    uint32_t hubclkhz;
    uint32_t hubdelayns;
} test_dev_t;

struct oni_test_ctx_impl {

    // Block read size during buffer update in liboni
    // NB: Must be a multiple of the read frame size
    // ONI_FRAMEHEADERSZ + sizeof(data)
    oni_size_t block_read_size;

    // HW address
    uint32_t hw_addr;

    // Counters
    uint64_t frame_num;

    // Signal queue
    queue_u8_t *sig_queue;

    // Configuration registers
    struct conf_reg conf;

    // Device table is a single device
    int num_devs;
    test_dev_t dev_table[4];
};

typedef struct oni_test_ctx_impl* oni_test_ctx;

typedef enum oni_signal {
    NULLSIG             = (1u << 0),
    CONFIGWACK          = (1u << 1), // Configuration write-acknowledgment
    CONFIGWNACK         = (1u << 2), // Configuration no-write-acknowledgment
    CONFIGRACK          = (1u << 3), // Configuration read-acknowledgment
    CONFIGRNACK         = (1u << 4), // Configuration no-read-acknowledgment
    DEVICEMAPACK        = (1u << 5), // Device map start acknowledgment
    DEVICEINST          = (1u << 6), // Device map instance
} oni_signal_t;

static void _fill_read_buffer(oni_test_ctx ctx,
                              uint32_t *data,
                              size_t num_words);
static int _send_msg_signal(oni_test_ctx ctx, oni_signal_t type);
static int _send_data_signal(oni_test_ctx ctx,
                             oni_signal_t type,
                             void *data,
                             size_t n);
static int _find_dev(oni_test_ctx ctx, oni_dev_idx_t idx);
static int _find_hub_mgr(oni_dev_idx_t idx);

// TODO:
//static const size_t write_stream_width = 4;
//static const size_t read_stream_width = 4;
static const oni_size_t write_block_size = 1024;

oni_driver_ctx oni_driver_create_ctx()
{
    // Context
    oni_test_ctx ctx;
    ctx = calloc(1, sizeof(struct oni_test_ctx_impl));
    if (ctx == NULL)
        return NULL;

    ctx->block_read_size = 280;

    // HW address
    ctx->hw_addr = 0;

    // Counters
    ctx->frame_num = 0;

    // Create devices, each on their own hub
    ctx->num_devs = sizeof(ctx->dev_table) / sizeof(test_dev_t);

    int i;
    for (i = 0; i < ctx->num_devs; i++) {

        ctx->dev_table[i].dev.idx = i << 8; // All dev_idx 0 on different hubs
        ctx->dev_table[i].dev.id = ONIX_TEST0;
        ctx->dev_table[i].dev.version = 1;
        ctx->dev_table[i].dev.read_size = 12;
        ctx->dev_table[i].dev.write_size = 32;
        ctx->dev_table[i].message = i * 42;
        ctx->dev_table[i].counter = 0;
        ctx->dev_table[i].hubhwid = 5;
        ctx->dev_table[i].hubfirmver = 1600;
        ctx->dev_table[i].hubclkhz = (i + 1) * 50e6;
        ctx->dev_table[i].hubdelayns = 628;
    }

    // Configuration registers
    ctx->conf.dev_idx = 0;
    ctx->conf.reg_addr = 0;
    ctx->conf.reg_value = 0;
    ctx->conf.rw = 0;
    ctx->conf.running = 1;
    ctx->conf.sysclkhz = 200e6;
    ctx->conf.acqclkhz = 200e6;
    ctx->conf.hwaddress = 0;

    // Signal queue
    ctx->sig_queue = queue_u8_create(1024);

    return ctx;
}

int oni_driver_init(oni_driver_ctx driver_ctx, int host_idx)
{
    UNUSED(driver_ctx);
    UNUSED(host_idx);

    return ONI_ESUCCESS;
}

int oni_driver_destroy_ctx(oni_driver_ctx driver_ctx)
{
    CTX_CAST;
    assert(ctx != NULL && "Driver context is NULL");

    // Free the queue
    queue_u8_destroy(ctx->sig_queue);

    // Free the context
    free(ctx);

    return ONI_ESUCCESS;
}

static int _send_data_signal(oni_test_ctx ctx,
                             oni_signal_t type,
                             void *data,
                             size_t n)
{
    size_t packet_size = sizeof(oni_signal_t) + n;

    // Make sure packet_size < 254
    if (packet_size > 254)
        return -1;

    // Src and dst buffers
    uint8_t *src = malloc(packet_size);
    uint8_t dst[256] = {0}; // Maximal packet size including delimeter

    // Concatenate signal type and data
    memcpy(src, &type, sizeof(type));
    if (n > 0 && data != NULL)
        memcpy(src + sizeof(type), data, n);

    // Create COBs packet with overhead byte
    cobs_stuff(dst, src, packet_size);

    // COBS data, 1 overhead byte + 0x0 delimiter
    size_t i;
    for (i = 0; i < packet_size + 2; i++)
       if (queue_u8_enqueue(ctx->sig_queue, dst[i]) == -1)
           return -1;

    free(src);

    return packet_size + 2;
}

// Generate frames
// TODO: Second thread puts frames into FIFO whenever it reaches XX fraction of full
int oni_driver_read_stream(oni_driver_ctx driver_ctx,
                           oni_read_stream_t stream,
                           void *data,
                           size_t size)
{
    CTX_CAST;
    int rc;

    if (stream == ONI_READ_STREAM_DATA)
    {
        // TODO: Pre-generate single static frame so we can see true
        // performance of library
        _fill_read_buffer(ctx, data, size >> 2);
        return ctx->block_read_size;
    }
    else if (stream == ONI_READ_STREAM_SIGNAL)
    {
        // Might actually need a blocking FIFO here.
        oni_size_t read = 0;
        while (read < size)
        {
            rc = queue_u8_dequeue(ctx->sig_queue, (uint8_t *)data + read);
            if (rc < 0) return ONI_EREADFAILURE;
            read++;
        }
        return read;
    }
    else return ONI_EPATHINVALID;
}

// Accept frame data and side effect frames in some way
int oni_driver_write_stream(oni_driver_ctx driver_ctx,
                            oni_write_stream_t stream,
                            const char *data,
                            size_t size)
{
    UNUSED(driver_ctx);
    size_t remaining = size >> 2; // bytes to 32 bit words
    size_t to_send, sent;
    uint32_t *ptr = (uint32_t *)data;

    if (stream != ONI_WRITE_STREAM_DATA) return ONI_EPATHINVALID;

    while (remaining > 0) {
        to_send = MIN(remaining, write_block_size);
        sent = to_send; // TODO: Some side effect instead of nothing
        if (sent != to_send) return ONI_EWRITEFAILURE;
        ptr += sent;
        remaining -= sent;
    }

    return size;
}

int oni_driver_write_config(oni_driver_ctx driver_ctx,
                            oni_config_t reg,
                            oni_reg_val_t value)
{
    CTX_CAST;

    switch (reg) {
        case ONI_CONFIG_DEV_IDX:
            ctx->conf.dev_idx = value;
            break;
        case ONI_CONFIG_REG_ADDR:
            ctx->conf.reg_addr = value;
            break;
        case ONI_CONFIG_REG_VALUE:
            ctx->conf.reg_value = value;
            break;
        case ONI_CONFIG_RW:
            ctx->conf.rw = value;
            break;
        case ONI_CONFIG_TRIG: {

            // Find device in table
            int i = _find_dev(ctx, ctx->conf.dev_idx);

            int hub_mgr = 0;
            if (i < 0) { // If no device, maybe it's a hub manager

                // Find the hub if this is a hub manager
                i = _find_hub_mgr(ctx->conf.dev_idx);

                if (i < 0)
                    return ONI_EDEVIDX;
                else
                    hub_mgr = 1;
            }

            if (value && !ctx->conf.rw) { // read
                if (hub_mgr) {
                    switch (ctx->conf.reg_addr) {
                        case ONIX_HUB_HARDWAREID:
                            ctx->conf.reg_value = ctx->dev_table[i].hubhwid;
                            break;
                        case ONIX_HUB_FIRMWAREVER:
                            ctx->conf.reg_value = ctx->dev_table[i].hubfirmver;
                            break;
                        case ONIX_HUB_CLKRATEHZ:
                            ctx->conf.reg_value = ctx->dev_table[i].hubclkhz;
                            break;
                        case ONIX_HUB_DELAYNS:
                            ctx->conf.reg_value = ctx->dev_table[i].hubdelayns;
                            break;
                    }
                    _send_msg_signal(ctx, CONFIGRACK);
                } else if (ctx->conf.reg_addr == 1) { // Only register 1 (message) is specified for this device
                    ctx->conf.reg_value = ctx->dev_table[i].message;
                    _send_msg_signal(ctx, CONFIGRACK);
                } else {
                    _send_msg_signal(ctx, CONFIGRNACK);
                }
            } else if (value) { // write
                if (hub_mgr) {
                    _send_msg_signal(ctx, CONFIGWNACK); // Read only
                } else if (ctx->conf.reg_addr == 1) { // Only register 1 (message) is specified for this device
                    ctx->dev_table[i].message = (short)ctx->conf.reg_value;
                    _send_msg_signal(ctx, CONFIGWACK);
                } else {
                    _send_msg_signal(ctx, CONFIGWNACK);
                }
            }
            break;

        }
        case ONI_CONFIG_RUNNING:
            // TODO: To do this, we need data to be produced on a separate thread
            // and passed through a blocking FIFO
            ctx->conf.running = value;
            break;
        case ONI_CONFIG_RESET: {

            if (value == 0) return ONI_ESUCCESS;

            // Put the device map onto the signal stream fifo
            _send_data_signal(ctx, DEVICEMAPACK, &ctx->num_devs, sizeof(ctx->num_devs));

            // Loop through devices
            int i;
            for (i = 0; i < ctx->num_devs; i++)
                _send_data_signal(ctx, DEVICEINST, &ctx->dev_table[i].dev, sizeof(oni_device_t));

            break;
        }
        case ONI_CONFIG_SYSCLKHZ:
            return ONI_EREADONLY;
        case ONI_CONFIG_ACQCLKHZ:
            return ONI_EREADONLY;
        case ONI_CONFIG_RESETACQCOUNTER:
            if (value)
                ctx->frame_num = 0;
            break;
        case ONI_CONFIG_HWADDRESS:
            ctx->hw_addr = value;
            break;
        default:
            return ONI_EINVALARG;
    }

    return ONI_ESUCCESS;
}

int oni_driver_read_config(oni_driver_ctx driver_ctx,
                           oni_config_t reg,
                           oni_reg_val_t *value)
{
    CTX_CAST;

    switch (reg) {
        case ONI_CONFIG_DEV_IDX:
            *value = ctx->conf.dev_idx;
            break;
        case ONI_CONFIG_REG_ADDR:
            *value = ctx->conf.reg_addr;
            break;
        case ONI_CONFIG_REG_VALUE:
            *value = ctx->conf.reg_value;
            break;
        case ONI_CONFIG_RW:
            *value = ctx->conf.rw;
            break;
        case ONI_CONFIG_TRIG:
            // TODO: if reading register takes time, with more complex
            // implementation, this must reflect that
            *value = 0;
            break;
        case ONI_CONFIG_RUNNING:
            *value = ctx->conf.running;
            break;
        case ONI_CONFIG_RESET:
            return ONI_EWRITEONLY;
        case ONI_CONFIG_SYSCLKHZ:
            *value = ctx->conf.sysclkhz;
            break;
        case ONI_CONFIG_ACQCLKHZ:
            *value = ctx->conf.acqclkhz;
            break;
        case ONI_CONFIG_RESETACQCOUNTER:
            return ONI_EWRITEONLY;
        case ONI_CONFIG_HWADDRESS:
            *value = ctx->hw_addr;
            break;
        default:
            return ONI_EINVALARG;
    }

    return ONI_ESUCCESS;
}

int oni_driver_set_opt_callback(oni_driver_ctx driver_ctx,
                                int oni_option,
                                const void *value,
                                size_t option_len)
{
    CTX_CAST;
    UNUSED(oni_option);
    UNUSED(option_len);

    if (oni_option == ONI_OPT_BLOCKREADSIZE)
    {
        // NB: Block size must be a multiple of a full data frame
        if (*(oni_size_t *)value % (ONI_FRAMEHEADERSZ + ctx->dev_table[0].dev.read_size) != 0)
            return ONI_EINVALARG;
        else
            ctx->block_read_size = *(oni_size_t *) value;
    }

    return ONI_ESUCCESS;
}

// NB: This driver does not have custom options
int oni_driver_set_opt(oni_driver_ctx driver_ctx,
                       int driver_option,
                       const void *value,
                       size_t option_len)
{
    UNUSED(driver_ctx);
    UNUSED(driver_option);
    UNUSED(value);
    UNUSED(option_len);
    return ONI_EINVALOPT;
}

int oni_driver_get_opt(oni_driver_ctx driver_ctx,
                       int driver_option,
                       void *value,
                       size_t *option_len)
{
    UNUSED(driver_ctx);
    UNUSED(driver_option);
    UNUSED(value);
    UNUSED(option_len);
    return ONI_EINVALOPT;
}

const char *oni_driver_str()
{
    return "test";
}

static void _fill_read_buffer(oni_test_ctx ctx, uint32_t *data, size_t num_words)
{
    // Here we are dealing with uint32_t data
    // 1. index (1)
    // 2. data_sz (1)
    // 3. timer (2)
    // 4. Data ([8: counter, 4: message])
    size_t i;
    int d = rand() % ctx->num_devs;
    for (i = 0;
         i < num_words;
         d = rand() % ctx->num_devs,
         i += (ONI_FRAMEHEADERSZ + ctx->dev_table[d].dev.read_size) >> 2) {

        // Header
        *((uint64_t *)(data + i)) = ctx->frame_num++;
        *((data + i + 2)) = ctx->dev_table[d].dev.idx;
        *((data + i + 3)) = ctx->dev_table[d].dev.read_size;

        // Data in the order it is actually produced
        // by real hardware version of this device.
        uint16_t *temp = (uint16_t *)(data + i + 4);

        // Hub counter
        temp[0] = (uint16_t)(ctx->dev_table[d].counter << 48);
        temp[1] = (uint16_t)(ctx->dev_table[d].counter << 32);
        temp[2] = (uint16_t)(ctx->dev_table[d].counter << 16);
        temp[3] = (uint16_t)(ctx->dev_table[d].counter << 0);

        // Message
        temp[4] = (uint16_t)(ctx->dev_table[d].message << 16);
        temp[5] = (uint16_t)(ctx->dev_table[d].message << 0);

        ctx->dev_table[d].counter++;
    }
}

static int _send_msg_signal(oni_test_ctx ctx, oni_signal_t type)
{
    // COBS data, 1 overhead byte + 0x0 delimiter
    uint8_t dst[sizeof(oni_signal_t) + 2] = {0};

    cobs_stuff(dst, (uint8_t *)&type, sizeof(type));

    size_t i;
    for (i = 0; i < sizeof(dst); i++)
        if (queue_u8_enqueue(ctx->sig_queue, dst[i]) == -1)
            return -1;

    return sizeof(dst);
}

// Simple & slow device lookup
static int _find_dev(oni_test_ctx ctx, oni_dev_idx_t idx)
{
    int i;
    for (i = 0; i < ctx->num_devs; i++)
        if (ctx->dev_table[i].dev.idx == idx)
            return i;

    return -1;
}

static int _find_hub_mgr(oni_dev_idx_t idx)
{
    if ((idx & 0x000000FF) == ONIX_HUB_DEV_IDX)
        return ((idx & 0x0000FF00) >> 8);

    return -1;
}
