#ifndef __ONIX_H__
#define __ONIX_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define ONI_EXPORT __declspec(dllexport)
#else
#define ONI_EXPORT
#endif

// Device definitions
#define MAXDEVID 99999

// NB: "Officially" supported device IDs for the ONIX project occupy
// device IDs < MAXDEVID. IDs above this value are not reserved and can be used
// for custom projects without future conflict.
// NB: If you add a device here, make sure to update oni_device_str().
enum {
    ONIX_NULL               = 0,    // Placeholder device
    ONIX_INFO               = 1,    // Virtual device that provides status and error information
    ONIX_RHD2132            = 2,    // Intan RHD2132 bioamplifier
    ONIX_RHD2164            = 3,    // Intan RHD2162 bioamplifier
    ONIX_ESTIM              = 4,    // Electrical stimulation subcircuit
    ONIX_OSTIM              = 5,    // Optical stimulation subcircuit
    ONIX_TS4231             = 6,    // Triad semiconductor TS421 optical to digital converter
    ONIX_DINPUT32           = 7,    // 32-bit digital input port
    ONIX_DOUTPUT32          = 8,    // 32-bit digital output port
    ONIX_BNO055             = 9,    // BNO055 9-DOF IMU
    ONIX_TEST0              = 10,   // A test device used for debugging
    ONIX_NEUROPIX1R0        = 11,   // Neuropixels 1.0
    ONIX_HEARTBEAT          = 12,   // Host heartbeat
    ONIX_AD51X2             = 13,   // AD51X2 digital potentiometer
    ONIX_FMCVCTRL           = 14,   // Open Ephys FMC Host Board rev. 1.3 link voltage control subcircuit
    ONIX_AD7617             = 15,   // AD7617 ADC/DAS
    ONIX_AD576X             = 16,   // AD576X DAC
    ONIX_TESTREG0           = 17,   // A test device used for testing remote register programming
    ONIX_BREAKDIG1R3        = 18,   // Open Ephys Breakout Board rev. 1.3 digital and user IO
    ONIX_FMCCLKIN1R3        = 19,   // Open Ephys FMC Host Board rev. 1.3 clock input subcircuit
    ONIX_FMCCLKOUT1R3       = 20,   // Open Ephys FMC Host Board rev. 1.3 clock output subcircuit
    ONIX_TS4231V2ARR        = 21,   // Triad semiconductor TS421 optical to digital converter array targeting V2 base-stations
    ONIX_FMCANALOG1R3       = 22,   // Open Ephys FMC Host Board rev. 1.3 analog IO subcircuit
    ONIX_FMCLINKCTRL        = 23,   // Open Ephys FMC Host Board coaxial headstage link control circuit
    ONIX_DS90UB9RAW         = 24,   // Raw DS90UB9x deserializer
    ONIX_TS4231V1ARR        = 25,   // Triad semiconductor TS421 optical to digital converter array targeting V1 base-stations
    ONIX_MAX10ADCCORE       = 26,   // Max10 internal ADC device
    ONIX_LOADTEST           = 27,   // Variable load testing device
    ONIX_MEMUSAGE           = 28,   // Acquisition hardware buffer usage reporting device
    ONIX_HARPSYNCINPUT      = 30,   // Harp synchronization data input device

    // NB: Final reserved device ID. Always on bottom
    ONIX_MAXDEVICEID        = MAXDEVID,

    // >= MAXDEVID: Not reserved. Free to use for custom projects
};

// Hub definitions
#define MAXHUBID 99999

// Each hub has a "hidden" information device with a fixed device index and 
// several read only registers
#define ONIX_HUB_DEV_IDX 254        // Device index

// Hub device register addresses
typedef enum {
    ONIX_HUB_HARDWAREID     = 0,    // Hub hardware ID
    ONIX_HUB_HARDWAREREV    = 1,    // Hub hardware revision
    ONIX_HUB_FIRMWAREVER    = 2,    // Hub firmware version
    ONIX_HUB_CLKRATEHZ      = 4,    // Hub clock rate in Hz
    ONIX_HUB_DELAYNS        = 5,    // Hub to host transmission delay in nanoseconds
};

// NB: "Officially" supported hub IDs for the ONIX project occupy
// hub IDs < MAXHUBID. IDs above this value are not reserved and can be used
// for custom projects without future conflict.
// NB: If you add a hub here, make sure to update oni_hub_str().
enum {
    ONIX_HUB_NULL           = 0,    // Placeholder hub
    ONIX_HUB_FMCHOST        = 1,    // Open Ephys FMC host
    ONIX_HUB_HS64           = 2,    // Open Ephys headstage-64
    ONIX_HUB_HSNP           = 3,    // Open Ephys headstage-neuropix1

    // NB: Final reserved hub ID. Always on bottom
    ONIX_HUB_MAXID          = MAXHUBID,

    // >= MAXHUBID: Not reserved. Free to use for custom projects
};

// ONIX Specific configuration registers
enum {
    ONIX_OPT_PASSTHROUGH = ONI_OPT_CUSTOMBEGIN,
};

// Human readable strings from IDs
ONI_EXPORT const char *onix_device_str(int dev_id);
ONI_EXPORT const char *onix_hub_str(int hub_hardware_id);

#ifdef __cplusplus
}
#endif

#endif
