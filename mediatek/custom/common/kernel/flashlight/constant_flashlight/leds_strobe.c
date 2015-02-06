#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"

#if defined (CONFIG_ARCH_MT6577)
#include <mach/mt_pwm.h>
#include <mach/pmic_mt6329_hw_bank1.h>
#include <mach/pmic_mt6329_sw_bank1.h>
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#endif

#if defined (CONFIG_ARCH_MT6575)
#include <mach/mt_pwm.h>
#include <mach/pmic_mt6329_hw_bank1.h>
#include <mach/pmic_mt6329_sw_bank1.h>
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#endif

extern void mt_power_on (U32 pwm_no);
extern void mt_power_off (U32 pwm_no);
extern S32 mt_set_pwm_enable ( U32 pwm_no ) ;
extern S32 mt_set_pwm_disable ( U32 pwm_no ) ;


/*******************************************************************************
* structure & enumeration
*******************************************************************************/
struct strobe_data{
    spinlock_t lock;
    wait_queue_head_t read_wait;
    struct semaphore sem;
};
/******************************************************************************
 * local variables
******************************************************************************/
static struct strobe_data strobe_private;

static u32 strobe_Res = 0;
static u32 strobe_Timeus = 0;
static int g_strobe_On = FALSE;
static u32 strobe_width = 100; //0 is disable
static eFlashlightState strobe_eState = FLASHLIGHTDRV_STATE_PREVIEW;

static int fl_pwm_num = PWM2;                                   //***
unsigned int fl_div = CLK_DIV4;


    ssize_t gpio_FL_Init(void) {
        return 0;
    }
    ssize_t gpio_FL_Uninit(void) {
        return 0;
    }
    ssize_t gpio_FL_Enable(void) {
		int gpio_mode;
		static struct pwm_easy_config pwm_setting;

	mt_set_gpio_mode(48, GPIO_MODE_00);
	mt_set_gpio_dir(48, GPIO_DIR_OUT);
	mt_set_gpio_out(48, GPIO_OUT_ONE);

	mt_set_gpio_mode(66, GPIO_MODE_01);

	pwm_setting.pwm_no = fl_pwm_num;
	pwm_setting.clk_div = fl_div;
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;

	pwm_setting.duty = strobe_width;
	pwm_setting.duration = 0;
	pwm_set_easy_config(&pwm_setting);

	return 0;
    }
    ssize_t gpio_FL_Disable(void) {
	mt_set_pwm_disable(fl_pwm_num);
	mt_pwm_power_off(fl_pwm_num);

	mt_set_gpio_mode(48, GPIO_MODE_00);
	mt_set_gpio_dir(48, GPIO_DIR_OUT);
	mt_set_gpio_out(48, GPIO_OUT_ZERO);
    return 0;
    }
    ssize_t gpio_FL_dim_duty(kal_uint8 duty) {
        return 0;
    }
	ssize_t gpio_TORCH_Enable(void)
	{
		static struct pwm_easy_config pwm_setting;

		mt_set_gpio_mode(48, GPIO_MODE_00);
		mt_set_gpio_dir(48, GPIO_DIR_OUT);
		mt_set_gpio_out(48, GPIO_OUT_ZERO);

		mt_set_gpio_mode(66, GPIO_MODE_01);
		pwm_setting.pwm_no = fl_pwm_num;
		pwm_setting.clk_div = fl_div;
		pwm_setting.clk_src = PWM_CLK_OLD_MODE_32K;

		pwm_setting.duty = strobe_width;
		pwm_setting.duration = 0;
		pwm_set_easy_config(&pwm_setting);

		return 0;
	}

//abstract interface
#define FL_Init gpio_FL_Init
#define FL_Uninit gpio_FL_Uninit
#define FL_Enable gpio_FL_Enable
#define FL_Disable gpio_FL_Disable
#define FL_dim_duty gpio_FL_dim_duty
#define TORCH_select gpio_TORCH_Enable

/*****************************************************************************
User interface
*****************************************************************************/
static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
    int i4RetValue = 0;
    int iFlashType = (int)FLASHLIGHT_NONE;


	printk("XXXXXXXXXXXXXXXXXXX constant_flashlight_ioctl E XXXXXXXXXXXXXXXXX\n");
    switch(cmd)
    {
    	case FLASHLIGHTIOC_T_ENABLE :
		printk("xxxxxxxxxx enable arg = %d, width = %d xxxxxxxx\n", arg, strobe_width);
            if (arg && strobe_width) {
                //enable strobe watchDog timer.
                if(FL_Enable()){
                    printk("FL_Enable fail!\n");
                    goto strobe_ioctl_error;
                }
                g_strobe_On = TRUE;
            }
            else {
                if(FL_Disable()) {
                    printk("FL_Disable fail!\n");
                    goto strobe_ioctl_error;
                }
                g_strobe_On = FALSE;
            }
    		break;
        case FLASHLIGHTIOC_T_LEVEL:
					strobe_width = 100;

				//PK_DBG("level:%d \n",(int)arg);
				if (arg > 0)
				{
					if(FL_dim_duty((kal_int8)arg-1)) {
						//PK_ERR("FL_dim_duty fail!\n");
						//i4RetValue = -EINVAL;
						goto strobe_ioctl_error;
					}
				}
            break;
        case FLASHLIGHTIOC_T_FLASHTIME:
            strobe_Timeus = (u32)arg;
            printk("strobe_Timeus:%d \n",(int)strobe_Timeus);
            break;
        case FLASHLIGHTIOC_T_STATE:
            strobe_eState = (eFlashlightState)arg;
            break;
        case FLASHLIGHTIOC_G_FLASHTYPE:
            iFlashType = FLASHLIGHT_LED_CONSTANT;
            if(copy_to_user((void __user *) arg , (void*)&iFlashType , _IOC_SIZE(cmd)))
            {
                printk("[strobe_ioctl] ioctl copy to user failed\n");
                return -EFAULT;
            }
            break;
         case FLASHLIGHTIOC_X_SET_FLASHLEVEL:
				strobe_width = arg;
                printk("level:%d \n",(int)arg);
	     break;
	case FLASHLIGHTIOC_ENABLE_STATUS:
//		printk("**********g_strobe_on = %d \n", g_strobe_On);
		copy_to_user((void __user *) arg , (void*)&g_strobe_On , sizeof(int));
		break;
	case FLASHLIGHT_TORCH_SELECT:
		printk("@@@@@@FLASHLIGHT_TORCH_SELECT@@@@@@\n");
		if (arg && strobe_width){
			TORCH_select();
			g_strobe_On = TRUE;
		}
		else {
			if(FL_Disable()) {
			 printk("FL_Disable fail!\n");
			goto strobe_ioctl_error;
                }
                g_strobe_On = FALSE;
            }

		break;
    	default :
			printk("No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }

	printk("XXXXXXXXXXXXXXXXXXX constant_flashlight_ioctl X XXXXXXXXXXXXXXXXX\n");
    return i4RetValue;

strobe_ioctl_error:
    printk("Error or ON state!\n");
    return -EPERM;
}

static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    if (0 == strobe_Res) {
        FL_Init();

		//enable HW here if necessary
        if(FL_dim_duty(0)) {
            //0(weak)~31(strong)
            printk("FL_dim_duty fail!\n");
            i4RetValue = -EINVAL;
        }
    }

    spin_lock_irq(&strobe_private.lock);

    if(strobe_Res)
    {
        printk("busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }

    //LED On Status
    //g_strobe_On = FALSE;

    spin_unlock_irq(&strobe_private.lock);

	printk("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX constant_flashlight_open XXXXXXXXXXXXXXXXXXXXX\n");
    return i4RetValue;
}

static int constant_flashlight_release(void *pArg)
{
    if (strobe_Res)
    {
        spin_lock_irq(&strobe_private.lock);

        strobe_Res = 0;
        strobe_Timeus = 0;

        //LED On Status
        //g_strobe_On = FALSE;

        spin_unlock_irq(&strobe_private.lock);

	//un-init. HW here if necessary
        if(FL_dim_duty(0)) {
            printk("FL_dim_duty fail!\n");
        }

	FL_Uninit();
    }

    return 0;
}

FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl,
};

MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc) {
    if (pfFunc!=NULL) {
        *pfFunc=&constantFlashlightFunc;
    }
	printk("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX constantFlashlightInit XXXXXXXXXXXXXXXXXXXXX\n");
    return 0;
}

ssize_t strobe_VDIrq(void)
{
	return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);
