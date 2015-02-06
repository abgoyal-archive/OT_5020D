
//tao.zhang@tcl.com modify code refer to beetle
//http://10.128.161.91/gitweb-mtk6575/?p=mediatek.git;a=tree;f=custom/jrdsh75_beetlelite_jb/kernel/alsps/tmd2771;h=3c47fdff5ac5c5389f776f9842af7faba568e402;hb=41c14cce1e61dfff3b95cf69a5da895325abf75c

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#ifdef MT6516
#include <mach/mt6516_devs.h>
#include <mach/mt6516_typedefs.h>
#include <mach/mt6516_gpio.h>
#include <mach/mt6516_pll.h>
#endif

#ifdef MT6573
#include <mach/mt6573_devs.h>
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_gpio.h>
#include <mach/mt6573_pll.h>
#endif

#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#ifdef MT6577
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#ifdef MT6516
#define POWER_NONE_MACRO MT6516_POWER_NONE
#endif

#ifdef MT6573
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6575
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6577
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "tmd2772.h"
/*----------------------------------------------------------------------------*/

#define JOHN_ADD_ATTR 1

#define TMD2771_DEV_NAME     "TMD2771"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)


#define RUBBER_NO_045 0  // for 0.45 rubber
#define RUBBER_NO_070 1  // for 0.7 rubber

u8 rubber_deep_no;
/*for interrup work mode support --add by liaoxl.lenovo 12.08.2011*/
#ifdef MT6575
	extern void mt65xx_eint_unmask(unsigned int line);
	extern void mt65xx_eint_mask(unsigned int line);
	extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
	extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
	extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
	extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
										 kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
										 kal_bool auto_umask);

#endif

#ifdef MT6577
	extern void mt65xx_eint_unmask(unsigned int line);
	extern void mt65xx_eint_mask(unsigned int line);
	extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
	extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
	extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
	extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
										 kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
										 kal_bool auto_umask);

#endif

#ifdef MT6516
extern void MT6516_EINTIRQUnmask(unsigned int line);
extern void MT6516_EINTIRQMask(unsigned int line);
extern void MT6516_EINT_Set_Polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void MT6516_EINT_Set_HW_Debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 MT6516_EINT_Set_Sensitivity(kal_uint8 eintno, kal_bool sens);
extern void MT6516_EINT_Registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif
/*----------------------------------------------------------------------------*/
static struct i2c_client *tmd2771_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id tmd2771_i2c_id[] = {{TMD2771_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_TMD2771={ I2C_BOARD_INFO("TMD2771", (0X72>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short tmd2771_force[] = {0x02, 0X72, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const tmd2771_forces[] = { tmd2771_force, NULL };
//static struct i2c_client_address_data tmd2771_addr_data = { .forces = tmd2771_forces,};
/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tmd2771_i2c_remove(struct i2c_client *client);
static int tmd2771_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int tmd2771_i2c_resume(struct i2c_client *client);

static struct tmd2771_priv *g_tmd2771_ptr = NULL;

int  boot_ps_ok=0;
struct PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
    int mean;
    u8 offset;	//add for boot cali
};

static struct PS_CALI_DATA_STRUCT ps_cali={{0,0,0},};
static struct PS_CALI_DATA_STRUCT boot_ps_cali={{0,0,0},};

static int intr_flag_value = 0;

#define USE_CALIBRATION_ON_CALL
static int tmd2771_read_data_for_cali(struct i2c_client *client, struct PS_CALI_DATA_STRUCT *ps_data_cali,  bool boot);
int  tmd2771_prx_offset_set(struct i2c_client *client, u8 *data);
static int tmd2771_read_data_mean(struct i2c_client *client, int cnt);
static int tmd2771_cali_psensor(struct PS_CALI_DATA_STRUCT *ps_data_cali, bool boot);
int  tmd2771_prx_offset_get(struct i2c_client *client, u8 *data);



/*----------------------------------------------------------------------------*/
typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;
/*----------------------------------------------------------------------------*/
struct tmd2771_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;
    u8  ps_thd;     /*PS INT threshold*/
};
/*----------------------------------------------------------------------------*/
struct tmd2771_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;

    /*i2c address group*/
    struct tmd2771_i2c_addr  addr;

    /*misc*/
    u16		    als_modulus;
    atomic_t    i2c_retry;
    atomic_t    als_suspend;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    atomic_t    ps_mask;        /*mask ps: always return far away*/
    atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    atomic_t    ps_suspend;


    /*data*/
    u16         als;
    u16          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
	atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver tmd2771_i2c_driver = {
	.probe      = tmd2771_i2c_probe,
	.remove     = tmd2771_i2c_remove,
	.detect     = tmd2771_i2c_detect,
	.suspend    = tmd2771_i2c_suspend,
	.resume     = tmd2771_i2c_resume,
	.id_table   = tmd2771_i2c_id,
	//.address_data = &tmd2771_addr_data,
	.driver = {
		//.owner          = THIS_MODULE,
		.name           = TMD2771_DEV_NAME,
	},
};

static struct tmd2771_priv *tmd2771_obj = NULL;
static struct platform_driver tmd2771_alsps_driver;
/*----------------------------------------------------------------------------*/
int tmd2771_get_addr(struct alsps_hw *hw, struct tmd2771_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}
/*----------------------------------------------------------------------------*/
static void tmd2771_power(struct alsps_hw *hw, unsigned int on)
{
	static unsigned int power_on = 0;

	//APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "TMD2771"))
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "TMD2771"))
			{
				APS_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/
static long tmd2771_enable_als(struct i2c_client *client, int enable)
{
		struct tmd2771_priv *obj = i2c_get_clientdata(client);
		u8 databuf[2];	
		long res = 0;
		u8 buffer[1];
		u8 reg_value[1];
		uint32_t testbit_PS;


		if(client == NULL)
		{
			APS_DBG("CLIENT CANN'T EQUL NULL\n");
			return -1;
		}

		#if 0	/*yucong MTK enable_als function modified for fixing reading register error problem 2012.2.16*/
		buffer[0]=TMD2771_CMM_ENABLE;
		res = i2c_master_send(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		res = i2c_master_recv(client, reg_value, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);

		if(enable)
		{
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = reg_value[0] |0x0B;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			/*Lenovo-sw chenlj2 add 2011-06-03,modify ps to ALS below two lines */
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
			APS_DBG("tmd2771 power on\n");
		}
		else
		{
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = reg_value[0] &0xFD;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			/*Lenovo-sw chenlj2 add 2011-06-03,modify ps_deb_on to als_deb_on */
			atomic_set(&obj->als_deb_on, 0);
			APS_DBG("tmd2771 power off\n");
		}
		#endif
		#if 1
		/*yucong MTK enable_als function modified for fixing reading register error problem 2012.2.16*/
		testbit_PS = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
		if(enable)
		{
			if(testbit_PS){
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x2F;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			/*debug code for reading register value*/
			#if 0
			res = i2c_master_recv(client, reg_value, 0x1);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
			#endif
			}
			else{
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x2B;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}

			/*debug code for reading register value*/
			#if 0
			res = i2c_master_recv(client, reg_value, 0x1);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
			#endif

			}
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
			APS_DBG("tmd2771 power on\n");
		}
		else
		{
			if(testbit_PS){
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x2D;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			}
			else{
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x00;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			}
			/*Lenovo-sw chenlj2 add 2011-06-03,modify ps_deb_on to als_deb_on */
			atomic_set(&obj->als_deb_on, 0);
			APS_DBG("tmd2771 power off\n");
		}
		#endif
		#if 0 /*yucong add for debug*/
			buffer[0]=TMD2771_CMM_ENABLE;
			res = i2c_master_send(client, buffer, 0x1);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			res = i2c_master_recv(client, reg_value, 0x1);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
		#endif

		return 0;

	EXIT_ERR:
		APS_ERR("tmd2771_enable_als fail\n");
		return res;
}

/*----------------------------------------------------------------------------*/
static long tmd2771_enable_ps(struct i2c_client *client, int enable)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	long res = 0;
	u8 buffer[1];
	u8 reg_value[1];
	uint32_t testbit_ALS;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
#if 0	/*yucong MTK modified for fixing reading register error problem 2012.2.16*/
	buffer[0]=TMD2771_CMM_ENABLE;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, reg_value, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	/*yucong MTK: lenovo orignal code*/
	if(enable)
	{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = reg_value[0] |0x0d;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		APS_DBG("tmd2771 power on\n");

		/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
		if(0 == obj->hw->polling_mode_ps)
		{
			if(1 == ps_cali.valid)
			{
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				databuf[1] = (u8)(ps_cali.far_away & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				databuf[1] = (u8)((ps_cali.far_away & 0xFF00) >> 8);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				databuf[1] = (u8)(ps_cali.close & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				databuf[1] = (u8)((ps_cali.close & 0xFF00) >> 8);;
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
			}
			else
			{
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				databuf[1] = (u8)(480 & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				databuf[1] = (u8)((480 & 0xFF00) >> 8);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				databuf[1] = (u8)(700 & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				databuf[1] = (u8)((700 & 0xFF00) >> 8);;
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}

			}

			databuf[0] = TMD2771_CMM_Persistence;
			databuf[1] = 0x20;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = reg_value[0] | 0x0d | 0x20;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}

			mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
		}
	}
	else
	{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = reg_value[0] &0xfb;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
		APS_DBG("tmd2771 power off\n");

		/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
		if(0 == obj->hw->polling_mode_ps)
		{
			cancel_work_sync(&obj->eint_work);
			mt65xx_eint_mask(CUST_EINT_ALS_NUM);
		}
	}
#endif
#if 1
	/*yucong MTK: enable_ps function modified for fixing reading register error problem 2012.2.16*/
	testbit_ALS = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
	if(enable)
	{
		if(testbit_ALS){
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x0F;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		/*debug code for reading register value*/
		#if 0
		res = i2c_master_recv(client, reg_value, 0x1);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
		#endif
		}else{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x0D;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		/*debug code for reading register value*/
		#if 0
		res = i2c_master_recv(client, reg_value, 0x1);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
		#endif
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		APS_DBG("tmd2771 power on\n");

		/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
		if(0 == obj->hw->polling_mode_ps)
		{
			//if(1 == ps_cali.valid)
			{
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				databuf[1] = (u8)(ps_cali.far_away & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				databuf[1] = (u8)((ps_cali.far_away & 0xFF00) >> 8);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				databuf[1] = (u8)(ps_cali.close & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				databuf[1] = (u8)((ps_cali.close & 0xFF00) >> 8);;
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
			}
			#if 0
			else
			{
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				databuf[1] = (u8)(750 & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				databuf[1] = (u8)((750 & 0xFF00) >> 8);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				databuf[1] = (u8)(900 & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				databuf[1] = (u8)((900 & 0xFF00) >> 8);;
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return TMD2771_ERR_I2C;
				}

			}
			#endif
		
			databuf[0] = TMD2771_CMM_Persistence;
			databuf[1] = 0x20;//0x50; //modify by zy PR350462
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			if(testbit_ALS){
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x2F;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			/*debug code for reading register value*/
			#if 0
			res = i2c_master_recv(client, reg_value, 0x1);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
			#endif
			}else{
			databuf[0] = TMD2771_CMM_ENABLE;
			databuf[1] = 0x2D;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			}
			/*debug code for reading register value*/
			#if 0
			res = i2c_master_recv(client, reg_value, 0x1);
			if(res <= 0)
				{
					goto EXIT_ERR;
				}
			printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
			#endif

			mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
		}
	}
	else
	{
	/*yucong MTK: enable_ps function modified for fixing reading register error problem 2012.2.16*/
	if(testbit_ALS){
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x2B;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}else{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x00;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		atomic_set(&obj->ps_deb_on, 0);
		APS_DBG("tmd2771 power off\n");

		/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
		if(0 == obj->hw->polling_mode_ps)
		{
			cancel_work_sync(&obj->eint_work);
			mt65xx_eint_mask(CUST_EINT_ALS_NUM);
		}
	}
#endif
	return 0;

EXIT_ERR:
	APS_ERR("tmd2771_enable_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static int tmd2771_enable(struct i2c_client *client, int enable)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;
	u8 buffer[1];
	u8 reg_value[1];

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	/* modify to restore reg setting after cali ---liaoxl.lenovo */
	buffer[0]=TMD2771_CMM_ENABLE;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, reg_value, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	if(enable)
	{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = reg_value[0] | 0x01;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		APS_DBG("tmd2771 power on\n");
	}
	else
	{
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = reg_value[0] & 0xFE;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
		/*Lenovo-sw chenlj2 add 2011-06-03,close als_deb_on */
		atomic_set(&obj->als_deb_on, 0);
		APS_DBG("tmd2771 power off\n");
	}
	return 0;

EXIT_ERR:
	APS_ERR("tmd2771_enable fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static int tmd2771_check_and_clear_intr(struct i2c_client *client)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	int res,intp,intl;
	u8 buffer[2];

	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/
	//    return 0;

	buffer[0] = TMD2771_CMM_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//printk("yucong tmd2771_check_and_clear_intr status=0x%x\n", buffer[0]);
	res = 1;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x20))
	{
		res = 0;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x10))
	{
		res = 0;
		intl = 1;
	}

	if(0 == res)
	{
		if((1 == intp) && (0 == intl))
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x05);
		}
		else if((0 == intp) && (1 == intl))
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x06);
		}
		else
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x07);
		}
		res = i2c_master_send(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		else
		{
			res = 0;
		}
	}

	return res;

EXIT_ERR:
	APS_ERR("tmd2771_check_and_clear_intr fail\n");
	return 1;
}
/*----------------------------------------------------------------------------*/

/*yucong add for interrupt mode support MTK inc 2012.3.7*/
static int tmd2771_check_intr(struct i2c_client *client)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	int res,intp,intl;
	u8 buffer[2];

	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/
	//    return 0;

	buffer[0] = TMD2771_CMM_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//APS_ERR("tmd2771_check_and_clear_intr status=0x%x\n", buffer[0]);
	res = 1;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x20))
	{
		res = 0;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x10))
	{
		res = 0;
		intl = 1;
	}

	return res;

EXIT_ERR:
	APS_ERR("tmd2771_check_intr fail\n");
	return 1;
}

static int tmd2771_clear_intr(struct i2c_client *client)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 buffer[2];

#if 0
	if((1 == intp) && (0 == intl))
	{
		buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x05);
	}
	else if((0 == intp) && (1 == intl))
	{
		buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x06);
	}
	else
#endif
	{
		buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x07);
	}
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	else
	{
		res = 0;
	}

	return res;

EXIT_ERR:
	APS_ERR("tmd2771_check_and_clear_intr fail\n");
	return 1;
}


/*-----------------------------------------------------------------------------*/
void tmd2771_eint_func(void)
{
	struct tmd2771_priv *obj = g_tmd2771_ptr;
	if(!obj)
	{
		return;
	}

	schedule_work(&obj->eint_work);
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
int tmd2771_setup_eint(struct i2c_client *client)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);

	g_tmd2771_ptr = obj;

	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_ALS_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_ALS_POLARITY, tmd2771_eint_func, 0);

	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
    return 0;
}

/*----------------------------------------------------------------------------*/

static int tmd2771_init_client_for_cali(struct i2c_client *client)
{

	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;
#if 0
	u8 id = 0;
   //---------read id:

	databuf[0]=0x92;
	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, &id, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
       APS_DBG("***id = %d\n", id);
#endif

	databuf[0] = TMD2771_CMM_ENABLE;
	databuf[1] = 0x01;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_ATIME;
	databuf[1] = 0xFF;//0xEE
	//databuf[1] = 0xEE;//0xEE
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_PTIME;
	databuf[1] = 0xFF;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_WTIME;
	databuf[1] = 0xFF;//0xFF
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

#if MIN_CURRENT_MODE

	databuf[0] = TMD2771_CMM_CONFIG;
	databuf[1] = 0x01;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#else

	databuf[0] = TMD2771_CMM_CONFIG;
	databuf[1] = 0x00;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#endif

	databuf[0] = TMD2771_CMM_PPCOUNT;
	databuf[1] = TMD2771_CMM_PPCOUNT_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

#if MIN_CURRENT_MODE

	databuf[0] = TMD2771_CMM_CONTROL;
	databuf[1] = TMD2771_CMM_CONTROL_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#else

	databuf[0] = TMD2771_CMM_CONTROL;
	databuf[1] = TMD2771_CMM_CONTROL_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#endif

	databuf[0] = TMD2771_CMM_ENABLE;
	databuf[1] = 0x05;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	return TMD2771_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;

}

static int tmd2771_init_client(struct i2c_client *client)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

	databuf[0] = TMD2771_CMM_ENABLE;
	databuf[1] = 0x00;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_ATIME;
	databuf[1] = 0xFF;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_PTIME;
	databuf[1] = 0xFF;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

	databuf[0] = TMD2771_CMM_WTIME;
	databuf[1] = 0xEE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(0 == obj->hw->polling_mode_ps)
	{
		//if(1 == ps_cali.valid)
		{
			databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8)(ps_cali.far_away & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8)((ps_cali.far_away & 0xFF00) >> 8);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8)(ps_cali.close & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8)((ps_cali.close & 0xFF00) >> 8);;
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
		}
		#if 0
		else
		{
			databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
			databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_low) & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low) & 0xFF00) >> 8);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
			databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_high) & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high) & 0xFF00) >> 8);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
				return TMD2771_ERR_I2C;
			}

		}
		#endif
#if 0
		databuf[0] = TMD2771_CMM_Persistence;
		databuf[1] = 0x20;//0x50;//modify by zy PR350462
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return TMD2771_ERR_I2C;
		}
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x20;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return TMD2771_ERR_I2C;
		}
#endif
	}

		databuf[0] = TMD2771_CMM_Persistence;
		databuf[1] = 0x20;//0x50; //modify by zy PR350462
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return TMD2771_ERR_I2C;
		}
		databuf[0] = TMD2771_CMM_ENABLE;
		databuf[1] = 0x20;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return TMD2771_ERR_I2C;
		}

#if MIN_CURRENT_MODE
	databuf[0] = TMD2771_CMM_CONFIG;
	databuf[1] = 0x01;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#else
	databuf[0] = TMD2771_CMM_CONFIG;
	databuf[1] = 0x00;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#endif

       /*Lenovo-sw chenlj2 add 2011-06-03,modified pulse 2  to 4 */
	databuf[0] = TMD2771_CMM_PPCOUNT;
	databuf[1] = TMD2771_CMM_PPCOUNT_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}

#if MIN_CURRENT_MODE
        /*Lenovo-sw chenlj2 add 2011-06-03,modified gain 16  to 1 */
	databuf[0] = TMD2771_CMM_CONTROL;
	databuf[1] = TMD2771_CMM_CONTROL_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#else
	databuf[0] = TMD2771_CMM_CONTROL;
	databuf[1] = TMD2771_CMM_CONTROL_VALUE;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
		return TMD2771_ERR_I2C;
	}
#endif

	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(res = tmd2771_setup_eint(client))
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	if(res = tmd2771_check_and_clear_intr(client))
	{
		APS_ERR("check/clear intr: %d\n", res);
		//    return res;
	}

	return TMD2771_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}

int tmd2771_read_als(struct i2c_client *client, u16 *data)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);	
	u16 c0_value, c1_value;	
	u32 c0_nf, c1_nf;
	u8 als_value_low[1], als_value_high[1];
	u8 buffer[1];
	u16 atio;
	u16 als_value;
	int res = 0;
	u8 reg_value[1];

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	/*debug tag for yucong*/
	#if 0
	buffer[0]=TMD2771_CMM_ENABLE;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, reg_value, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	printk("Yucong:0x%x, %d, %s\n", reg_value[0], __LINE__, __FUNCTION__);
	#endif
//get adc channel 0 value
	buffer[0]=TMD2771_CMM_C0DATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//printk("yucong: TMD2771_CMM_C0DATA_L = 0x%x\n", als_value_low[0]);

	buffer[0]=TMD2771_CMM_C0DATA_H;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//printk("yucong: TMD2771_CMM_C0DATA_H = 0x%x\n", als_value_high[0]);
	c0_value = als_value_low[0] | (als_value_high[0]<<8);
	c0_nf = obj->als_modulus*c0_value;
	//APS_DBG("c0_value=%d, c0_nf=%d, als_modulus=%d\n", c0_value, c0_nf, obj->als_modulus);

//get adc channel 1 value
	buffer[0]=TMD2771_CMM_C1DATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//printk("yucong: TMD2771_CMM_C1DATA_L = 0x%x\n", als_value_low[0]);

	buffer[0]=TMD2771_CMM_C1DATA_H;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	//printk("yucong: TMD2771_CMM_C1DATA_H = 0x%x\n", als_value_high[0]);

	c1_value = als_value_low[0] | (als_value_high[0]<<8);
	c1_nf = obj->als_modulus*c1_value;
	//APS_DBG("c1_value=%d, c1_nf=%d, als_modulus=%d\n", c1_value, c1_nf, obj->als_modulus);

	if((c0_value > c1_value) &&(c0_value < 50000))
	{  	/*Lenovo-sw chenlj2 add 2011-06-03,add {*/
		atio = (c1_nf*100)/c0_nf;

		//APS_DBG("atio = %d\n", atio);
		if(atio<30)
		{
			*data = (13*c0_nf - 24*c1_nf)/10000;
		}
		else if(atio>= 30 && atio<38) /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
		{
			*data = (16*c0_nf - 35*c1_nf)/10000;
		}
		else if(atio>= 38 && atio<45)  /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
		{
			*data = (9*c0_nf - 17*c1_nf)/10000;
		}
		else if(atio>= 45 && atio<54) /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
		{
			*data = (6*c0_nf - 10*c1_nf)/10000;
		}
		else
			*data = 0;
	/*Lenovo-sw chenlj2 add 2011-06-03,add }*/
    }
	else if (c0_value > 50000)
	{
		*data = 65535;
	}
	else if(c0_value == 0)
        {
                *data = 0;
        }
        else
	{
		APS_DBG("als_value is invalid!!\n");
		return -1;
	}
	APS_DBG("als_value_lux = %d\n", *data);
	//printk("yucong: als_value_lux = %d\n", *data);
	return 0;	



EXIT_ERR:
	APS_ERR("tmd2771_read_ps fail\n");
	return res;
}
int tmd2771_read_als_ch0(struct i2c_client *client, u16 *data)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);	
	u16 c0_value;	
	u8 als_value_low[1], als_value_high[1];
	u8 buffer[1];
	int res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
//get adc channel 0 value
	buffer[0]=TMD2771_CMM_C0DATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	buffer[0]=TMD2771_CMM_C0DATA_H;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	c0_value = als_value_low[0] | (als_value_high[0]<<8);
	*data = c0_value;
	return 0;	



EXIT_ERR:
	APS_ERR("tmd2771_read_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/

static int tmd2771_get_als_value(struct tmd2771_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	for(idx = 0; idx < obj->als_level_num; idx++)
	{
		if(als < obj->hw->als_level[idx])
		{
			break;
		}
	}

	if(idx >= obj->als_value_num)
	{
		APS_ERR("exceed range\n");
		idx = obj->als_value_num - 1;
	}

	if(1 == atomic_read(&obj->als_deb_on))
	{
		unsigned long endt = atomic_read(&obj->als_deb_end);
		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->als_deb_on, 0);
		}

		if(1 == atomic_read(&obj->als_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
		//APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
		return obj->hw->als_value[idx];
	}
	else
	{
		//APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
		return -1;
	}
}
/*----------------------------------------------------------------------------*/

long tmd2771_read_ps(struct i2c_client *client, u16 *data)
{
	struct tmd2771_priv *obj = i2c_get_clientdata(client);    
	u16 ps_value;    
	u8 ps_value_low[1], ps_value_high[1];
	u8 buffer[1];
	long res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0]=TMD2771_CMM_PDATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, ps_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	buffer[0]=TMD2771_CMM_PDATA_H;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, ps_value_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	*data = ps_value_low[0] | (ps_value_high[0]<<8);
	//APS_DBG("ps_data=%d, low:%d  high:%d\n", *data, ps_value_low[0], ps_value_high[0]);
	printk("ps_data=%d, low:%d  high:%d\n", *data, ps_value_low[0], ps_value_high[0]);
	return 0;    

EXIT_ERR:
	APS_ERR("tmd2771_read_ps fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
static int tmd2771_get_ps_value(struct tmd2771_priv *obj, u16 ps)
{
	int val, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;
	static int val_temp=1;
	 /*Lenovo-sw chenlj2 add 2011-10-12 begin*/
	 u16 temp_ps[1];
	 /*Lenovo-sw chenlj2 add 2011-10-12 end*/

	tmd2771_read_ps(obj->client,temp_ps);
	//APS_LOG("zy***close=%d, far=%d, ps=%d, temp_ps=%d, valid=%d", ps_cali.close, ps_cali.far_away, ps, temp_ps[0], ps_cali.valid);
	//if(ps_cali.valid == 1)
	{
			if((ps >ps_cali.close)&&(temp_ps[0] >ps_cali.close))
			{
				val = 0;  /*close*/
				val_temp = 0;
				intr_flag_value = 1;
			}
			else if((ps <ps_cali.far_away)&&(temp_ps[0] < ps_cali.far_away))
			{
				val = 1;  /*far away*/
				val_temp = 1;
				intr_flag_value = 0;
			}
			else
				val = val_temp;

			//APS_LOG("tmd2771_get_ps_value val  = %d",val);
	}
#if 0
	else
	{
			if((ps > atomic_read(&obj->ps_thd_val_high))&&(temp_ps[0]  > atomic_read(&obj->ps_thd_val_high)))
			{
				val = 0;  /*close*/
				val_temp = 0;
				intr_flag_value = 1;
			}
			else if((ps < atomic_read(&obj->ps_thd_val_low))&&(temp_ps[0]  < atomic_read(&obj->ps_thd_val_low)))
			{
				val = 1;  /*far away*/
				val_temp = 1;
				intr_flag_value = 0;
			}
			else
			       val = val_temp;

	}
#endif
	
	if(atomic_read(&obj->ps_suspend))
	{
		invalid = 1;
	}
	else if(1 == atomic_read(&obj->ps_deb_on))
	{
		unsigned long endt = atomic_read(&obj->ps_deb_end);
		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->ps_deb_on, 0);
		}

		if (1 == atomic_read(&obj->ps_deb_on))
		{
			invalid = 1;
		}
		if((1 == val) && (1 == invalid)){	// for the first mmi test
			return -2;	//different from default value of mmi.
		}
	}
	else if (obj->als > 45000)
	{
		APS_DBG("ligh too high will result to failt proximiy\n");
		return 1;  /*far away*/
	}

	if(!invalid)
	{
		//APS_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	}
	else
	{
		return -1;
	}
}


/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static void tmd2771_eint_work(struct work_struct *work)
{
	struct tmd2771_priv *obj = (struct tmd2771_priv *)container_of(work, struct tmd2771_priv, eint_work);
	int err;
	hwm_sensor_data sensor_data;
	u8 buffer[1];
	u8 reg_value[1];
	u8 databuf[2];
	int res = 0;

	if((err = tmd2771_check_intr(obj->client)))
	{
		APS_ERR("tmd2771_eint_work check intrs: %d\n", err);
	}
	else
	{
		//get raw data
		tmd2771_read_ps(obj->client, &obj->ps);
		//mdelay(160);
		tmd2771_read_als_ch0(obj->client, &obj->als);
		APS_DBG("tmd2771_eint_work close=%d far=%d rawdata ps=%d als_ch0=%d!\n",ps_cali.close, ps_cali.far_away, obj->ps,obj->als);
		//printk("tmd2771_eint_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);
		sensor_data.values[0] = tmd2771_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;	
/*singal interrupt function add*/
#if 1
		if(intr_flag_value){
				//printk("yucong interrupt value ps will < 750");
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				//databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
				databuf[1] = ps_cali.far_away & 0x00FF; //modify for PR350462
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				//databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
				databuf[1] = (ps_cali.far_away & 0xFF00)>>8;  //modify for PR350462
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				databuf[1] = (u8)(0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				databuf[1] = (u8)((0xFF00) >> 8);;
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
		}
		else{
				//printk("yucong interrupt value ps will > 900");
				databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
				databuf[1] = (u8)(0 & 0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;
				databuf[1] = (u8)((0 & 0xFF00) >> 8);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
				//databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
				databuf[1] = ps_cali.close & 0x00FF;//modify for PR350462
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;
				//databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);
				databuf[1] = (ps_cali.close & 0xFF00) >> 8; //modify for PR350462
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
		}
#endif
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
		  APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	tmd2771_clear_intr(obj->client);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
}


static int tmd2771_open(struct inode *inode, struct file *file)
{
	file->private_data = tmd2771_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tmd2771_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static tmd2771_WriteCalibration(struct PS_CALI_DATA_STRUCT *data_cali)
{

	   APS_LOG("tmd2771_WriteCalibration  1 %d," ,data_cali->close);
		   APS_LOG("tmd2771_WriteCalibration  2 %d," ,data_cali->far_away);
		   APS_LOG("tmd2771_WriteCalibration  3 %d,", data_cali->valid);
		
	  if(data_cali->valid == 1)
	  {
			ps_cali.close = data_cali->close;
			ps_cali.far_away= data_cali->far_away;
			ps_cali.valid = 1;
	  }
	
}

static int tmd2771_read_data_mean(struct i2c_client *client, int cnt)
{
	int i=0 ,err = 0,j = 0;
	u32 sum = 0;
	u16 val;
	int prox_mean = 0;
	u16 min = 0xffff;
	u16 max = 0;
	int valid_cnt = 0;

	if(cnt > 100){
		APS_ERR("tmd2771_read_data_mean cnt > 100\n");
		cnt = 20;
	}else if(cnt < 3){
		APS_ERR("tmd2771_read_data_mean cnt < 2\n");
		cnt = 3;
	}
	mdelay(10);
	sum = 0;
	valid_cnt = 0;
	prox_mean = 0;
	for(i = 0; i < cnt; i++){
		if(err = tmd2771_read_ps(client,&val)){
			APS_ERR("tmd2771_read_data_mean fail: %d\n", i);
		}else{
			if(val <= 1024){	// some time it will come a big number.1024 is greatest value.
				sum += val;
				valid_cnt++;
			}
		}
		mdelay(10);
	}
	if(valid_cnt > 0){
		prox_mean = sum / valid_cnt;
	}
	//APS_DBG("prox_mean : %d 0x%x\n", prox_mean, prox_mean);
	printk("prox_mean : %d 0x%x\n", prox_mean, prox_mean);
	 
	return prox_mean;
}

static int tmd2771_cali_psensor(struct PS_CALI_DATA_STRUCT *ps_data_cali, bool boot)
{
	u16 prox_mean = 0;
	u16 prox_mean_new = 0;
	u8 offset;
	int ret;
	int cnt;
	u16 val;
	static u8 boot_mean_valid = 0;
	static u16 boot_mean_value;

	if(boot){
		cnt = 10;
	}else{
		cnt = 4;
	}

	offset = 0;
	tmd2771_read_ps(tmd2771_obj->client, &val);  	//discard the first data
	prox_mean = tmd2771_read_data_mean(tmd2771_obj->client, cnt);
	if(prox_mean < 700){
		ps_data_cali->mean = prox_mean;
		ps_data_cali->close = prox_mean+100;
		ps_data_cali->far_away = prox_mean+50;
		return 0;
	}else{
		return -1;
	}
}

static int tmd2771_read_data_for_cali(struct i2c_client *client, struct PS_CALI_DATA_STRUCT *ps_data_cali,  bool boot)
{
     	int i=0 ,err = 0,j = 0;
	u16 val;
	u16 tmp = 0;
	u8 offset = 0;
	u16 prox_max = 0,prox_mean = 0;
	u32	sum = 0;
	
	u8 databuf[2];
	struct tmd2771_priv *obj = NULL;

	//Need to set 0 to offset register before calibration, because last calibration may modify its value
	databuf[0] = TMD2771_CMM_OFFSET;
	databuf[1] = 0x0;
	err= i2c_master_send(client, databuf, 0x2);
	if(err<= 0)
	{
		goto EXIT_ERR;
	}
	
	if (boot){
		ps_data_cali->offset = 0;
		tmd2771_read_ps(client, &val);  //discard the first data
		prox_mean = tmd2771_read_data_mean(client, 4);
		if(prox_mean < 20){
			// can not do calibration
			rubber_deep_no = RUBBER_NO_045;
			TMD2771_OFFSET_VALUE = RUBBER_45_OFFSET;
			TMD2771_CMM_PPCOUNT_VALUE = RUBBER_45_PPCOUNT;
			TMD2771_CMM_CONTROL_VALUE = RUBBER_45_CONTROL;

			ps_data_cali->valid = 1;
			ps_data_cali->mean = prox_mean;
			ps_data_cali->close = 400;
			ps_data_cali->far_away = 100;

			//default value is use for rubber 0.70mm. so change the value in here.
			atomic_set(&tmd2771_obj->ps_thd_val_high, ps_data_cali->close);
			atomic_set(&tmd2771_obj->ps_thd_val_low, ps_data_cali->far_away);

			ps_cali.valid = ps_data_cali->valid;
			ps_cali.mean = ps_data_cali->mean;
			ps_cali.close = ps_data_cali->close;
			ps_cali.far_away = ps_data_cali->far_away;
			
			return 0; //not let it go through to EXIT_ERR
		}
		rubber_deep_no = RUBBER_NO_070;
		TMD2771_OFFSET_VALUE = RUBBER_70_OFFSET;
		TMD2771_CMM_PPCOUNT_VALUE = RUBBER_70_PPCOUNT;
		TMD2771_CMM_CONTROL_VALUE = RUBBER_70_CONTROL;
		tmd2771_init_client_for_cali(client);
	}

	if(tmd2771_cali_psensor(ps_data_cali, boot)){
		goto EXIT_ERR;
	}else{
		#if 1
		if(!boot_ps_ok){
			//if(ps_data_cali->offset <= 0x20)
			{		//offset = 0; prox_mean < 600; this cali is ok.
				boot_ps_ok = 1;
				boot_ps_cali.close = ps_data_cali->close;
				boot_ps_cali.far_away = ps_data_cali->far_away;
				boot_ps_cali.mean = ps_data_cali->mean;
				boot_ps_cali.offset = ps_data_cali->offset;
				boot_ps_cali.valid = 0;	// this value is not use. just use for debug.
				printk("boot calibrate donot work. close=%d, far=%d, offset=%d\n", ps_data_cali->close, ps_data_cali->far_away, ps_data_cali->offset);
			}
		}
		#endif
		return 0; //not let it go through to EXIT_ERR
	}

EXIT_ERR:
	//Use default threshold value for close and faraway
	obj = i2c_get_clientdata(client);
	//if(boot_ps_ok && (boot_ps_cali.mean>100)){
	if(boot_ps_ok){
		//databuf[0] = TMD2771_CMM_OFFSET;
		//databuf[1] = boot_ps_cali.offset;
		//err = i2c_master_send(client, databuf, 0x2);

		ps_data_cali->close = boot_ps_cali.close; 	//this can customize, adjust close and farway distance
		ps_data_cali->far_away = boot_ps_cali.far_away;
		ps_data_cali->valid = 1;
		APS_LOG("boot ps ok, close=%d, far=%d, offset=%d\n", ps_data_cali->close, ps_data_cali->far_away, ps_data_cali->offset);
		return 0;
	}else{
		//If calibration failed, recover the offset register to 0 to use default threshold.
		//databuf[0] = TMD2771_CMM_OFFSET;
		//databuf[1] = 0x0;
		//err= i2c_master_send(client, databuf, 0x2);
		
		ps_data_cali->close = obj->hw->ps_threshold_high;
		ps_data_cali->far_away = obj->hw->ps_threshold_low;
		ps_data_cali->offset = 0;
		ps_data_cali->valid = 0;
		APS_LOG("tmd2771_read_data_for_cali fail use default close  = %d,far_away = %d,valid = %d",ps_data_cali->close,ps_data_cali->far_away,ps_data_cali->valid);
		err=  -1;
		return err;
	}
}


/*----------------------------------------------------------------------------*/
static long tmd2771_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	struct PS_CALI_DATA_STRUCT ps_cali_temp;

	switch (cmd)
	{
		case ALSPS_SET_PS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(enable)
			{
				if(err = tmd2771_enable_ps(obj->client, 1))
				{
					APS_ERR("enable ps fail: %d\n", err);
					goto err_out;
				}

				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(err = tmd2771_enable_ps(obj->client, 0))
				{
					APS_ERR("disable ps fail: %d\n", err);
					goto err_out;
				}

				clear_bit(CMC_BIT_PS, &obj->enable);
			}
			break;

		case ALSPS_GET_PS_MODE:
			enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_PS_DATA:
			if(err = tmd2771_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}

			dat = tmd2771_get_ps_value(obj, obj->ps);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_PS_RAW_DATA:
			if(err = tmd2771_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}

			dat = obj->ps;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_SET_ALS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(enable)
			{
				if(err = tmd2771_enable_als(obj->client, 1))
				{
					APS_ERR("enable als fail: %d\n", err);
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				if(err = tmd2771_enable_als(obj->client, 0))
				{
					APS_ERR("disable als fail: %d\n", err);
					goto err_out;
				}
				clear_bit(CMC_BIT_ALS, &obj->enable);
			}
			break;

		case ALSPS_GET_ALS_MODE:
			enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_ALS_DATA:
			if(err = tmd2771_read_als(obj->client, &obj->als))
			{
				goto err_out;
			}

			dat = tmd2771_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_ALS_RAW_DATA:
			if(err = tmd2771_read_als(obj->client, &obj->als))
			{
				goto err_out;
			}

			dat = obj->als;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
		default:
			APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;
}



/*john add use for add attr for debug start*/
#ifdef JOHN_ADD_ATTR
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t tmd2771_show_als(struct device_driver *ddri, char *buf)
{
	int res;

	if(!tmd2771_obj) {
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}
	if((res = tmd2771_read_als_ch0(tmd2771_obj->client, &tmd2771_obj->als))){
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}else{
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", tmd2771_obj->als);
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t tmd2771_show_ps(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	u16 val;
	if(!g_tmd2771_ptr)
	{
		APS_ERR("g_tmd2771_ptr is null!!\n");
		return 0;
	}

	if((res = tmd2771_read_ps(g_tmd2771_ptr->client, &val)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "ps_dec= %d 0x%x\n", val, val);
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t tmd2771_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if(!tmd2771_obj)
	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "i2c_retry\tals_debounce\tps_mask\tps_debounce\tps_thd_val_high\tps_thd_val_low\t\n(%d\t%d\t%d\t%d\t%d\t%d)\n",
		atomic_read(&tmd2771_obj->i2c_retry), atomic_read(&tmd2771_obj->als_debounce),
		atomic_read(&tmd2771_obj->ps_mask), atomic_read(&tmd2771_obj->ps_debounce),
	        atomic_read(&tmd2771_obj->ps_thd_val_high),atomic_read(&tmd2771_obj->ps_thd_val_low));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t tmd2771_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thres, thrh, thrl;
	if(!tmd2771_obj)
	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}

	if(6 == sscanf(buf, "%d %d %d %d %d %d", &retry, &als_deb, &mask, &ps_deb,&thrh,&thrl))
	{
		atomic_set(&tmd2771_obj->i2c_retry, retry);
		atomic_set(&tmd2771_obj->als_debounce, als_deb);
		atomic_set(&tmd2771_obj->ps_mask, mask);
		atomic_set(&tmd2771_obj->ps_debounce, ps_deb);
		atomic_set(&tmd2771_obj->ps_thd_val_high, thrh);
		atomic_set(&tmd2771_obj->ps_thd_val_low, thrl);
	}
	else
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;
}

static ssize_t tmd2771_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	u8 buffer[1];
	u8 chip_id[1];
	u8 offset;

	buffer[0]=TMD2771_CHIP_ID;
	i2c_master_send(tmd2771_obj->client, buffer, 0x1);
	i2c_master_recv(tmd2771_obj->client, chip_id, 0x1);
	APS_LOG("tmd2771  chip_id 0x%x\n", chip_id[0]);

	tmd2771_prx_offset_get(tmd2771_obj->client, &offset);

	if(!tmd2771_obj)
	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}

	if(tmd2771_obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST:i2c_num= %d\nppcount=0x%x\ncmm(control)=0x%x\nhigh=%d\nlow=%d\n",
			tmd2771_obj->hw->i2c_num, TMD2771_CMM_PPCOUNT_VALUE,  TMD2771_CMM_CONTROL_VALUE,
			tmd2771_obj->hw->ps_threshold_high,tmd2771_obj->hw->ps_threshold_low);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %02X %02X %02lX %02lX\n",
				atomic_read(&tmd2771_obj->als_cmd_val), atomic_read(&tmd2771_obj->ps_cmd_val),
				tmd2771_obj->enable, tmd2771_obj->pending_intr);


	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&tmd2771_obj->als_suspend), atomic_read(&tmd2771_obj->ps_suspend));

	len += snprintf(buf+len, PAGE_SIZE-len, "chip id 0x%x %d\n", chip_id[0], chip_id[0]);
	len += snprintf(buf+len, PAGE_SIZE-len, "offset 0x%x %d\n", offset, offset);

	len += snprintf(buf+len, PAGE_SIZE-len, "ps_cali.valid 0x%x %d\n", ps_cali.valid, ps_cali.valid);
	len += snprintf(buf+len, PAGE_SIZE-len, "ps_cali.far_away 0x%x %d\n", ps_cali.far_away, ps_cali.far_away);
	len += snprintf(buf+len, PAGE_SIZE-len, "ps_cali.close 0x%x %d\n", ps_cali.close, ps_cali.close);
	len += snprintf(buf+len, PAGE_SIZE-len, "ps_cali.mean 0x%x %d\n", ps_cali.mean, ps_cali.mean);
	len += snprintf(buf+len, PAGE_SIZE-len, "rubber_deep_no 0x%x %d\n", rubber_deep_no, rubber_deep_no);

	len += snprintf(buf+len, PAGE_SIZE-len, "\nboot_ps_cali.valid 0x%x %d\n", boot_ps_cali.valid, boot_ps_cali.valid);
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_ps_cali.far_away 0x%x %d\n", boot_ps_cali.far_away, boot_ps_cali.far_away);
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_ps_cali.close 0x%x %d\n", boot_ps_cali.close, boot_ps_cali.close);
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_ps_cali.mean 0x%x %d\n", boot_ps_cali.mean, boot_ps_cali.mean);


	return len;
}


/* copy code from shanghai. start*/

/*the function is used to read the p-sensor pulse count register to get the pulse count*/
int tmd2771_read_pulse_count(struct i2c_client *client, u8 *data)

{
        int res=0;
        u8 buffer[1];
        u8 pulse_count[1];


    	if(client == NULL)

	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;

	}
    //get pulse count
        //psensor_mutex_lock(&sensor_mutex);
        APS_DBG("tmd2771_read_pulse_count is called\n");
        buffer[0]=TMD2771_CMM_PPCOUNT;
        res = i2c_master_send(client, buffer, 0x1);
        APS_DBG("send is over!res=%d\n,",buffer[0]);

        if(res <= 0)
	{
	    goto EXIT_ERR;
	}
        res = i2c_master_recv(client, pulse_count, 0x1);
        //APS_DBG("tmd2771_read_pulse_count is over!res=%d,pulse_count[0]=%d",res,pulse_count[0]);
        if(res <= 0)
	{
	    goto EXIT_ERR;
	}
        *data=pulse_count[0];
        //psensor_mutex_unlock(&sensor_mutex);
        #ifdef YANG_DEBUG
        APS_DBG("tmd2771_read_pulse_count is over!res=%d,data=%d",res,*data);
        #endif
        return 0;

        EXIT_ERR:

	APS_ERR("tmd2771_read_pulse_count fail\n");
        //psensor_mutex_unlock(&sensor_mutex);

	return res;
}

/*the function is used to write the p-sensor pulse count register*/
int tmd2771_write_pulse_count(struct i2c_client *client, u8 *data)

{
        int res=0;
        u8 databuf[2];
        int debug_res=0;
        u8 debug_pulse[1];
        u8 buf[1];
    	if(client == NULL)

	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;

	}
        //psensor_mutex_lock(&sensor_mutex);
        databuf[0]=TMD2771_CMM_PPCOUNT;
        databuf[1]=*data;
        res = i2c_master_send(client, databuf, 0x2);
        //APS_DBG("tmd2771_write_pulse_count is over!res=%d,databuf[1]=%d",res,databuf[1]);
        if(res <= 0)
	{
	    goto EXIT_ERR;
	}
        //psensor_mutex_unlock(&sensor_mutex);
        return 0;

EXIT_ERR:

	APS_ERR("tmd2771_write_pulse_count fail\n");
        //psensor_mutex_unlock(&sensor_mutex);

	return res;

    }

static ssize_t tmd2771_pulse_count_prx_show(struct device_driver *ddri, char *buf)

{
    	int res=0;
        U8 prx_pulse_count=0;

	if(!tmd2771_obj)

	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}
        /*read the p-sensor pulse count*/
        if((res=tmd2771_read_pulse_count(tmd2771_obj->client,&prx_pulse_count)))
        {
            return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);

        }
        else
        {
            #ifdef YANG_DEBUG
            APS_DBG("tmd2771_pulse_count_prx_show2 is called!res=%d,prx_pulse_count=%d",res,prx_pulse_count);
            #endif
            return snprintf(buf, PAGE_SIZE, "%d 0x%x\n", prx_pulse_count, prx_pulse_count);
        }
   // return snprintf(buf, PAGE_SIZE, "%d\n", TMD2771_CMM_PPCOUNT_VALUE);

    }

static ssize_t tmd2771_pulse_count_prx_store(struct device_driver *ddri, const char *buf,ssize_t size)

{
        int res=0;
        int rc=0;
        u8 pulse_count_prx=0;

    	if(!tmd2771_obj)

	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}

        rc=kstrtou8(buf,16,&pulse_count_prx);
        //APS_DBG("tmd2771_pulse_count_prx_store is called!rc=%d,pulse_count_prx=%d",rc,pulse_count_prx);
        if(rc)
        {
            APS_ERR("tmd2771_pulse_count_prx_store fail!!\n");
            return -1;
        }
        //set pulse count
         if((res=tmd2771_write_pulse_count(tmd2771_obj->client,&pulse_count_prx)))
        {
            return snprintf(pulse_count_prx, PAGE_SIZE, "ERROR: %d\n", res);
        }
        else
        {
            return size;
        }

    }

/*read Proximity Offset Register to get the offset*/
int  tmd2771_prx_offset_get(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 prx_offset[1];
    u8 buffer[1];

    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    #ifdef YANG_DEBUG
    APS_DBG("tmd2771_prx_offset_get is called\n");
    #endif
    buffer[0]=TMD2771_CMM_OFFSET;
    res = i2c_master_send(client, buffer, 0x1);
    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    res = i2c_master_recv(client, prx_offset, 0x1);
    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    *data=prx_offset[0];
    #ifdef YANG_DEBUG
    APS_DBG("tmd2771_prx_offset_get is over res=%d,data=%d\n",res,*data);
    #endif
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

    APS_ERR("tmd2771_prx_offset_get fail\n");
    //psensor_mutex_unlock(&sensor_mutex);

    return res;

}

/*write data to Proximity Offset Register */
int  tmd2771_prx_offset_set(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 databuf[2];

    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    databuf[0]=TMD2771_CMM_OFFSET;
    databuf[1]=*data;
    res = i2c_master_send(client, databuf, 0x2);
    //APS_DBG("tmd2771_prx_offset_set is over!res=%d,databuf[1]=%d",res,databuf[1]);
    if(res<=0)
    {
        goto EXIT_ERR;
    }
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

    APS_ERR("tmd2771_prx_offset_set fail\n");
    //psensor_mutex_unlock(&sensor_mutex);
    return res;
}


static ssize_t tmd2771_offset_prx_show(struct device_driver *ddri, char *buf)

{
    int res=0;
    u8 prx_offset=0;

    if(!tmd2771_obj)
    {
    	APS_ERR("tmd2771_obj is null!!\n");
    	return 0;
    }
    //set Proximity offset
    if((res=tmd2771_prx_offset_get(tmd2771_obj->client,&prx_offset)))
    {
          return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return snprintf(buf, PAGE_SIZE, "%d 0x%x\n", prx_offset, prx_offset);
    }
}

static ssize_t tmd2771_offset_prx_store(struct device_driver *ddri, const char *buf,ssize_t size)
{
    int res;
    u8 offset=0;
    int rc;

    if(!tmd2771_obj)

    {
	APS_ERR("tmd2771_obj is null!!\n");
	return 0;
    }

    rc=kstrtou8(buf,16,&offset);
    //APS_DBG("tmd2771_offset_prx_store is called!rc=%d,offset=%d",rc,offset);

    if(rc)
    {
        APS_ERR("tmd2771_offset_prx_store fail!!\n");
        return -1;
    }

    if((res=tmd2771_prx_offset_set(tmd2771_obj->client,&offset)))
    {
        return snprintf(offset, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return size;
    }
}

/*Read the Control register to get the data*/
int  tmd2771_control_prx_get(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 control_prx[1];
    u8 buffer[1];


    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    APS_DBG("tmd2771_control_prx_get is called\n");
    buffer[0]=TMD2771_CMM_CONTROL;
    res = i2c_master_send(client, buffer, 0x1);
    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    res = i2c_master_recv(client, control_prx, 0x1);

    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    *data=control_prx[0];
    #ifdef YANG_DEBUG
    APS_DBG("tmd2771_control_prx_get is over!res=%d,data=%d",res,*data);
    #endif
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

        APS_ERR("tmd2771_control_prx_get fail\n");
        //psensor_mutex_unlock(&sensor_mutex);
        return res;

}

/*set the Control register*/
int  tmd2771_control_prx_set(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 databuf[2];

    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    databuf[0]=TMD2771_CMM_CONTROL;
    databuf[1]=*data;
    res = i2c_master_send(client, databuf, 0x2);
    //APS_DBG("tmd2771_control_prx_set is over!res=%d,databuf[1]=%d",res,databuf[1]);
    if(res<=0)
    {
        goto EXIT_ERR;
    }
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

    APS_ERR("tmd2771_control_prx_set fail\n");
    //psensor_mutex_unlock(&sensor_mutex);
    return res;
}


static ssize_t tmd2771_control_prx_show(struct device_driver *ddri, char *buf)
{
    ssize_t res;
    u8 control_prx;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    if((res = tmd2771_control_prx_get(tmd2771_obj->client, &control_prx)))

    {
        return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return snprintf(buf, PAGE_SIZE, "%d 0x%x\n", control_prx, control_prx);
    }

}

static ssize_t tmd2771_control_prx_store(struct device_driver *ddri, const char *buf,ssize_t size)
{
    ssize_t res;
    u8 control_prx;
    int rc;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    rc=kstrtou8(buf,16,&control_prx);
    APS_DBG("tmd2771_control_prx_store is called!rc=%d,control_prx=%d",rc,control_prx);
    if(rc)
    {
        APS_ERR("tmd2771_control_prx_store fail!!\n");
        return -1;
    }

    if((res=tmd2771_control_prx_set(tmd2771_obj->client,&control_prx)))
    {
        return snprintf(control_prx, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return size;
    }
}

long tmd2771_read_threshold(struct i2c_client *client, u16 *low_threshold,u16*high_threshold)
{
//	struct tmd2771_priv *obj = i2c_get_clientdata(client);
	//u16 ps_value;
	u8 low_threshold_low[1], low_threshold_high[1],high_threshold_low[1],high_threshold_high[1];
	u8 buffer[1];
	long res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
        //psensor_mutex_lock(&sensor_mutex);
	buffer[0]=TMD2771_CMM_INT_LOW_THD_LOW;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, low_threshold_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	buffer[0]=TMD2771_CMM_INT_LOW_THD_HIGH;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, low_threshold_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	*low_threshold = low_threshold_low[0] | (low_threshold_high[0]<<8);

        buffer[0]=TMD2771_CMM_INT_HIGH_THD_LOW;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, high_threshold_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	buffer[0]=TMD2771_CMM_INT_HIGH_THD_HIGH;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, high_threshold_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	*high_threshold = high_threshold_low[0] | (high_threshold_high[0]<<8);
        #ifdef YANG_DEBUG
        APS_DBG("high_threshold_low[0]=%d,high_threshold_high[0]=%d",high_threshold_low[0] ,high_threshold_low[0]);
        #endif
        #ifdef YANG_DEBUG
        APS_DBG("tmd2771_read_threshold is called!low_threshold=%d,high_threshold=%d",*low_threshold ,*high_threshold);
        #endif
        //psensor_mutex_unlock(&sensor_mutex);
	return 0;

EXIT_ERR:
	APS_ERR("tmd2771_read_threshold fail\n");
        //psensor_mutex_unlock(&sensor_mutex);
	return res;
}

static ssize_t tmd2771_threshold_prx_show(struct device_driver *ddri, char *buf)

{
    	int res=0;
        u16 low_threshold=0;
        u16 high_threshold=0;

	if(!tmd2771_obj)

	{
		APS_ERR("tmd2771_obj is null!!\n");
		return 0;
	}
        /*read threshold*/
        if((res=tmd2771_read_threshold(tmd2771_obj->client,&low_threshold,&high_threshold)))
        {
            return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);

        }
        else
        {
            return snprintf(buf, PAGE_SIZE, "low_threshold=%d,high_threshold=%d\n", low_threshold,high_threshold);
        }
   // return snprintf(buf, PAGE_SIZE, "%d\n", TMD2771_CMM_PPCOUNT_VALUE);

}
/* copy code from shanghai. end*/



/*Read the Control register to get the data*/
int  tmd2771_get_config_reg(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 control_prx[1];
    u8 buffer[1];


    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    APS_DBG("tmd2771_control_prx_get is called\n");
    buffer[0]=TMD2771_CMM_CONTROL;
    res = i2c_master_send(client, buffer, 0x1);
    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    res = i2c_master_recv(client, control_prx, 0x1);

    if(res <= 0)
    {
        goto EXIT_ERR;
    }
    *data=control_prx[0];
    #ifdef YANG_DEBUG
    APS_DBG("tmd2771_control_prx_get is over!res=%d,data=%d",res,*data);
    #endif
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

        APS_ERR("tmd2771_control_prx_get fail\n");
        //psensor_mutex_unlock(&sensor_mutex);
        return res;

}

static ssize_t tmd2771_show_config_reg(struct device_driver *ddri, char *buf)
{
    ssize_t res;
    u8 config_reg;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    if((res = tmd2771_get_config_reg(tmd2771_obj->client, &config_reg)))

    {
        return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return snprintf(buf, PAGE_SIZE, "config_reg:%d 0x%x\n", config_reg, config_reg);
    }

}


/*set the config register*/
int  tmd2771_set_config_reg_value(struct i2c_client *client, u8 *data)
{
    int res=0;
    u8 databuf[2];

    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
	return -1;
    }
    //psensor_mutex_lock(&sensor_mutex);
    databuf[0]=TMD2771_CMM_CONFIG;
    databuf[1]=*data;
    res = i2c_master_send(client, databuf, 0x2);
    //APS_DBG("tmd2771_control_prx_set is over!res=%d,databuf[1]=%d",res,databuf[1]);
    if(res<=0)
    {
        goto EXIT_ERR;
    }
    //psensor_mutex_unlock(&sensor_mutex);
    return 0;

EXIT_ERR:

    APS_ERR("tmd2771_config_reg fail\n");
    //psensor_mutex_unlock(&sensor_mutex);
    return res;
}

static ssize_t tmd2771_set_config_reg(struct device_driver *ddri, const char *buf,ssize_t size)
{
    ssize_t res;
    u8 control_prx;
    int rc;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    rc=kstrtou8(buf,16,&control_prx);
    APS_DBG("tmd2771_control_prx_store is called!rc=%d,control_prx=%d",rc,control_prx);
    if(rc)
    {
        APS_ERR("tmd2771_control_prx_store fail!!\n");
        return -1;
    }

    if((res=tmd2771_set_config_reg_value(tmd2771_obj->client,&control_prx)))
    {
        return snprintf(control_prx, PAGE_SIZE, "ERROR: %d\n", res);
    }
    else
    {
        return size;
    }
}


static ssize_t tmd2771_show_para_zoom(struct device_driver *ddri, char *buf)
{
    ssize_t res;
    u8 config_reg;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    return snprintf(buf, PAGE_SIZE, "zoom_time:%d als_modulus:%d\n", ZOOM_TIME, tmd2771_obj->als_modulus);

}


static ssize_t tmd2771_set_para_zoom(struct device_driver *ddri, const char *buf,ssize_t size)
{
    ssize_t res;
    u8 zoom;
    int rc;

    if(!tmd2771_obj)

    {
        APS_ERR("tmd2771_obj is null!!\n");
        return 0;
    }

    rc=kstrtou8(buf,10,&zoom);
    if(rc)
    {
        APS_ERR("tmd2771_control_prx_store fail!!\n");
        return -1;
    }
    ZOOM_TIME = zoom;
    tmd2771_obj->als_modulus = (400*100*zoom)/(1*150);

    
    APS_DBG("tmd2771_set_para_zoom zoom=%d,als_modulus=%d", zoom, tmd2771_obj->als_modulus);

    return size;
}



static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, tmd2771_show_als,   NULL);
static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, tmd2771_show_ps,    NULL);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, tmd2771_show_config,tmd2771_store_config);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, tmd2771_show_status,  NULL);

static DRIVER_ATTR(pulse_count_prx,  S_IWUSR | S_IRUGO, tmd2771_pulse_count_prx_show,tmd2771_pulse_count_prx_store);
static DRIVER_ATTR(offset_prx, S_IWUSR | S_IRUGO, tmd2771_offset_prx_show, tmd2771_offset_prx_store);
static DRIVER_ATTR(control_prx,S_IWUSR | S_IRUGO,tmd2771_control_prx_show,tmd2771_control_prx_store);
static DRIVER_ATTR(threshold_prx,S_IWUSR | S_IRUGO,tmd2771_threshold_prx_show,NULL);

static DRIVER_ATTR(config_reg,S_IWUSR | S_IRUGO,tmd2771_show_config_reg,tmd2771_set_config_reg);

static DRIVER_ATTR(zoom,S_IWUSR | S_IRUGO, tmd2771_show_para_zoom,tmd2771_set_para_zoom);



static struct driver_attribute *tmd2771_attr_list[] = {
    &driver_attr_als,
    &driver_attr_ps,
    &driver_attr_config,
    &driver_attr_status,


    &driver_attr_pulse_count_prx,
    &driver_attr_offset_prx,
    &driver_attr_control_prx,
    &driver_attr_threshold_prx,

    &driver_attr_zoom,
};

static int tmd2771_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(tmd2771_attr_list)/sizeof(tmd2771_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, tmd2771_attr_list[idx])))
		{
			APS_ERR("driver_create_file (%s) = %d\n", tmd2771_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int tmd2771_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(tmd2771_attr_list)/sizeof(tmd2771_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, tmd2771_attr_list[idx]);
	}

	return err;
}

#endif

/*----------------------------------------------------------------------------*/
static struct file_operations tmd2771_fops = {
	.owner = THIS_MODULE,
	.open = tmd2771_open,
	.release = tmd2771_release,
	.unlocked_ioctl = tmd2771_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tmd2771_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &tmd2771_fops,
};
/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	//struct tmd2771_priv *obj = i2c_get_clientdata(client);
	//int err;
	APS_FUN();
#if 0
	if(msg.event == PM_EVENT_SUSPEND)
	{
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}

		atomic_set(&obj->als_suspend, 1);
		if(err = tmd2771_enable_als(client, 0))
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		if(err = tmd2771_enable_ps(client, 0))
		{
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}

		tmd2771_power(obj->hw, 0);
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_resume(struct i2c_client *client)
{
	//struct tmd2771_priv *obj = i2c_get_clientdata(client);
	//int err;
	APS_FUN();
#if 0
	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	tmd2771_power(obj->hw, 1);
	if(err = tmd2771_init_client(client))
	{
		APS_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if(err = tmd2771_enable_als(client, 1))
		{
			APS_ERR("enable als fail: %d\n", err);
		}
	}
	atomic_set(&obj->ps_suspend, 0);
	if(test_bit(CMC_BIT_PS,  &obj->enable))
	{
		if(err = tmd2771_enable_ps(client, 1))
		{
			APS_ERR("enable ps fail: %d\n", err);
		}
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static void tmd2771_early_suspend(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct tmd2771_priv *obj = container_of(h, struct tmd2771_priv, early_drv);
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	#if 1
	atomic_set(&obj->als_suspend, 1);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if(err = tmd2771_enable_als(obj->client, 0))
		{
			APS_ERR("disable als fail: %d\n", err);
		}
	}
	#endif
}
/*----------------------------------------------------------------------------*/
static void tmd2771_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct tmd2771_priv *obj = container_of(h, struct tmd2771_priv, early_drv);
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

        #if 1
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if(err = tmd2771_enable_als(obj->client, 1))
		{
			APS_ERR("enable als fail: %d\n", err);

		}
	}
	#endif
}

int tmd2771_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct tmd2771_priv *obj = (struct tmd2771_priv *)self;

	//APS_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
#ifdef USE_CALIBRATION_ON_CALL
					//add for calibration on call setup, now we read 3 times, before setting the interrupt threshold  register
					if(RUBBER_NO_070 == rubber_deep_no){
						tmd2771_init_client_for_cali(obj->client);
						tmd2771_read_data_for_cali(obj->client, &ps_cali, 0);
					}
#endif
					if(err = tmd2771_enable_ps(obj->client, 1))
					{
						APS_ERR("enable ps fail: %d\n", err);
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
					#if 0
					if(err = tmd2771_enable_als(obj->client, 1))
					{
						APS_ERR("enable als fail: %d\n", err);
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
					#endif
				}
				else
				{
					if(err = tmd2771_enable_ps(obj->client, 0))
					{
						APS_ERR("disable ps fail: %d\n", err);
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
					#if 0
					if(err = tmd2771_enable_als(obj->client, 0))
					{
						APS_ERR("disable als fail: %d\n", err);
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
					#endif
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;
				tmd2771_read_ps(obj->client, &obj->ps);

                                //mdelay(10);
				tmd2771_read_als_ch0(obj->client, &obj->als);
				APS_ERR("tmd2771_ps_operate als data=%d!\n",obj->als);
				sensor_data->values[0] = tmd2771_get_ps_value(obj, obj->ps);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

int tmd2771_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct tmd2771_priv *obj = (struct tmd2771_priv *)self;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if(err = tmd2771_enable_als(obj->client, 1))
					{
						APS_ERR("enable als fail: %d\n", err);
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if(err = tmd2771_enable_als(obj->client, 0))
					{
						APS_ERR("disable als fail: %d\n", err);
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}

			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;
				/*yucong MTK add for fixing know issue*/
				#if 1
				tmd2771_read_als(obj->client, &obj->als);
				//if(obj->als == 0) //modify for PR353124
				//{
				//	sensor_data->values[0] = -1;

				//}else{
					u16 b[2];
					int i;
					for(i = 0;i < 2;i++){
					tmd2771_read_als(obj->client, &obj->als);
					b[i] = obj->als;
					}
					(b[1] > b[0])?(obj->als = b[0]):(obj->als = b[1]);
					sensor_data->values[0] = tmd2771_get_als_value(obj, obj->als);
				//}
				#endif
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}


/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TMD2771_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmd2771_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	u8 chip_id[1];
	u8 buffer[1];

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	tmd2771_obj = obj;

	obj->hw = get_cust_alsps_hw();
	tmd2771_get_addr(obj->hw, &obj->addr);

	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	INIT_WORK(&obj->eint_work, tmd2771_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 200);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val,  0xC1);
	//atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	//atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);
	/*Lenovo-sw chenlj2 add 2011-06-03,modified gain 16 to 1/5 accoring to actual thing */
	obj->als_modulus = (400*100*ZOOM_TIME)/(1*150);//(1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);


	tmd2771_i2c_client = client;

	rubber_deep_no = RUBBER_NO_045;
	TMD2771_OFFSET_VALUE = RUBBER_45_OFFSET;
	TMD2771_CMM_PPCOUNT_VALUE = RUBBER_45_PPCOUNT;
	TMD2771_CMM_CONTROL_VALUE = RUBBER_45_CONTROL;

	tmd2771_init_client_for_cali(client);

	buffer[0] = TMD2771_CHIP_ID;
	i2c_master_send(client, buffer, 0x1);
	i2c_master_recv(client, chip_id, 0x1);
	APS_LOG("tmd2771  chip_id 0x%x\n", chip_id[0]);
	
	boot_ps_ok = 0;
	if(err = tmd2771_read_data_for_cali(client,&boot_ps_cali, 1))
	{
		boot_ps_ok = 0;
	}else{
		boot_ps_ok = 1;
	}
		
	if(err = tmd2771_init_client(client))
	{
		goto exit_init_failed;
	}
	APS_LOG("tmd2771_init_client() OK!\n");

	if(err = misc_register(&tmd2771_device))
	{
		APS_ERR("tmd2771_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if(err = tmd2771_create_attr(&tmd2771_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	obj_ps.self = tmd2771_obj;
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(1 == obj->hw->polling_mode_ps)
	{
		obj_ps.polling = 1;
	}
	else
	{
		obj_ps.polling = 0;
	}

	obj_ps.sensor_operate = tmd2771_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	obj_als.self = tmd2771_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = tmd2771_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = tmd2771_early_suspend,
	obj->early_drv.resume   = tmd2771_late_resume,
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&tmd2771_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	exit_kfree:
	kfree(obj);
	exit:
	tmd2771_i2c_client = NULL;
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int tmd2771_i2c_remove(struct i2c_client *client)
{
	int err;
	if(err = misc_deregister(&tmd2771_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);
	}

	tmd2771_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int tmd2771_probe(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	tmd2771_power(hw, 1);
	//tmd2771_force[0] = hw->i2c_num;
	//tmd2771_force[1] = hw->i2c_addr[0];
	//APS_DBG("I2C = %d, addr =0x%x\n",tmd2771_force[0],tmd2771_force[1]);
	if(i2c_add_driver(&tmd2771_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int tmd2771_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();
	tmd2771_power(hw, 0);
	i2c_del_driver(&tmd2771_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver tmd2771_alsps_driver = {
	.probe      = tmd2771_probe,
	.remove     = tmd2771_remove,
	.driver     = {
		.name  = "als_ps",
//		.owner = THIS_MODULE,
	}
};
/*----------------------------------------------------------------------------*/
static int __init tmd2771_init(void)
{
	APS_FUN();
	i2c_register_board_info(0, &i2c_TMD2771, 1);
	if(platform_driver_register(&tmd2771_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit tmd2771_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&tmd2771_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(tmd2771_init);
module_exit(tmd2771_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Dexiang Liu");
MODULE_DESCRIPTION("tmd2771 driver");
MODULE_LICENSE("GPL");




