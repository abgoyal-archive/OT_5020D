

#ifndef _LIB3A_AWB_FEATURE_H
#define _LIB3A_AWB_FEATURE_H

// AWB command ID: 0x3000 ~
typedef enum
{
    LIB3A_AWB_CMD_ID_SET_AWB_MODE = 0x3000 // Set AWB Mode
} LIB3A_AWB_CMD_ID_T;

// AWB mode definition
typedef enum
{
    LIB3A_AWB_MODE_AUTO ,                // Auto white balance
    LIB3A_AWB_MODE_DAYLIGHT,             // Daylight
    LIB3A_AWB_MODE_CLOUDY_DAYLIGHT,      // Cloudy daylight
    LIB3A_AWB_MODE_SHADE,                // Shade
    LIB3A_AWB_MODE_TWILIGHT,             // Twilight
    LIB3A_AWB_MODE_FLUORESCENT,          // Fluorescent
    LIB3A_AWB_MODE_WARM_FLUORESCENT,     // Warm fluorescent
    LIB3A_AWB_MODE_INCANDESCENT,         // Incandescent
    LIB3A_AWB_MODE_GRAYWORLD,            // Gray world mode for CCT use
    LIB3A_AWB_MODE_NUM,                  // AWB mode number
    LIB3A_AWB_MODE_MIN = LIB3A_AWB_MODE_AUTO,
    LIB3A_AWB_MODE_MAX = LIB3A_AWB_MODE_GRAYWORLD
} LIB3A_AWB_MODE_T;

#endif
