#ifndef __ONIDEVICES_H__
#define __ONIDEVICES_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define ONI_EXPORT __declspec(dllexport)
#else
#define ONI_EXPORT
#endif

#define MAXDEVID 99999

// NB: "Officially" supported device IDs for the ONIX project occupy
// device IDs < 100,000. IDs above this value are not reserved and can be used
// for custom projects without future conflict.
// NB: If you add a device here, make sure to update oni_device_str(), and
// update documentation below
typedef enum oni_device_id {
    ONI_NULL                = 0,   // Virtual device that provides status and error information
    ONI_INFO                = 1,   // Virtual device that provides status and error information
    ONI_RHD2132             = 2,   // Intan RHD2132 bioamplifier
    ONI_RHD2164             = 3,   // Intan RHD2162 bioamplifier
    ONI_ESTIM               = 4,   // Electrical stimulation subcircuit
    ONI_OSTIM               = 5,   // Optical stimulation subcircuit
    ONI_TS4231              = 6,   // Triad semiconductor TS421 optical to digital converter
    ONI_DINPUT32            = 7,   // 32-bit digital input port
    ONI_DOUTPUT32           = 8,   // 32-bit digital output port
    ONI_BNO055              = 9,   // BNO055 9-DOF IMU
    ONI_TEST0               = 10,  // A test device used for debugging
    ONI_NEUROPIX1R0         = 11,  // Neuropixels 1.0
    ONI_HEARTBEAT           = 12,  // Host heartbeat
    ONI_AD51X2              = 13,  // AD51X2 digital potentiometer
    ONI_FMCVCTRL            = 14,  // Open Ephys FMC Host Board rev. 1.3 link voltage control subcircuit
    ONI_AD7617              = 15,  // AD7617 ADC/DAS
    ONI_AD576X              = 16,  // AD576X DAC
    ONI_TESTREG0            = 17,  // A test device used for testing remote register programming
    ONI_BREAKDIG1R3         = 18,  // Open Ephys Breakout Board rev. 1.3 digital and user IO
    ONI_FMCCLKIN1R3         = 19,  // Open Ephys FMC Host Board rev. 1.3 clock input subcircuit
    ONI_FMCCLKOUT1R3        = 20,  // Open Ephys FMC Host Board rev. 1.3 clock output subcircuit
    ONI_TS4231V2ARR         = 21,  // Triad semiconductor TS421 optical to digital converter array targeting V2 base-stations
    ONI_FMCANALOG1R3        = 22,  // Open Ephys FMC Host Board rev. 1.3 analog IO subcircuit
    ONI_FMCLINKCTRL         = 23,  // Open Ephys FMC Host Board coaxial headstage link control circuit

    // NB: Final reserved device ID. Always on bottom
    ONI_MAXDEVICEID          = MAXDEVID,

    // >= 10000: Not reserved. Free to use for custom projects
} oni_devivce_id_t;

// Human readable string from ID
ONI_EXPORT const char *oni_device_str(int dev_id);

#ifdef __cplusplus
}
#endif

#endif
