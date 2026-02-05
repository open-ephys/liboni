#ifndef __ONI_DRIVER_H__
#define __ONI_DRIVER_H__

#include <stddef.h>
#include <stdint.h>

#include "onidefs.h"

// Possible read streams
typedef enum {
    ONI_READ_STREAM_DATA = 0,
    ONI_READ_STREAM_SIGNAL
} oni_read_stream_t;

// Possible write streams.
typedef enum {
    ONI_WRITE_STREAM_DATA = 0
} oni_write_stream_t;

// Generic pointer for driver-specific options
typedef void *oni_driver_ctx;

// Registers available in the specification
typedef enum {
    // operation registers (0x0000-0x3FFF)
    ONI_OP_SOFT_RESET = 0x0000,
    ONI_OP_ACQ_RUNNING = 0x0001,
    ONI_OP_SYS_CLK_HZ = 0x0002,
    ONI_OP_ACQ_CLK_HZ = 0x0003,
    ONI_OP_ACQ_CNT_RESET = 0x0004,
    ONI_OP_SYNC_HW_ADDR = 0x0005,

    ONI_OP_RI_DEV_ADDR = 0x0006,
    ONI_OP_RI_REG_ADDR = 0x0007,
    ONI_OP_RI_REG_VAL = 0x0008,
    ONI_OP_RI_RW = 0x0009,
    ONI_OP_RI_TRIGGER = 0x000A,

    //Specification parameters (0x4000-0x7FFF)
    ONI_SPEC_VER = 0x4000,
    ONI_ATTR_READ_STR_ALIGN = 0x4001,
    ONI_ATTR_WRITE_STR_ALIGN = 0x4002,
    ONI_ATTR_MAX_REGISTER_Q_SIZE = 0x4003,
    ONI_ATTR_NUM_SYNC_DEVS = 0x4004
} oni_config_t;

// Prototype functions for drivers. Every driver has to implement these
#ifndef ONI_DRIVER_IGNORE_FUNCTION_PROTOTYPES // For use only for including in the main library driver loader
#ifdef _WIN32
#define ONI_DRIVER_EXPORT __declspec(dllexport)
#else
#define ONI_DRIVER_EXPORT
#endif

/* Driver lifetime management                                               */
/****************************************************************************/

// Creates a driver translator instance
ONI_DRIVER_EXPORT oni_driver_ctx oni_driver_create_ctx(void);
// Destroys the friver translatos instance
ONI_DRIVER_EXPORT int oni_driver_destroy_ctx(oni_driver_ctx);

// Initialize driver. Argument is the host device index
ONI_DRIVER_EXPORT int oni_driver_init(oni_driver_ctx driver_ctx, int host_idx);

/* Stream channel operations                                                */
/****************************************************************************/

// Reads from a stream channel
ONI_DRIVER_EXPORT int oni_driver_read_stream(oni_driver_ctx driver_ctx, oni_read_stream_t stream, void *data, size_t size);
// Writes to a stream channel
ONI_DRIVER_EXPORT int oni_driver_write_stream(oni_driver_ctx driver_ctx, oni_write_stream_t stream, const char *data, size_t size);

/* Configuration channel operations                                        */
/***************************************************************************/

// Reads from a configuration address
ONI_DRIVER_EXPORT int oni_driver_read_config(oni_driver_ctx driver_ctx, oni_config_t config, oni_reg_val_t *value);
// Writes to a configuration address
ONI_DRIVER_EXPORT int oni_driver_write_config(oni_driver_ctx driver_ctx, oni_config_t config, oni_reg_val_t value);

/* Device register interface                                                */
/****************************************************************************/
/* These must be called before andafter a device register access sequence   */
/* if multiple register are queued, prepare must be called before the first */
/* operation and commit after the last                                      */
/* Driver translators might have empty implementations of these if not      */
/* required by its operation                                                */
/****************************************************************************/

// Inform the driver translator that a sequence of num device register operations is
// going to be performed next
ONI_DRIVER_EXPORT int oni_driver_prepare_register_operation(oni_driver_ctx driver_ctx, size_t num);

// Commits a register sequence of device register operations
// previously started by oni_driver_prepare_register_operation
ONI_DRIVER_EXPORT int oni_driver_commit_register_operation(oni_driver_ctx driver_ctx);

// Cancels a pending register operation if there was some error
ONI_DRIVER_EXPORT void oni_driver_cancel_register_operation(oni_driver_ctx driver_ctx);

// This gets called when oni_set_opt is called. This method does not need to
// perform any configuration but it is provided for the driver to do some
// internal adjustments if required
ONI_DRIVER_EXPORT int oni_driver_set_opt_callback(oni_driver_ctx driver_ctx, int oni_option, const void *value, size_t option_len);

// Functions to get and set set driver-specific options. This kind of optiosn
// must be avoided when necessary to allow for a general interface
// Internally, thse functions can call the hardware-specific registers (0x8000-0xBFFF) if needed
ONI_DRIVER_EXPORT int oni_driver_set_opt(oni_driver_ctx driver_ctx, int driver_option, const void *value, size_t option_len);
ONI_DRIVER_EXPORT int oni_driver_get_opt(oni_driver_ctx driver_ctx, int driver_option, void *value, size_t* option_len);

// Get a string identifying the driver
ONI_DRIVER_EXPORT const oni_driver_info_t *oni_driver_info(void);

#endif

#endif
