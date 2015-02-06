#include <linux/types.h>
#include <mach/mt6575_pm_ldo.h>
#include <cust_alsps.h>
//#include <mach/mt6575_pm_ldo.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 0,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},
    // tao.zhang modify 2013-01-25. bug:371809 start.
    .als_level  = { 2,  5,  10, 20,  50, 100, 200, 400,  800, 1600, 2000, 2500,  3500,  5000, 10000, 65535},
    .als_value  = {10, 20, 40, 90, 160, 225, 320, 640, 1280, 2600, 3500, 9000, 11000, 20000, 30240, 30240},
    // tao.zhang modify 2013-01-25. bug:371809 end.
    .ps_threshold_high = 600,
    .ps_threshold_low = 500,
    .ps_threshold = 900,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}


#if 1
int TMD2771_CMM_PPCOUNT_VALUE = 0x07; 

int ZOOM_TIME = 10;
int TMD2771_CMM_CONTROL_VALUE = (0x20 | 0x40);
int TMD2771_OFFSET_VALUE = 0x00;
#endif




