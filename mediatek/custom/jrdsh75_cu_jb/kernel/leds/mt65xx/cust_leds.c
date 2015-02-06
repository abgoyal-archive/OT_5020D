#include <cust_leds.h>
#include <mach/mt_pwm.h>

#include <linux/kernel.h>
#include <mach/pmic_mt6329_hw_bank1.h> 
#include <mach/pmic_mt6329_sw_bank1.h> 
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>

extern int mtkfb_set_backlight_level(unsigned int level);
extern int mtkfb_set_backlight_pwm(int div);

unsigned int brightness_mapping(unsigned int level)
{
    unsigned int mapped_level;
    
	if (level < 35)
		mapped_level = 13;//limit min backlight duty again by zhiping.liu 13.03.22
	else
		mapped_level = 6 + level/5;
       
	return mapped_level;
}

unsigned int Cust_SetBacklight(int level, int div)
{
    mtkfb_set_backlight_pwm(div);
    mtkfb_set_backlight_level(brightness_mapping(level));
    return 0;
}

static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
//2013-01-09 huanhuan.guo "Enable led flicker when battery is low" begin
#if defined(JRD_CAMRY_BORAD)
	{"red",               MT65XX_LED_MODE_NONE, -1,{0}},
#else
	{"red",               MT65XX_LED_MODE_PWM, PWM1,{0}},
#endif
//2013-01-09 huanhuan.guo "Enable led flicker when battery is low" end
	{"green",             MT65XX_LED_MODE_NONE, -1,{0}},
	{"blue",              MT65XX_LED_MODE_NONE, -1,{0}},
	{"jogball-backlight", MT65XX_LED_MODE_NONE, -1,{0}},
	{"keyboard-backlight",MT65XX_LED_MODE_NONE, -1,{0}},
	{"button-backlight",  MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_BUTTON,{0}},
	{"lcd-backlight",     MT65XX_LED_MODE_PWM, PWM3,{1,CLK_DIV8,5,5}},
};

struct cust_mt65xx_led *get_cust_led_list(void)
{
	return cust_led_list;
}

