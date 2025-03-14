// TODO:
// 1. Get rid of oni_fifo_size_t and anything else hardcoding fifo width at
// this top level driver. Or the loaded driver could import this number somehow
// (e.g. via extern variable or context member)

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oni.h"
#include "onidriverloader.h"

// Device hash table overhead factor
#define ONI_DEVHASHOVERHEAD (10.0)

// NB: 255 in sub-address is invalid in this implementation,
// so we can use this to clear the hash table
#define ONI_DEVIDXNULL 0xFFFFFFFF

// Consistent overhead bytestuffing buffer size
#define ONI_COBSBUFFERSIZE 255

// Frame constants
#define ONI_RFRAMEHEADERSZ sizeof(oni_fifo_time_t) + 2 * sizeof(oni_fifo_dat_t) // [time, dev_idx, data_sz]
#define ONI_WFRAMEHEADERSZ 2 * sizeof(oni_fifo_dat_t) // [dev_idx, data_sz]

// Reference counter
struct ref {
    void (*free)(const struct ref *);
    int count;
};

// Reference counting buffer
struct oni_buf_impl {

    // Raw data buffer
    uint8_t *buffer;
    uint8_t *read_pos;
    uint8_t *end_pos;

    // Reference count
    struct ref count;
};

// Frame with attached, automatically managed storage
typedef union {
    oni_frame_t public;
    struct {
        oni_frame_t f;
        struct oni_buf_impl *buffer;
    } private;
} oni_frame_impl_t;

// Acquisition context
struct oni_ctx_impl {

    // Hardware translation driver
    oni_driver_t driver;

    // Device array
    oni_size_t num_dev;
    oni_device_t *dev_table;

    // oni_device_t.idx addressable device hash table
    oni_size_t dev_hash_len;
    oni_device_t *dev_hash_table;

    // Maximum frame size (bytes, includes header)
    oni_size_t max_read_frame_size;
    oni_size_t max_write_frame_size;

    // Block read/write size (bytes, defaults to max_read/write_frame_size)
    oni_size_t block_read_size;
    oni_size_t block_write_size;

    // Current, attached buffers
    struct oni_buf_impl *shared_rbuf;
    struct oni_buf_impl *shared_wbuf;

    // Acquisition state
    enum {
        CTXNULL = 0,
        UNINITIALIZED,
        IDLE,
        RUNNING
    } run_state;
};

// Signal flags
typedef enum {
    NULLSIG             = (1u << 0),
    CONFIGWACK          = (1u << 1), // Configuration write-acknowledgment
    CONFIGWNACK         = (1u << 2), // Configuration no-write-acknowledgment
    CONFIGRACK          = (1u << 3), // Configuration read-acknowledgment
    CONFIGRNACK         = (1u << 4), // Configuration no-read-acknowledgment
    DEVICETABLEACK      = (1u << 5), // Device table start acknowledgment
    DEVICEINST          = (1u << 6), // Device table instance
} oni_signal_t;

// Helpers
static inline oni_dev_idx_t _oni_hash32(oni_dev_idx_t x);
static inline int _oni_hash32_find(oni_ctx ctx, oni_dev_idx_t x);
static int _oni_reset_routine(oni_ctx ctx);
static inline int _oni_read(oni_ctx ctx, oni_read_stream_t stream, void *data, size_t size);
static inline int _oni_write(oni_ctx ctx, oni_write_stream_t stream, const char* data, size_t size);
static int _oni_read_signal_packet(oni_ctx ctx, uint8_t *buffer);
static int _oni_read_signal_data(oni_ctx ctx, oni_signal_t *type, void *data, size_t size);
static int _oni_pump_signal_type(oni_ctx ctx, int flags, oni_signal_t *type);
static int _oni_pump_signal_data(oni_ctx ctx, int flags, oni_signal_t *type, void *data, int size);
static int _oni_cobs_unstuff(uint8_t *dst, const uint8_t *src, size_t size);
static inline int _oni_write_config(oni_ctx ctx, oni_config_t reg, oni_reg_val_t value);
static inline int _oni_read_config(oni_ctx, oni_config_t reg, oni_reg_val_t *value);
static int _oni_alloc_write_buffer(oni_ctx ctx, void **data, size_t size);
static int _oni_read_buffer(oni_ctx ctx, void **data, size_t size, int);
static void _oni_dump_buffers(oni_ctx ctx);
static void _oni_destroy_buffer(const struct ref *ref);
static inline void _ref_inc(const struct ref *ref);
static inline void _ref_dec(const struct ref *ref);

oni_ctx oni_create_ctx(const char* drv_name)
{
    oni_ctx ctx = calloc(1, sizeof(struct oni_ctx_impl));

    if (ctx == NULL) {
        errno = EAGAIN;
        return NULL;
    }

    if (oni_create_driver(drv_name, &ctx->driver)) {
        errno = EINVAL;
        free(ctx);
        return NULL;
    }

    ctx->num_dev = 0;
    ctx->dev_hash_table = NULL;
    ctx->run_state = UNINITIALIZED;

    return ctx;
}

int oni_init_ctx(oni_ctx ctx, int host_idx)
{
    assert(ctx != NULL && "Context is NULL.");
    assert(ctx->run_state == UNINITIALIZED && "Context is in invalid state.");

    if (ctx->run_state != UNINITIALIZED)
        return ONI_EINVALSTATE;

    int rc = ctx->driver.init(ctx->driver.ctx, host_idx);
    if (rc) return rc;

    // NB: Trigger reset routine (populates device table and key acquisition
    // parameters) Success will set ctx->run_state to IDLE

    // Set the reset register
    rc = _oni_write_config(ctx, ONI_CONFIG_RESET, 1);
    if (rc) return rc;

    // Get device table etc
    rc = _oni_reset_routine(ctx);
    if (rc) return rc;

    // Run state is now IDLE
    ctx->run_state = IDLE;

    return ONI_ESUCCESS;
}

int oni_destroy_ctx(oni_ctx ctx)
{
    assert(ctx != NULL && "Context is NULL");

    int rc = ctx->driver.destroy_ctx(ctx->driver.ctx);
    if (rc) return rc;

    if (ctx->dev_hash_table != NULL)
        free(ctx->dev_hash_table);

    free(ctx);

    return ONI_ESUCCESS;
}

int oni_get_opt(const oni_ctx ctx, int ctx_opt, void *value, size_t *option_len)
{
    assert(ctx != NULL && "Context is NULL");

    switch (ctx_opt) {
        case ONI_OPT_DEVICETABLE: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            size_t required_bytes = sizeof(oni_device_t) * ctx->num_dev;
            if (*option_len < required_bytes)
                return ONI_EBUFFERSIZE;

            memcpy(value, ctx->dev_table, required_bytes);
            *option_len = required_bytes;
            break;
        }
        case ONI_OPT_NUMDEVICES: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            size_t required_bytes = sizeof(oni_size_t);
            if (*option_len < required_bytes)
                return ONI_EBUFFERSIZE;

            *(oni_size_t *)value = ctx->num_dev;
            *option_len = required_bytes;
            break;
        }
        case ONI_OPT_RUNNING: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_read_config(ctx, ONI_CONFIG_RUNNING, value);
            if (rc) return rc;

            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_SYSCLKHZ: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_read_config(ctx, ONI_CONFIG_SYSCLKHZ, value);
            if (rc) return rc;

            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_ACQCLKHZ: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_read_config(ctx, ONI_CONFIG_ACQCLKHZ, value);
            if (rc) return rc;

            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_HWADDRESS: {

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_read_config(ctx, ONI_CONFIG_HWADDRESS, value);
            if (rc) return rc;

            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_MAXREADFRAMESIZE: {

            assert(ctx->run_state > UNINITIALIZED
                   && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            size_t required_bytes = sizeof(oni_size_t);
            if (*option_len < required_bytes)
                return ONI_EBUFFERSIZE;

            *(oni_size_t *)value = ctx->max_read_frame_size;
            *option_len = required_bytes;
            break;
        }
        case ONI_OPT_MAXWRITEFRAMESIZE: {

            assert(ctx->run_state > UNINITIALIZED
                   && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            size_t required_bytes = sizeof(oni_size_t);
            if (*option_len < required_bytes)
                return ONI_EBUFFERSIZE;

            *(oni_size_t *)value = ctx->max_write_frame_size;
            *option_len = required_bytes;
            break;
        }
        case ONI_OPT_BLOCKREADSIZE: {

            assert(ctx->run_state > UNINITIALIZED
                   && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            *(oni_size_t *)value = ctx->block_read_size;
            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_BLOCKWRITESIZE: {

            assert(ctx->run_state > UNINITIALIZED
                   && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            *(oni_size_t *)value = ctx->block_write_size;
            *option_len = ONI_REGSZ;
            break;
        }
        case ONI_OPT_RESET:
        case ONI_OPT_RESETACQCOUNTER:
            return ONI_EWRITEONLY;
        default: {

            // Attempt to read to custom (outside ONI spec) configuration
            // option
            assert(ctx_opt >= ONI_OPT_CUSTOMBEGIN && "Invalid custom configuration register.");
            if (ctx_opt < ONI_OPT_CUSTOMBEGIN)
                return ONI_EPROTCONFIG;

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (*option_len < ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_read_config(ctx,
                                      ONI_CONFIG_CUSTOMBEGIN
                                          + (ctx_opt - ONI_OPT_CUSTOMBEGIN),
                                      value);
            if (rc) return rc;

            *option_len = ONI_REGSZ;

            break;
        }
    }
    return ONI_ESUCCESS;
}

int oni_set_opt(oni_ctx ctx, int ctx_opt, const void *value, size_t option_len)
{
    assert(ctx != NULL && "Context is NULL");

    switch (ctx_opt) {
        case ONI_OPT_RUNNING: {
            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (option_len != ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_write_config(
                ctx, ONI_CONFIG_RUNNING, *(oni_reg_val_t*)value);
            if (rc) return rc;

            // Dump buffers
            // TODO: Is this always the right thing to do? In the case our
            // RIFFA implementation, yes. But other implementations may not
            // clear intermediate FIFOs meaning that the first data encountered
            // on restart is not the start of a frame.
            _oni_dump_buffers(ctx);

            if (*(oni_reg_val_t *)value != 0)
                ctx->run_state = RUNNING;
            else
                ctx->run_state = IDLE;
            break;
        }
        case ONI_OPT_RESET: {
            assert(ctx->run_state == IDLE && "Context state must be IDLE.");
            if (ctx->run_state != IDLE)
                return ONI_EINVALSTATE;

            if (option_len != ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            if (*(oni_reg_val_t *)value != 0) {

                int rc = _oni_write_config(
                    ctx, ONI_CONFIG_RESET, *(oni_reg_val_t*)value);
                if (rc) return rc;

                // Get device table etc
                _oni_reset_routine(ctx);
            }

            break;
        }
        case ONI_OPT_RESETACQCOUNTER: {
            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (option_len != ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            if (*(oni_reg_val_t *)value != 0) {

                int rc = _oni_write_config(
                    ctx, ONI_CONFIG_RESETACQCOUNTER, *(oni_reg_val_t *)value);
                if (rc) return rc;
            }

            break;
        }
        case ONI_OPT_HWADDRESS: {
            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (option_len != ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_write_config(
                ctx, ONI_CONFIG_HWADDRESS, *(oni_reg_val_t *)value);
            if (rc) return rc;

            break;
        }
        case ONI_OPT_BLOCKREADSIZE: {
            // NB: If we are careful, this could be changed during RUNNING
            // state. However, I would need to perform runtime size check of
            // the block readsize. Also, what if it occurred on a separate
            // thread during a call to oni_read_frame?
            assert(ctx->run_state == IDLE && "Context state must be IDLE.");
            if (ctx->run_state != IDLE)
                return ONI_EINVALSTATE;

            if (option_len != sizeof(oni_size_t))
                return ONI_EBUFFERSIZE;

            oni_size_t block_read_size = *(oni_size_t *)value;

            // Make sure the block read size is greater than max frame size
            if (block_read_size < ctx->max_read_frame_size)
                return ONI_EINVALREADSIZE;

            // Make sure the block read size is a multiple of the FIFO width
            if (block_read_size % sizeof(oni_fifo_dat_t) != 0)
                return ONI_EINVALREADSIZE;

            ctx->block_read_size = block_read_size;

            break;

        }
        case ONI_OPT_BLOCKWRITESIZE: {
            // NB: If we are careful, this could be changed during RUNNING
            // state. However, I would need to perform runtime size check of
            // the block readsize. Also, what if it occurred on a separate
            // thread during a call to oni_read_frame?
            assert(ctx->run_state == IDLE && "Context state must be IDLE.");
            if (ctx->run_state != IDLE)
                return ONI_EINVALSTATE;

            if (option_len != sizeof(oni_size_t))
                return ONI_EBUFFERSIZE;

            oni_size_t block_write_size = *(oni_size_t *)value;

            // Make sure the block read size is greater than max frame size
            if (block_write_size < ctx->max_write_frame_size)
                return ONI_EINVALWRITESIZE;

            // Make sure the block read size is a multiple of the FIFO width
            if (block_write_size % sizeof(oni_fifo_dat_t) != 0)
                return ONI_EINVALWRITESIZE;

            ctx->block_write_size = block_write_size;

            break;
        }
        case ONI_OPT_DEVICETABLE:
        case ONI_OPT_NUMDEVICES:
        case ONI_OPT_SYSCLKHZ:
        case ONI_OPT_ACQCLKHZ:
        case ONI_OPT_MAXREADFRAMESIZE:
        case ONI_OPT_MAXWRITEFRAMESIZE:
            return ONI_EREADONLY;
        default: {

            // Attempt to write to custom (outside ONI spec) configuration
            // option
            assert(ctx_opt >= ONI_OPT_CUSTOMBEGIN && "Invalid custom configuration register.");
            if (ctx_opt < ONI_OPT_CUSTOMBEGIN)
                return ONI_EPROTCONFIG;

            assert(ctx->run_state > UNINITIALIZED && "Context state must be IDLE or RUNNING.");
            if (ctx->run_state < IDLE)
                return ONI_EINVALSTATE;

            if (option_len != ONI_REGSZ)
                return ONI_EBUFFERSIZE;

            int rc = _oni_write_config(ctx,
                                       ONI_CONFIG_CUSTOMBEGIN + (ctx_opt - ONI_OPT_CUSTOMBEGIN),
                                       *(oni_reg_val_t *)value);
            if (rc) return rc;

            break;
        }
    }

    return ctx->driver.set_opt_callback(ctx->driver.ctx, ctx_opt, value, option_len);
}

int oni_get_driver_opt(const oni_ctx ctx, int drv_opt, void* value, size_t *option_len)
{
    return ctx->driver.get_opt(ctx->driver.ctx, drv_opt, value, option_len);
}

int oni_set_driver_opt(oni_ctx ctx, int drv_opt, const void* value, size_t option_len)
{
    return ctx->driver.set_opt(ctx->driver.ctx, drv_opt, value, option_len);
}

int oni_write_reg(const oni_ctx ctx,
                  oni_dev_idx_t dev_idx,
                  oni_reg_addr_t addr,
                  oni_reg_val_t value)
{
    assert(ctx != NULL && "Context is NULL");
    assert(ctx->run_state > UNINITIALIZED && "Context must be INITIALIZED.");

    // Make sure we are not already in config triggered state
    oni_reg_val_t trig = 0;
    int rc = _oni_read_config(ctx, ONI_CONFIG_TRIG, &trig);
    if (rc) return rc;

    if (trig != 0) return ONI_ERETRIG;

    // Set config registers and trigger a write
    rc = _oni_write_config(ctx, ONI_CONFIG_DEV_IDX, dev_idx);
    if (rc) return rc;
    rc = _oni_write_config(ctx, ONI_CONFIG_REG_ADDR, addr);
    if (rc) return rc;
    rc = _oni_write_config(ctx, ONI_CONFIG_REG_VALUE, value);
    if (rc) return rc;

    oni_reg_val_t rw = 1;
    rc = _oni_write_config(ctx, ONI_CONFIG_RW, rw);
    if (rc) return rc;

    trig = 1;
    rc = _oni_write_config(ctx, ONI_CONFIG_TRIG, trig);
    if (rc) return rc;

    // Wait for response from hardware
    oni_signal_t type;
    rc = _oni_pump_signal_type(ctx, CONFIGWACK | CONFIGWNACK, &type);
    if (rc) return rc;

    if (type == CONFIGWNACK) return ONI_EWRITEFAILURE;

    return ONI_ESUCCESS;
}

int oni_read_reg(const oni_ctx ctx,
                 oni_dev_idx_t dev_idx,
                 oni_reg_addr_t addr,
                 oni_reg_val_t *value)
{
    assert(ctx != NULL && "Context is NULL");
    assert(ctx->run_state > UNINITIALIZED && "Context must be INITIALIZED.");

    // Make sure we are not already in config triggered state
    oni_reg_val_t trig = 0;
    int rc = _oni_read_config(ctx, ONI_CONFIG_TRIG, &trig);
    if (rc) return rc;

    if (trig != 0) return ONI_ERETRIG;

    // Set configuration registers and trigger a write
    rc = _oni_write_config(ctx, ONI_CONFIG_DEV_IDX, dev_idx);
    if (rc) return rc;
    rc = _oni_write_config(ctx, ONI_CONFIG_REG_ADDR, addr);
    if (rc) return rc;

    oni_reg_val_t rw = 0;
    rc = _oni_write_config(ctx, ONI_CONFIG_RW, rw);
    if (rc) return rc;

    trig = 1;
    rc = _oni_write_config(ctx, ONI_CONFIG_TRIG, trig);
    if (rc) return rc;

    // Wait for response from hardware
    oni_signal_t type;
    rc = _oni_pump_signal_type(ctx, CONFIGRACK | CONFIGRNACK, &type);
    if (rc) return rc;

    if (type == CONFIGRNACK) return ONI_EREADFAILURE;

    rc = _oni_read_config(ctx, ONI_CONFIG_REG_VALUE, value);
    if (rc) return rc;

    return ONI_ESUCCESS;
}

// NB: Although it seems that with fixed sized reads, we should be able to just
// point the frame header into the shared buffer, the issue is that
// we still need to know what device we are dealing with, which requires that we
// look at the buffer. So there needs to be two _oni_read_buffer's in here no matter
// what, as far as I can tell. But these are basically just function call overhead
// unless there is an allocation event anyway.
int oni_read_frame(const oni_ctx ctx, oni_frame_t **frame)
{
    assert(ctx != NULL && "Context is NULL");

    // NB: We don't need run_state == RUNNING because this could be changed in
    // a different thread
    assert(ctx->run_state >= IDLE && "Context is not acquiring.");

    // No devices produce data
    if (ctx->max_read_frame_size == 0)
        return ONI_ENOREADDEV;

    // Get the device index and data size from the buffer
    // TODO: what is the point of having an oni_fifo_t if we are hard coding the header size anyway?
    uint8_t *header = NULL;
    int rc = _oni_read_buffer(ctx,
                              (void **)&header,
                              ONI_RFRAMEHEADERSZ,
                              1);
    if (rc) return rc;

    // Allocate frame and buffer
    oni_frame_impl_t *iframe = malloc(sizeof(oni_frame_impl_t));

    // Total frame size
    int total_size = sizeof(oni_frame_t);

    // Copy frame header members (continuous)
    // 0. timer (8)
    // 1. index (4)
    // 2. data_sz (4)
    memcpy((void *)&iframe->private.f.time, header, ONI_RFRAMEHEADERSZ);
    assert(iframe->private.f.data_sz > 0 && "Zero-sized frame");
    assert(iframe->private.f.data_sz <= ctx->max_read_frame_size && "Invalid frame size");

    // TODO: max_read_frame_size contains the header as well so the upper bound
    // check is too relaxed.
    if (iframe->private.f.data_sz == 0
        || iframe->private.f.data_sz > ctx->max_read_frame_size)
        return ONI_EBADFRAME;

    // Find read size (+ padding)
    // TODO:https://github.com/open-ephys/ONI/issues/3
    size_t rsize = iframe->private.f.data_sz;
    rsize += rsize % sizeof(oni_fifo_dat_t);
    total_size += rsize;

    // Direct frame data's view into the pre-collected buffer
    rc = _oni_read_buffer(ctx, (void **)&iframe->private.f.data, rsize, 0);
    if (rc) return rc;

    // Update buffer ref count and provide reference to frame
    _ref_inc(&(ctx->shared_rbuf->count));
    iframe->private.buffer = ctx->shared_rbuf;

    // Public portion of frame
    *frame = &iframe->public;

    // Size of public portion of frame
    return total_size;
}

// NB : Multiframe writes are allowed as long as data_sz is a multiple of
// a single write frame's size.
int oni_create_frame(const oni_ctx ctx,
                     oni_frame_t **frame,
                     oni_dev_idx_t dev_idx,
                     void *data,
                     size_t data_sz)
{
    assert(ctx != NULL && "Context is NULL");

    // NB: We don't need run_state == RUNNING because this could be changed in
    // a different thread
    assert(ctx->run_state >= IDLE && "Context is not acquiring.");

    // Get the device hash index
    int i = _oni_hash32_find(ctx, dev_idx);
    if (i < 0) return ONI_EDEVIDX;

    // Check that the devices accepts data
    if (ctx->dev_hash_table[i].write_size == 0)
        return ONI_ENOTWRITEDEV;

    // Check that data_sz is a multiple of write_size
    if (data_sz % ctx->dev_hash_table[i].write_size != 0)
        return ONI_EWRITESIZE;

    // Allocate frame and buffer
    oni_frame_impl_t *iframe = malloc(sizeof(oni_frame_impl_t));

    // Total frame size
    int total_size = sizeof(oni_frame_t);

    // Pad if needed
    size_t asize = data_sz;
    asize += asize % sizeof(oni_fifo_dat_t);
    total_size += asize;

    // Allocate data storage
    char *buffer_start = NULL;
    int rc = _oni_alloc_write_buffer(ctx, (void **)&buffer_start, ONI_WFRAMEHEADERSZ + asize);
    if (rc) return rc;

    // Fill out public fields
    // NB: https://stackoverflow.com/questions/9691404/how-to-initialize-const-in-a-struct-in-c-with-malloc
    *(oni_size_t *)&iframe->private.f.dev_idx = dev_idx;
    *(oni_size_t *)&iframe->private.f.data_sz = data_sz;
    iframe->private.f.data = buffer_start + ONI_WFRAMEHEADERSZ;

    // Copy frame header members into start of continuous buffer, before data
    // 0. index (4)
    // 1. data_sz (4)
    *((oni_fifo_dat_t *)buffer_start + 0) = iframe->private.f.dev_idx;
    *((oni_fifo_dat_t *)buffer_start + 1) = iframe->private.f.data_sz >> BYTE_TO_FIFO_SHIFT;

    // Copy data into frame
    memcpy(iframe->private.f.data, data, data_sz);

    // Update buffer ref count and provide reference to frame
    _ref_inc(&(ctx->shared_wbuf->count));
    iframe->private.buffer = ctx->shared_wbuf;

    // Public portion of frame
    *frame = &iframe->public;

    // Size of public portion of frame
    return total_size;
}

int oni_write_frame(const oni_ctx ctx, const oni_frame_t *frame)
{
    // Write the frame
    oni_frame_impl_t *iframe = (oni_frame_impl_t *)frame;

    // Continuous frame starts ONI_WFRAMEHEADERSZ back in shared buffer
    size_t wsize = iframe->private.f.data_sz + ONI_WFRAMEHEADERSZ;
    int rc = _oni_write(ctx, ONI_WRITE_STREAM_DATA, iframe->private.f.data - ONI_WFRAMEHEADERSZ, wsize);
    if (rc != (int)wsize) return ONI_EWRITEFAILURE;

    return rc;
}

void oni_destroy_frame(oni_frame_t *frame)
{
    if (frame != NULL) {

        oni_frame_impl_t* iframe = (oni_frame_impl_t*)frame;

        // Decrement buffer reference count
        _ref_dec(&(iframe->private.buffer->count));

        // Free the container
        free(iframe);
    }
}

void oni_version(int *major, int *minor, int *patch)
{
    *major = ONI_VERSION_MAJOR;
    *minor = ONI_VERSION_MINOR;
    *patch = ONI_VERSION_PATCH;
}

const oni_driver_info_t* oni_get_driver_info(const oni_ctx ctx)
{
    return ctx->driver.info();
}

const char *oni_error_str(int err)
{
    assert(err > ONI_MINERRORNUM && "Invalid error number.");
    assert(err <= 0 && "Invalid error number.");

    switch (err) {
        case ONI_ESUCCESS: {
            return "Success";
        }
        case ONI_EPATHINVALID: {
            return "Invalid stream path";
        }
        case ONI_EDEVID: {
            return "Invalid device ID";
        }
        case ONI_EDEVIDX: {
            return "Invalid device index";
        }
        case ONI_EWRITESIZE: {
            return "Data size is not an integer multiple of the write size for "
                   "the designated device";
        }
        case ONI_EREADFAILURE: {
            return "Failure to read from a stream or register";
        }
        case ONI_EWRITEFAILURE: {
            return "Failure to write to a stream or register";
        }
        case ONI_ENULLCTX: {
            return "Attempt to use a NULL context";
        }
        case ONI_ESEEKFAILURE: {
            return "Failure to seek on stream";
        }
        case ONI_EINVALSTATE: {
            return "Invalid operation for the current context run state";
        }
        case ONI_EINVALOPT: {
            return "Invalid context option";
        }
        case ONI_EINVALARG: {
            return "Invalid function arguments";
        }
        case ONI_ECOBSPACK: {
            return "Invalid COBS packet";
        }
        case ONI_ERETRIG: {
            return "Attempt to trigger an already triggered operation";
        }
        case ONI_EBUFFERSIZE: {
            return "Supplied buffer is too small";
        }
        case ONI_EBADDEVTABLE: {
            return "Badly formatted device table supplied by firmware";
        }
        case ONI_EBADALLOC: {
            return "Bad dynamic memory allocation";
        }
        case ONI_ECLOSEFAIL: {
            return "File descriptor close failure (check errno)";
        }
        case ONI_EREADONLY: {
            return "Attempted write to read only object (register, context "
                   "option, etc)";
        }
        case ONI_EUNIMPL: {
            return "Unimplemented API feature";
        }
        case ONI_EINVALREADSIZE: {
            return "Block read size is smaller than the maximal frame size";
        }
        case ONI_ENOREADDEV: {
            return "Frame read attempted when there are no readable devices in "
                   "the device table";
        }
        case ONI_EWRITEONLY: {
            return "Attempted to read from a write only object (register, context "
                   "option, etc)";
        }
        case ONI_EINIT: {
            return "Hardware initialization failed";
        }
        case ONI_EINVALWRITESIZE: {
            return "Write buffer pre-allocation size is smaller than the "
                   "maximal write frame size";
        }
        case ONI_ENOTWRITEDEV: {
            return "Frame allocation attempted for a non-writable device";
        }
        case ONI_EDEVIDXREPEAT: {
            return "Device table contains repeated device indices";
        }
        case ONI_EPROTCONFIG : {
            return "Attempted to directly read or write a protected "
                   "configuration option";
        }
        case ONI_EBADFRAME:
        {
            return "Received malformed frame";
        }
        default:
            return "Unknown error";
    }
}

static inline oni_dev_idx_t _oni_hash32(oni_dev_idx_t x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static inline int _oni_hash32_find(oni_ctx ctx, oni_dev_idx_t x)
{
    int probe;

    // TODO: Not sure if this ideal. We are using a hashing function that has no collisions but this
    // is not true when doing the modulo here and requires and explicity check == x for that reason.
    for (probe = _oni_hash32(x) % ctx->dev_hash_len;
        ctx->dev_hash_table[probe].idx != ONI_DEVIDXNULL;
        probe = (probe + 1) % ctx->dev_hash_len) {

        if (ctx->dev_hash_table[probe].idx == x)
            return probe;
    }

    return -1;
}

static int _oni_reset_routine(oni_ctx ctx)
{
    // Get number of devices
    oni_signal_t sig_type = NULLSIG;
    int rc = _oni_pump_signal_data(
        ctx, DEVICETABLEACK, &sig_type, &(ctx->num_dev), sizeof(ctx->num_dev));
    if (rc) return rc;

    // Hash table size
    ctx->dev_hash_len = ctx->num_dev * ONI_DEVHASHOVERHEAD + 1;

    // Make space for the device table
    oni_device_t *temp = realloc(ctx->dev_table,
                                 ctx->num_dev * sizeof(oni_device_t));

    if (temp)
        ctx->dev_table = temp;
    else
        return ONI_EBADALLOC;

    // Make space for the device hash table
    temp = realloc(ctx->dev_hash_table,
                   ctx->dev_hash_len * sizeof(oni_device_t));

    if (temp)
        ctx->dev_hash_table = temp;
    else
        return ONI_EBADALLOC;

    size_t i;
    ctx->max_read_frame_size = 0;
    ctx->max_write_frame_size = 0;
    for (i = 0; i < ctx->num_dev; i++) {

        sig_type = NULLSIG;
        uint8_t buffer[ONI_COBSBUFFERSIZE];
        rc = _oni_read_signal_data(ctx, &sig_type, buffer, ONI_COBSBUFFERSIZE);
        if (rc) return rc;

        // We should see num_dev device instances appear on the signal stream
        if (sig_type != DEVICEINST)
            return ONI_EBADDEVTABLE;

        // Append the device onto the table
        memcpy(ctx->dev_table + i, buffer, sizeof(oni_device_t));

        // Check to see if this is the biggest frame in the table
        if ((ctx->dev_table + i)->read_size > ctx->max_read_frame_size)
            ctx->max_read_frame_size = (ctx->dev_table + i)->read_size;

        if ((ctx->dev_table + i)->write_size > ctx->max_write_frame_size)
            ctx->max_write_frame_size = (ctx->dev_table + i)->write_size;
    }

    // Clear the table
    for (i = 0; i < ctx->dev_hash_len; i++) {
        ctx->dev_hash_table[i].idx = ONI_DEVIDXNULL;
    }

    // Sort device_table
    // Check that dev_idx entries are unique (required for hash)
    // Fill the hash_table table
    for (i = 0; i < ctx->num_dev; i++) {

        // NB: i = ctx->num_dev - 1 will not loop
        size_t j;
        size_t min_idx = i;
        for (j = i + 1; j < ctx->num_dev; j++) {

            if (ctx->dev_table[i].idx == ctx->dev_table[j].idx)
                return ONI_EDEVIDXREPEAT;

            if (ctx->dev_table[j].idx < ctx->dev_table[min_idx].idx)
                min_idx = j;
        }

        // Swap place device i and min_idx
        oni_device_t temp = ctx->dev_table[min_idx];
        ctx->dev_table[min_idx] = ctx->dev_table[i];
        ctx->dev_table[i] = temp;

        // Hashing with open addressing
        size_t probe;
        for (probe = _oni_hash32(ctx->dev_table[i].idx) % ctx->dev_hash_len;
             ctx->dev_hash_table[probe].idx != ONI_DEVIDXNULL;
             probe = (probe + 1) % ctx->dev_hash_len) {
        };

        assert(ctx->dev_hash_table[probe].idx == ONI_DEVIDXNULL);
        ctx->dev_hash_table[probe] = ctx->dev_table[i];
    }

    // Add the header contents to the read size
    ctx->max_read_frame_size += ONI_RFRAMEHEADERSZ;
    ctx->max_write_frame_size += ONI_WFRAMEHEADERSZ;

    // NB: Default the block read size to a single max sized frame. This is bad
    // for high bandwidth performance and good for closed-loop delay. The opposite is true
    // for write frames (to an extent) so this is defaulted to something large.
    ctx->block_read_size = ctx->max_read_frame_size
                           + ctx->max_read_frame_size % sizeof(oni_fifo_dat_t);
    ctx->block_write_size = ctx->max_write_frame_size
                            + ctx->max_write_frame_size % sizeof(oni_fifo_dat_t);
    ctx->block_write_size = ctx->block_write_size < 4096 ? 4096 : ctx->block_write_size;

    // Set the block read size in the driver, in case it needs it
    ctx->driver.set_opt_callback(ctx->driver.ctx,
                                 ONI_OPT_BLOCKREADSIZE,
                                 &(ctx->block_read_size),
                                 sizeof(ctx->block_read_size));

    return ONI_ESUCCESS;
}

static inline int _oni_read(oni_ctx ctx, oni_read_stream_t stream, void *data, size_t size)
{
    return ctx->driver.read_stream(ctx->driver.ctx, stream, data, size);
}

static inline int _oni_write(oni_ctx ctx, oni_write_stream_t stream, const char *data, size_t size)
{
    return ctx->driver.write_stream(ctx->driver.ctx, stream, data, size);
}

static inline int _oni_read_signal_packet(oni_ctx ctx, uint8_t *buffer)
{
    // Read the next zero-delimited packet
    int i = 0;
    uint8_t curr_byte = 1;
    int bad_delim = 0;
    while (curr_byte != 0) {
        int rc = _oni_read(ctx, ONI_READ_STREAM_SIGNAL, &curr_byte, 1);
        if (rc != 1) return rc;

        if (i < 255)
            buffer[i] = curr_byte;
        else
            bad_delim = 1;

        i++;
    }

    if (bad_delim)
        return ONI_ECOBSPACK;
    else
        return --i; // Length of packet without 0 delimiter
}

static int _oni_read_signal_data(oni_ctx ctx, oni_signal_t *type, void *data, size_t size)
{
    if (type == NULL)
        return ONI_EINVALARG;

    uint8_t buffer[255] = {0};

    int pack_size = _oni_read_signal_packet(ctx, buffer);
    if (pack_size < 0) return pack_size;

    // Unstuff the packet
    int rc = _oni_cobs_unstuff(buffer, buffer, pack_size);
    if (rc < 0) return rc;

    // Remove the overhead byte and signal type
    // and make sure the buffer size is sufficient
    if ((int)size < (--pack_size - 4))
        return ONI_EBUFFERSIZE;

    // Get the type, which occupies first 4 bytes of buffer
    memcpy(type, buffer, sizeof(oni_signal_t));

    // pack_size still has overhead byte and header, so we remove those
    size_t data_size = pack_size - sizeof(oni_signal_t);
    if (size < data_size)
        return ONI_EBUFFERSIZE;

    // Copy remaining data into data buffer and update size to reflect the
    // actual data payload size
    memcpy(data, buffer + sizeof(oni_signal_t), data_size);

    return ONI_ESUCCESS;
}

static int _oni_pump_signal_type(oni_ctx ctx, int flags, oni_signal_t *type)
{
    oni_signal_t packet_type = NULLSIG;
    uint8_t buffer[255] = {0};

    do {
        int pack_size = _oni_read_signal_packet(ctx, buffer);

        if (pack_size < 1)
            continue; // Something wrong with delimiter, try again

        // Unstuff the packet (last byte is the 0, so we decrement
        int rc = _oni_cobs_unstuff(buffer, buffer, pack_size);
        if (rc < 0)
            continue; // Something wrong with packet, try again

        // Get the type, which occupies first 4 bytes of buffer
        memcpy(&packet_type, buffer, sizeof(oni_signal_t));

    } while (!(packet_type & flags));

    *type = packet_type;

    return ONI_ESUCCESS;
}

static int _oni_pump_signal_data(oni_ctx ctx, int flags, oni_signal_t *type, void *data, int size)
{
    oni_signal_t packet_type = NULLSIG;
    int pack_size = 0;
    uint8_t buffer[255] = {0};

    do {
        pack_size = _oni_read_signal_packet(ctx, buffer);

        if (pack_size < 1)
            continue; // Something wrong with delimiter, try again

        // Unstuff the packet (last byte is the 0, so we decrement
        int rc = _oni_cobs_unstuff(buffer, buffer, pack_size);
        if (rc < 0)
            continue;

        // Get the type, which occupies first 4 bytes of buffer
        memcpy(&packet_type, buffer, sizeof(oni_signal_t));

    } while (!(packet_type & flags));

    *type = packet_type;

    // pack_size still has overhead byte and header, so we remove those
    int data_size = pack_size - 1 - sizeof(oni_signal_t);
    if (size < data_size)
        return ONI_EBUFFERSIZE;

    // Copy remaining data into data buffer and update size to reflect the
    // actual data payload size
    memcpy(data, buffer + sizeof(oni_signal_t), data_size);

    return ONI_ESUCCESS;
}

static int _oni_cobs_unstuff(uint8_t *dst, const uint8_t *src, size_t size)
{
    // Minimal COBS packet is 1 overhead byte + 1 data byte
    // Maximal COBS packet is 1 overhead byte + 254 data bytes
    assert(size >= 2 && size <= 255 && "Invalid COBS packet buffer size.");

    const uint8_t *end = src + size;
    while (src < end) {
        int code = *src++;
        int i;
        for (i = 1; src < end && i < code; i++)
            *dst++ = *src++;
        if (code < 0xFF)
            *dst++ = 0;
    }

    return ONI_ESUCCESS;
}

static inline int _oni_write_config(oni_ctx ctx, oni_config_t reg, oni_reg_val_t value)
{
    return ctx->driver.write_config(ctx->driver.ctx, reg, value);
}

static inline int _oni_read_config(oni_ctx ctx, oni_config_t reg, oni_reg_val_t *value)
{
    return ctx->driver.read_config(ctx->driver.ctx, reg, value);
}

static int _oni_read_buffer(oni_ctx ctx, void **data, size_t size, int allow_refill)
{
    // Remaining bytes in buffer
    size_t remaining;
    if (ctx->shared_rbuf != NULL)
        remaining = ctx->shared_rbuf->end_pos - ctx->shared_rbuf->read_pos;
    else
        remaining = 0;

    // TODO: Is there a way to get rid of allow_refill?
    // NB: Frames must reference a single buffer, so we must refill if less
    // than max possible frame size on the first read within oni_read_frame().
    // Making this limit smaller will result in memory corruption, so don't do
    // it.  Allowing refills multiple times during one call to oni_read_frame()
    // will also cause memory corruption.
    if (remaining < ctx->max_read_frame_size && allow_refill) {

        assert(ctx->max_read_frame_size <= ctx->block_read_size &&
            "Block read size is too small given the possible read frame size.");

        // New buffer allocated, old_buffer saved
        struct oni_buf_impl *old_buffer = ctx->shared_rbuf;
        ctx->shared_rbuf = malloc(sizeof(struct oni_buf_impl));

        // Allocate data block in buffer
        ctx->shared_rbuf->buffer = malloc(remaining + ctx->block_read_size);

        // Transfer remaining data to new buffer
        if (old_buffer != NULL) {

            // Copy remaining contents into new buffer
            memcpy(ctx->shared_rbuf->buffer, old_buffer->read_pos, remaining);

            // Context releases control of old buffer
            _ref_dec(&(old_buffer->count));
        }

        // (Re)set buffer state
        ctx->shared_rbuf->count = (struct ref) {_oni_destroy_buffer, 1};
        ctx->shared_rbuf->read_pos = ctx->shared_rbuf->buffer;
        ctx->shared_rbuf->end_pos
            = ctx->shared_rbuf->buffer + remaining + ctx->block_read_size;

        // Fill the buffer with new data
        int rc = _oni_read(ctx, ONI_READ_STREAM_DATA,
                          ctx->shared_rbuf->buffer + remaining,
                          ctx->block_read_size);
        if ((size_t)rc != ctx->block_read_size) return ONI_EREADFAILURE;
    }

    // "Read" (i.e. reference) buffer and update buffer read position
    *data = ctx->shared_rbuf->read_pos;
    ctx->shared_rbuf->read_pos += size;

    return ONI_ESUCCESS;
}

static int _oni_alloc_write_buffer(oni_ctx ctx, void **data, size_t size)
{
    // Remaining bytes in buffer
    size_t remaining;
    if (ctx->shared_wbuf != NULL)
        remaining = ctx->shared_wbuf->end_pos - ctx->shared_wbuf->read_pos;
    else
        remaining = 0;

    // Size request is too large
    if (size > ctx->block_write_size) return ONI_EBADALLOC;

    if (remaining < size) {

        assert(ctx->max_write_frame_size <= ctx->block_write_size &&
            "Block write size is too small given the possible write frame size.");

        // New buffer allocated, old_buffer saved
        struct oni_buf_impl *old_buffer = ctx->shared_wbuf;
        ctx->shared_wbuf = malloc(sizeof(struct oni_buf_impl));

        // Allocate data block in buffer
        ctx->shared_wbuf->buffer = malloc(ctx->block_write_size);

        // Context releases control of old buffer
        if (old_buffer != NULL)
            _ref_dec(&(old_buffer->count));

        // (Re)set buffer state
        ctx->shared_wbuf->count = (struct ref) { _oni_destroy_buffer, 1 };
        ctx->shared_wbuf->read_pos = ctx->shared_wbuf->buffer;
        ctx->shared_wbuf->end_pos
            = ctx->shared_wbuf->buffer + ctx->block_write_size;
    }

    // "Read" (i.e. reference) buffer and update buffer read position
    *data = ctx->shared_wbuf->read_pos;
    ctx->shared_wbuf->read_pos += size;

    return ONI_ESUCCESS;
}

// NB: Allow context to release control of buffer without refilling in the case
// of restart
static void _oni_dump_buffers(oni_ctx ctx)
{
    // Trigger buffer recreation on next call to _oni_read_buffer
    if (ctx->shared_rbuf != NULL)
        ctx->shared_rbuf->read_pos = ctx->shared_rbuf->end_pos;

    if (ctx->shared_wbuf != NULL)
        ctx->shared_wbuf->read_pos = ctx->shared_wbuf->end_pos;
}

// NB: Stolen from Linux kernel. Used to get the buffer holding a given
// reference count for buffer freeing.
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

static void _oni_destroy_buffer(const struct ref *ref)
{
    struct oni_buf_impl *buf = container_of(ref, struct oni_buf_impl, count);
    free(buf->buffer);
    free(buf);
}

static inline void _ref_inc(const struct ref *ref)
{
#ifdef _WIN32
    _InterlockedIncrement((int *)&ref->count);
#else
    __sync_add_and_fetch((int *)&ref->count, 1);
#endif
}

static inline void _ref_dec(const struct ref *ref)
{
#ifdef _WIN32
    if (_InterlockedDecrement((int *)&ref->count) == 0)
#else
    if (__sync_sub_and_fetch((int *)&ref->count, 1) == 0)
#endif
        ref->free(ref);
}
