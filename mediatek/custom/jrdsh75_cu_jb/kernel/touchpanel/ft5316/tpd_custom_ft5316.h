
#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

enum ft5316_ts_regs {
	FT5X0X_REG_THGROUP					= 0x80,     /* touch threshold, related to sensitivity */
	FT5X0X_REG_THPEAK					= 0x81,
	FT5X0X_REG_THCAL					= 0x82,
	FT5X0X_REG_THWATER					= 0x83,
	FT5X0X_REG_THTEMP					= 0x84,
	FT5X0X_REG_THDIFF					= 0x85,				
	FT5X0X_REG_CTRL						= 0x86,
	FT5X0X_REG_TIMEENTERMONITOR			= 0x87,
	FT5X0X_REG_PERIODACTIVE				= 0x88,      /* report rate */
	FT5X0X_REG_PERIODMONITOR			= 0x89,
	FT5X0X_REG_HW						= 0x8a,
	FT5X0X_REG_CHARGER_DETECT			= 0x8b,		/* Detect charger to avoid interference */
	FT5X0X_REG_DIST_MOVE				= 0x8c,
	FT5X0X_REG_DIST_POINT				= 0x8d,
	FT5X0X_REG_FEG_FRAME				= 0x8e,
	FT5X0X_REG_SINGLE_CLICK_OFFSET		= 0x8f,
	FT5X0X_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
	FT5X0X_REG_SINGLE_CLICK_TIME		= 0x91,
	FT5X0X_REG_LEFT_RIGHT_OFFSET		= 0x92,
	FT5X0X_REG_UP_DOWN_OFFSET			= 0x93,
	FT5X0X_REG_DISTANCE_LEFT_RIGHT		= 0x94,
	FT5X0X_REG_DISTANCE_UP_DOWN			= 0x95,
	FT5X0X_REG_ZOOM_DIS_SQR				= 0x96,
	FT5X0X_REG_RADIAN_VALUE				= 0x97,
	FT5X0X_REG_MAX_X_HIGH               = 0x98,
	FT5X0X_REG_MAX_X_LOW             	= 0x99,
	FT5X0X_REG_MAX_Y_HIGH            	= 0x9a,
	FT5X0X_REG_MAX_Y_LOW             	= 0x9b,
	FT5X0X_REG_K_X_HIGH            		= 0x9c,
	FT5X0X_REG_K_X_LOW             		= 0x9d,
	FT5X0X_REG_K_Y_HIGH            		= 0x9e,
	FT5X0X_REG_K_Y_LOW             		= 0x9f,
	FT5X0X_REG_AUTO_CLB_MODE			= 0xa0,
	FT5X0X_REG_LIB_VERSION_H 			= 0xa1,
	FT5X0X_REG_LIB_VERSION_L 			= 0xa2,		
	FT5X0X_REG_CIPHER					= 0xa3,
	FT5X0X_REG_MODE						= 0xa4,
	FT5X0X_REG_PMODE					= 0xa5,	  /* Power Consume Mode		*/	
	FT5X0X_REG_FIRMID					= 0xa6,   /* Firmware version */
	FT5X0X_REG_STATE					= 0xa7,
	FT5X0X_REG_FT5316ID					= 0xa8,
	FT5X0X_REG_ERR						= 0xa9,
	FT5X0X_REG_CLB						= 0xaa,
};

/* Pre-defined definition */
#define TPD_TYPE_CAPACITIVE
#define TPD_TYPE_RESISTIVE
#define TPD_POWER_SOURCE         
#define TPD_I2C_NUMBER           0
#define TPD_WAKEUP_TRIAL         60
#define TPD_WAKEUP_DELAY         100

#define TPD_VELOCITY_CUSTOM_X 12
#define TPD_VELOCITY_CUSTOM_Y 16


#define TPD_DELAY                (2*HZ/100)
//#define TPD_RES_X                480
//#define TPD_RES_Y                800
#define TPD_CALIBRATION_MATRIX  {962,0,0,0,1600,0,0,0};

//#define TPD_HAVE_CALIBRATION
//#define TPD_HAVE_BUTTON
//#define TPD_HAVE_TREMBLE_ELIMINATION
#define TPD_HAVE_BUTTON
#define TPD_BUTTON_HEIGH        (100)
#define TPD_KEY_COUNT           3
#define TPD_KEYS                { KEY_BACK, KEY_HOMEPAGE ,KEY_MENU}
#define TPD_KEYS_DIM            {{100,850,120,TPD_BUTTON_HEIGH},{240,850,120,TPD_BUTTON_HEIGH},{380,850,120,TPD_BUTTON_HEIGH}}

#endif /* TOUCHPANEL_H__ */
