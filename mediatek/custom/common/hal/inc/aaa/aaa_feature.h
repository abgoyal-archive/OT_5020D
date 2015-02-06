

#ifndef _LIB3A_AAA_FEATURE_H
#define _LIB3A_AAA_FEATURE_H

#include "ae_feature.h"
#include "af_feature.h"
#include "awb_feature.h"

// 3A module UI command set
typedef struct
{
    // 3A command
    // AE command
    LIB3A_AE_MODE_T          eAEMode;
    LIB3A_AE_EVCOMP_T        eAEComp;
    LIB3A_AE_METERING_MODE_T eAEMeterMode;
    LIB3A_AE_ISO_SPEED_T     eAEISOSpeed;
    LIB3A_AE_STROBE_MODE_T   eAEStrobeMode;
    LIB3A_AE_REDEYE_MODE_T   eAERedeyeMode;
    LIB3A_AE_FLICKER_MODE_T  eAEFlickerMode;
    int eAEFrameRateMode;
    // AF command
    LIB3A_AF_MODE_T          eAFMode;
    LIB3A_AF_METER_T         eAFMeter;
    LIB3A_AF_ZONE_T          eAFZone;
    // AWB command
    LIB3A_AWB_MODE_T         eAWBMode;
} AAA_CMD_SET_T;

#endif
