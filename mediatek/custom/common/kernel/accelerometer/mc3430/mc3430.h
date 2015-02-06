
#ifndef MC3430_H
#define MC3430_H
	 
#include <linux/ioctl.h>
	 
#define MC3430_I2C_SLAVE_ADDR		0x98  
	 
	 /* MC3430 Register Map  (Please refer to MC3430 Specifications) */
#define MC3430_REG_INT_ENABLE		0x06   
#define MC3430_REG_POWER_CTL		0x07   
#define MC3430_WAKE_MODE		       0x01   
#define MC3430_STANDBY_MODE		0x03   
#define MC3430_REG_DATAX0			0x00   
#define MC3430_REG_DEVID			0x18   
#define MC3430_REG_DATA_FORMAT	0x20  
#define MC3430_RANGE_MUSTWRITE     0x03   
#define MC3430_RANGE_MASK			0x0C   
#define MC3430_RANGE_2G			0x00  
#define MC3430_RANGE_4G			0x04   
#define MC3430_RANGE_8G			0x08   
#define MC3430_RANGE_8G_14BIT		0x0C  
#define MC3430_REG_BW_RATE	       0x20  
#define MC3430_BW_MASK			0x70  
#define MC3430_BW_512HZ			0x00   
#define MC3430_BW_256HZ			0x10 
#define MC3430_BW_128HZ			0x20 
#define MC3430_BW_64HZ				0x30   

#define MC3430_FIXED_DEVID			0x88
	 
#define MC3430_SUCCESS						 0
#define MC3430_ERR_I2C						-1
#define MC3430_ERR_STATUS					-3
#define MC3430_ERR_SETUP_FAILURE			-4
#define MC3430_ERR_GETGSENSORDATA	       -5
#define MC3430_ERR_IDENTIFICATION			-6
	 
	 
#define MC3430_BUFSIZE				256
	 
#endif

