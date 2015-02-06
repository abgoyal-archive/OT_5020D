

#ifndef _LIB3A_AF_FEATURE_H
#define _LIB3A_AF_FEATURE_H

// AF command ID: 0x2000 ~
typedef enum
{
    LIB3A_AF_CMD_ID_SET_AF_MODE  = 0x2000,	 // Set AF Mode
    LIB3A_AF_CMD_ID_SET_AF_METER = 0x2001,	 // Set AF Meter
    LIB3A_AF_CMD_ID_SET_AF_ZONE  = 0x2002 	 // Set AF Zone

} LIB3A_AF_CMD_ID_T;

// AF mode definition
typedef enum
{
    LIB3A_AF_MODE_OFF = 0,           // Disable AF
    LIB3A_AF_MODE_AFS,               // AF-Single Shot Mode
    LIB3A_AF_MODE_AFC,               // AF-Continuous Mode
    LIB3A_AF_MODE_AFC_VIDEO,         // AF-Continuous Mode (Video)
	LIB3A_AF_MODE_MACRO,			   // AF Macro Mode
    LIB3A_AF_MODE_INFINITY,          // Infinity Focus Mode
	LIB3A_AF_MODE_MF, 			   // Manual Focus Mode
    LIB3A_AF_MODE_CALIBRATION,       // AF Calibration Mode
	LIB3A_AF_MODE_FULLSCAN,	       // AF Full Scan Mode

    LIB3A_AF_MODE_NUM,               // AF mode number
    LIB3A_AF_MODE_MIN = LIB3A_AF_MODE_OFF,
    LIB3A_AF_MODE_MAX = LIB3A_AF_MODE_FULLSCAN

} LIB3A_AF_MODE_T;

// AF meter definition
typedef enum
{
    LIB3A_AF_METER_UNSUPPORTED = 0,
    LIB3A_AF_METER_SPOT,          	 // Spot Window
    LIB3A_AF_METER_MATRIX,       		 // Matrix Window
    LIB3A_AF_METER_FD,			     // FD Window
    LIB3A_AF_METER_MOVESPOT,           // Move Spot
    LIB3A_AF_METER_CALI,               // for CALI

    LIB3A_AF_METER_NUM,
    LIB3A_AF_METER_MIN = LIB3A_AF_METER_SPOT,
    LIB3A_AF_METER_MAX = LIB3A_AF_METER_CALI

} LIB3A_AF_METER_T;

// AF zone definition
typedef enum
{
    LIB3A_AF_ZONE_UNSUPPORTED = 0,
    LIB3A_AF_ZONE_NORMAL,            // Normal AF
    LIB3A_AF_ZONE_MACRO,             // Macro  AF

    LIB3A_AF_ZONE_NUM,
    LIB3A_AF_ZONE_MIN = LIB3A_AF_ZONE_NORMAL,
    LIB3A_AF_ZONE_MAX = LIB3A_AF_ZONE_MACRO

} LIB3A_AF_ZONE_T;

#endif
