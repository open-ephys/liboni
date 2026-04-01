#ifndef __ONI_H__
#define __ONI_H__

// Version macros for compile-time API version detection
// NB: see https://semver.org/
#define ONI_VERSION_MAJOR 4
#define ONI_VERSION_MINOR 5
#define ONI_VERSION_PATCH 1

#define ONI_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ONI_VERSION \
    ONI_MAKE_VERSION(ONI_VERSION_MAJOR, ONI_VERSION_MINOR, ONI_VERSION_PATCH)

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// OS-specific definitions
#ifdef _WIN32
#ifdef LIBONI_EXPORTS
#define ONI_EXPORT __declspec(dllexport)
#else
#define ONI_EXPORT
#endif
#else
#define ONI_EXPORT
#endif

#include "onidefs.h"

// Acquisition context
typedef struct oni_ctx_impl *oni_ctx;

// Device type
typedef struct {
    // NB: Block read so don't change order
    oni_size_t idx;           // Complete rsv.rsv.hub.idx device table index
    oni_dev_id_t id;          // Device ID number
    oni_size_t version;       // Device firmware version
    oni_size_t read_size;     // Device data read size per frame in bytes
    oni_size_t write_size;    // Device data write size per frame in bytes

} oni_device_t;

// Frame type
typedef struct {
    const oni_fifo_time_t time;     // Frame time (ACQCLKHZ)
    const oni_fifo_dat_t dev_idx;   // Device index that produced or accepts the frame
    const oni_fifo_dat_t data_sz;   // Size in bytes of data buffer
    char *data;                     // Raw data block

} oni_frame_t;

// Human-readable specification version
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t reserved;
} oni_version_t;

// ONI spec 2.0 controller capabilities
typedef struct {
    oni_version_t spec_ver;
    oni_reg_val_t read_str_align;
    oni_reg_val_t write_str_align;
    oni_reg_val_t max_register_q_size;
    oni_reg_val_t num_sync_devs;
} oni_controller_caps_t;

typedef enum { 
    ONI_REG_OP_READ = 0, 
    ONI_REG_OP_WRITE = 1 
} oni_reg_optype_t;

typedef struct {
    oni_dev_idx_t dev_idx;
    oni_reg_addr_t addr;
    oni_reg_optype_t optype;
    oni_reg_val_t value;
    oni_fifo_time_t timestamp;
    oni_fifo_time_t hub_timestamp;
    int result;
} oni_reg_operation_t;

// Context management
ONI_EXPORT oni_ctx oni_create_ctx(const char *drv_name);
ONI_EXPORT int oni_init_ctx(oni_ctx ctx, int host_idx);
ONI_EXPORT int oni_destroy_ctx(oni_ctx ctx);

// Context option getting/setting
ONI_EXPORT int oni_get_opt(const oni_ctx ctx, int ctx_opt, void *value, size_t *size);
ONI_EXPORT int oni_set_opt(oni_ctx ctx, int ctx_opt, const void *value, size_t size);

// Driver option getting/setting
ONI_EXPORT int oni_get_driver_opt(const oni_ctx ctx, int drv_opt, void *value, size_t *size);
ONI_EXPORT int oni_set_driver_opt(oni_ctx ctx, int drv_opt, const void *value, size_t size);

// Hardware inspection, manipulation, and IO
ONI_EXPORT int oni_read_reg(const oni_ctx ctx, oni_dev_idx_t dev_idx, oni_reg_addr_t addr, oni_reg_val_t *value);
ONI_EXPORT int oni_write_reg(const oni_ctx ctx, oni_dev_idx_t dev_idx, oni_reg_addr_t addr, oni_reg_val_t value);
ONI_EXPORT int oni_reg_access(const oni_ctx ctx, oni_reg_operation_t* operations, int num);

ONI_EXPORT int oni_read_frame(const oni_ctx ctx, oni_frame_t **frame);
ONI_EXPORT int oni_create_frame(const oni_ctx ctx, oni_frame_t **frame, oni_dev_idx_t dev_idx, void *data, size_t data_sz);
ONI_EXPORT int oni_write_frame(const oni_ctx ctx, const oni_frame_t *frame);
ONI_EXPORT void oni_destroy_frame(oni_frame_t *frame);

// Helpers
ONI_EXPORT void oni_version(int *major, int *minor, int *patch);
ONI_EXPORT const oni_driver_info_t* oni_get_driver_info(const oni_ctx ctx);
ONI_EXPORT const char *oni_error_str(int err);
ONI_EXPORT void oni_create_reg_read_continuous(oni_reg_operation_t *ops,
                                                    oni_dev_idx_t dev_idx,
                                                    oni_reg_addr_t start,
                                                    size_t num);

ONI_EXPORT void oni_create_reg_read_sparse(oni_reg_operation_t *ops,
                                                    oni_dev_idx_t dev_idx,
                                                    oni_reg_addr_t* addresses,
                                                    size_t num);

ONI_EXPORT void oni_create_reg_write_continuous(oni_reg_operation_t *ops,
                                                oni_dev_idx_t dev_idx,
                                                oni_reg_addr_t start,
                                                oni_reg_val_t* values,
                                                size_t num);

ONI_EXPORT void oni_create_reg_write_sparse(oni_reg_operation_t *ops,
                                           oni_dev_idx_t dev_idx,
                                           oni_reg_addr_t *addresses,
                                           oni_reg_val_t *values,
                                           size_t num);

#ifdef __cplusplus
}
#endif

#endif
