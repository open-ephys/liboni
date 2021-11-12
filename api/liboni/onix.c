#include <assert.h>

#include "oni.h"
#include "onix.h"

const char *onix_device_str(int dev_id)
{
    switch (dev_id) {
        case ONIX_NULL: {
            return "Placeholder device: neither generates or accepts data";
        }
        case ONIX_INFO: {
            return "Host status and error information";
        }
        case ONIX_RHD2132: {
            return "Intan RHD2132 bioamplifier";
        }
        case ONIX_RHD2164: {
            return "Intan RHD2164 bioamplifier";
        }
        case ONIX_ESTIM: {
            return "Electrical stimulation subcircuit";
        }
        case ONIX_OSTIM: {
            return "Optical stimulation subcircuit";
        }
        case ONIX_TS4231: {
            return "Triad TS4231 optical to digital converter";
        }
        case ONIX_DINPUT32: {
            return "32-bit digital input port";
        }
        case ONIX_DOUTPUT32: {
            return "32-bit digital output port";
        }
        case ONIX_BNO055: {
            return "BNO055 9-axis IMU";
        }
        case ONIX_TEST0: {
            return "Open Ephys test device";
        }
        case ONIX_NEUROPIX1R0: {
            return "Neuropixels 1.0 probe";
        }
        case ONIX_HEARTBEAT: {
            return "Heartbeat";
        }
        case ONIX_AD51X2: {
            return "AD51X2 digital potentiometer";
        }
        case ONIX_FMCVCTRL: {
            return "Open Ephys FMC Host Board rev. 1.3 link voltage control "
                   "subcircuit";
        }
        case ONIX_AD7617: {
            return "AD7617 ADC/DAS";
        }
        case ONIX_AD576X: {
            return "AD576x DAC";
        }
        case ONIX_TESTREG0: {
            return "A test device used for testing remote register programming";
        }
        case ONIX_BREAKDIG1R3: {
            return "Open Ephys Breakout Board rev. 1.3 digital and user IO";
        }
        case ONIX_FMCCLKIN1R3: {
            return "Open Ephys FMC Host Board rev. 1.3 clock intput subcircuit";
        }
        case ONIX_FMCCLKOUT1R3: {
            return "Open Ephys FMC Host Board rev. 1.3 clock output subcircuit";
        }
        case ONIX_TS4231V2ARR: {
            return "Triad TS421 optical to digital converter array for V2 base "
                   "stations";
        }
        case ONIX_FMCANALOG1R3: {
            return "Open Ephys FMC Host Board rev. 1.3 analog IO subcircuit";
        }
        case ONIX_FMCLINKCTRL: {
            return "Open Ephys FMC Host Board coaxial headstage link control "
                   "circuit";
        }
        case ONIX_DS90UB9RAW: {
            return "Raw DS90UB9x deserializer";
        }
        case ONIX_TS4231V1ARR: {
            return "Triad TS421 optical to digital converter array for V1 base "
                   "stations";
        }
        case ONIX_MAX10ADCCORE: {
            return "Max10 internal ADC device";
        }
        case ONIX_LOADTEST: {
            return "Variable load testing device";
        }
        case ONIX_MEMUSAGE: {
            return "Acquisition hardware buffer usage reporting device";
        }
        case ONIX_HARPSYNCINPUT: {
            return "HARP Synchronization time input";
        }
        default:
            return "Unknown device";
    }
}

const char *onix_hub_str(int dev_id)
{
    switch (dev_id) {
        case ONIX_HUB_NULL: {
            return "Placeholder hub";
        }
        case ONIX_HUB_FMCHOST: {
            return "Open Ephys FMC host";
        }
        case ONIX_HUB_HS64: {
            return "Open Ephys headstage-64";
        }
        case ONIX_HUB_HSNP: {
            return "Open Ephys headstage-neuropix1";
        }
        default:
            return "Unknown hub";
    }
}
