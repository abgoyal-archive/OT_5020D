
#if 0
#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <mach/mt6575_typedefs.h>

#include <asm/uaccess.h>

#include "fan5405.h"
#endif

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
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

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "fan5405_jrd.h"
#include <linux/hwmsen_helper.h>

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
#include <mach/mt6577_devs.h>
#include <mach/mt6577_typedefs.h>
#include <mach/mt6577_gpio.h>
#include <mach/mt6577_pm_ldo.h>
#endif


#define INSMOD_DEBUG	0
#define fan5405_SLAVE_ADDR_WRITE	0xD4
#define fan5405_SLAVE_ADDR_Read	0xD5
#define FAN5405_WRITE_TRANS	1
#define FAN5405_READ_TRANS	2

/* Addresses to scan */
extern int Enable_BATDRV_LOG;
static struct i2c_client *new_client = NULL;
static const struct i2c_device_id fan5405_i2c_id[] = {{"fan5405",0},{}};   

struct fan5405_struct *the_fan5405_struct = NULL;

static int fan5405_driver_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int fan5405_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int fan5405_driver_remove(struct i2c_client *client);

static struct i2c_driver fan5405_driver = {
	.driver = {
		.name	= "fan5405",
	},
	.class		= I2C_CLASS_HWMON,
	.probe		= fan5405_driver_probe,
	.detect		= fan5405_driver_detect,
	.id_table	= fan5405_i2c_id,
};

#define fan5405_REG_NUM 7  
kal_uint8 fan5405_reg[fan5405_REG_NUM] = {0};

//static DEFINE_MUTEX(fan5405_i2c_access_mutex_0);
ssize_t fan5405_read_byte(u8 reg, u8 *returnData)	/*read reg data and save to returndata*/
{
    char     cmd_buf[2]={0x00, 0x00};
    char     readData = 0;
    int     ret=0;
    
    struct i2c_msg	fan_msg[2];

    struct i2c_client *client	= new_client;
    struct i2c_adapter	*adap	= client->adapter;
	

//printk(KERN_INFO "jrd %s: client->addr:0x%x! flags:%d,reg:%d!\n",__func__, new_client->addr, client->flags, reg);
//	mutex_lock(&fan5405_i2c_access_mutex_0);
	
	client->addr = ((client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;	
	client->flags = 0x00;

	cmd_buf[0] = reg;	/*reg num*/
	
	fan_msg[0].addr = client->addr;
	fan_msg[0].flags = client->flags & I2C_M_TEN;	/*flag for 10-bit address or 7-bit address*/
	fan_msg[0].len = 1;
	fan_msg[0].buf = &cmd_buf[0];
	fan_msg[0].timing = client->timing;
	fan_msg[0].ext_flag = client->ext_flag;

        fan_msg[1].addr = client->addr;
        fan_msg[1].flags = client->flags | I2C_M_RD;
        fan_msg[1].len = 1;
        fan_msg[1].buf = &cmd_buf[1];
	fan_msg[1].timing = client->timing;
	fan_msg[1].ext_flag = client->ext_flag;
	
	if(Enable_BATDRV_LOG >= 1)
	{
	printk(KERN_INFO "jrd %s: fan_msg0.addr:0x%x, flags:0x%x,len:0x%x, buf:0x%x, timing:0x%x, ext_flag:0x%x!\n\r",
				__func__, fan_msg[0].addr, fan_msg[0].flags, fan_msg[0].len, cmd_buf[0], fan_msg[0].timing, fan_msg[0].ext_flag);
	printk(KERN_INFO "jrd %s: fan_msg1.addr:0x%x, flags:0x%x,len:0x%x, buf:0x%x, timing:0x%x, ext_flag:0x%x!\n\r",
                                __func__, fan_msg[1].addr, fan_msg[1].flags, fan_msg[1].len, cmd_buf[1], fan_msg[1].timing, fan_msg[1].ext_flag);
	}	
//	i2c_transfer(adap, fan_msg, FAN5405_READ_TRANS);	/*i2c-gpio*/	
	ret = i2c_master_send(client, cmd_buf, (1 << 8 | 1));
	if(ret < 0)
		{
		printk(KERN_INFO "%s: send cmd fail!\n\r",__func__);
		}
//	ret = i2c_master_recv(client, &cmd_buf[1], 1);

    if (ret < 0) 
	{
        printk(KERN_INFO "Power/fan5405[mt6329_read_byte:write&read] fan5405 sends command error!! \n");
		new_client->addr = new_client->addr & I2C_MASK_FLAG;
//		mutex_unlock(&fan5405_i2c_access_mutex_0);
        return 0;
    }
	readData = cmd_buf[0];
    *returnData = readData;
//	printk(KERN_INFO "jrd %s: readData:0x%x",__func__, readData);

	new_client->addr = new_client->addr & I2C_MASK_FLAG;
    
//	mutex_unlock(&fan5405_i2c_access_mutex_0);
    return 1;

}

ssize_t fan5405_write_byte(u8 reg, u8 writeData)
{


    char    write_data[2] = {0};
    int    ret=0;

    struct i2c_msg      fan_msg[2];

    struct i2c_client *client   = new_client;
    struct i2c_adapter  *adap   = client->adapter;
    
//    printk(KERN_INFO "jrd %s: reg:0x%x, writeData:0x%x!\n",__func__, reg, writeData);
//	mutex_lock(&fan5405_i2c_access_mutex_0);
	
    write_data[0] = reg;
    write_data[1] = writeData;
    

	client->flags = 0x00;
        //client->addr = ((client->addr) & I2C_MASK_FLAG) | I2C_WR_FLAG;
        client->addr = ((client->addr) & I2C_MASK_FLAG);

        fan_msg[0].addr = client->addr;
        fan_msg[0].flags = client->flags & I2C_M_TEN;	/*flag for 10-bit address or 7-bit address*/
        fan_msg[0].len = 1;
        fan_msg[0].buf = &write_data[0];
	fan_msg[0].ext_flag = client->ext_flag;
	fan_msg[0].timing = client->timing;

        fan_msg[1].addr = client->addr;
        fan_msg[1].flags = client->flags | I2C_M_NOSTART;	/*set repeat start and write*/
        fan_msg[1].len = 1;
        fan_msg[1].buf = &write_data[1];
	fan_msg[1].ext_flag = client->ext_flag;
	fan_msg[1].timing = client->timing;

	if(Enable_BATDRV_LOG >= 1)
	{	
	printk(KERN_INFO "jrd %s: fan_msg0.addr:0x%x, flags:0x%x,len:0x%x, buf:0x%x, timing:0x%x, ext_flag:0x%x!\n\r",
                                __func__, fan_msg[0].addr, fan_msg[0].flags, fan_msg[0].len, write_data[0], fan_msg[0].timing, fan_msg[0].ext_flag);
	printk(KERN_INFO "jrd %s: fan_msg1.addr:0x%x, flags:0x%x,len:0x%x, buf:0x%x, timing:0x%x, ext_flag:0x%x!\n\r",
                                __func__, fan_msg[1].addr, fan_msg[1].flags, fan_msg[1].len, write_data[1], fan_msg[1].timing, fan_msg[1].ext_flag);
	}

//        i2c_transfer(adap, fan_msg, FAN5405_READ_TRANS);	/*i2c-gpio*/
	ret = i2c_master_send(client, write_data, 2);			/*hardware i2c0*/
//	mutex_unlock(&fan5405_i2c_access_mutex_0);
    return 1;

}


uint8_t fan5405_get_reg_value(struct fan5405_struct *fan_struct, uint8_t reg){
	int	ret = 0;
	uint8_t return_data;

	ret = fan5405_read_byte(reg, &return_data);
	#if 0
	if(ret < 0)
	
	#endif
        if(Enable_BATDRV_LOG >= 1)
		printk(KERN_INFO "jrd %s: reg:%d,returndata:0x%x, ret:0x%x",__func__, reg, return_data, ret);
	return return_data;	
}

int8_t fan5405_set_con0_val(struct fan5405_struct *fan_struct, uint8_t t32_rst, uint8_t en_stat){
	int ret = 0;
	uint8_t reg = fan5405_CON0;
	uint8_t val = (t32_rst << 1) | en_stat;
        uint8_t mask = 0x03;    /*CONTROL0[7:6]: TMR_RST_OTG and EN_STAT*/
        uint8_t shift = 6;
	uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)
		printk(KERN_INFO "jrd %s t23_rst:0x%x, en_stat:0x%x!\n",__func__, t32_rst, en_stat);
	write_data = fan5405_get_reg_value(fan_struct, reg);	/*read data from reg first*/

        write_data &= ~(mask << shift);
        write_data |= (val << shift);

	ret = fan5405_write_byte(reg, write_data);	
	#if 0
	if(ret < 0)
	#endif
	
	return 0;	
}

int8_t fan5405_set_con1_val(struct fan5405_struct *fan_struct, 
				uint8_t Iinlim, uint8_t Vlowv, uint8_t Te, uint8_t Ce, uint8_t Hz, uint8_t Opa){
        int ret = 0;
        uint8_t reg = fan5405_CON1;
        uint8_t val = (Iinlim << 6) | (Vlowv << 4) | (Te << 3) | (Ce << 2) | (Hz << 1) | Opa;
        uint8_t mask = 0xff;    /*CONTROL1[7:0]: */
        uint8_t shift = 0;
        uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)
		{
		printk(KERN_INFO "jrd %s: Iinlim:0x%x, Vlowv:0x%x, Te:0x%x, Ce:0x%x, Hz:0x%x, Opa:0x%x!\n", __func__,
				Iinlim, Vlowv, Te, Ce, Hz, Opa);
		}
#if 0
        write_data = fan5405_get_reg_value(fan_struct, reg);    /*read data from reg first*/

        write_data &= ~(mask << shift);
#endif
        write_data |= (val << shift);

        ret = fan5405_write_byte(reg, write_data);
        #if 0
        if(ret < 0)
        #endif

        return 0;
}


int8_t fan5405_set_con2_val(struct fan5405_struct *fan_struct, 
				uint8_t Vchg_out, uint8_t otg_pl, uint8_t otg_en){
        int ret = 0;
        uint8_t reg = fan5405_CON2;
        uint8_t val = (Vchg_out << 2) | (otg_pl << 1) | otg_en;
        uint8_t mask = 0xff;    /*CONTROL1[7:0]: */
        uint8_t shift = 0;
        uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)
		printk(KERN_INFO "jrd %s: Vchg_out:0x%x, otg_pl:0x%x, otg_en:0x%x",__func__, Vchg_out, otg_pl, otg_en);
#if 0
        write_data = fan5405_get_reg_value(fan_struct, reg);    /*read data from reg first*/
        
        write_data &= ~(mask << shift);
#endif
        write_data |= (val << shift);

        ret = fan5405_write_byte(reg, write_data);
        #if 0
        if(ret < 0)
        #endif

        return 0;
}

int8_t fan5405_set_con4_val(struct fan5405_struct *fan_struct,
				uint8_t reset, uint8_t Iochg, uint8_t Iterm){
        int ret = 0;
        uint8_t reg = fan5405_CON4;
       	uint8_t val = (reset << 7) | (Iochg << 4) | Iterm;
        uint8_t mask = 0xff;    /*CONTROL1[7:0]: */
        uint8_t shift = 0;
        uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)
		printk(KERN_INFO "jrd %s: reset:0x%x, Iochg:0x%x, Iterm:0x%x",__func__, reset, Iochg, Iterm);
#if 0
        write_data = fan5405_get_reg_value(fan_struct, reg);    /*read data from reg first*/
        
        write_data &= ~(mask << shift);
#endif
        write_data |= (val << shift);

        ret = fan5405_write_byte(reg, write_data);
        #if 0
        if(ret < 0)
        #endif

        return 0;
}

int8_t fan5405_set_con5_val(struct fan5405_struct *fan_struct,
				uint8_t dis_verg, uint8_t io_level, uint8_t vsp){
        int ret = 0;
        uint8_t reg = fan5405_CON5;
        uint8_t val = (dis_verg << 6) | (io_level << 5) | vsp;
        uint8_t mask = 0x03;    /*CONTROL1[7:0]: */
        uint8_t shift = 3;
        uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)        
		printk(KERN_INFO "jrd %s: dis_verg:0x%x,io_level:0x%x, vsp:0x%x",__func__,dis_verg, io_level, vsp);
#if 1
        write_data = fan5405_get_reg_value(fan_struct, reg);    /*read data from reg first*/
        
        write_data &= (mask << shift);
#endif
        write_data |= val;

        ret = fan5405_write_byte(reg, write_data);
        #if 0
        if(ret < 0)
        #endif

        return 0;
}

int8_t fan5405_set_con6_val(struct fan5405_struct *fan_struct,
				uint8_t Isafe, uint8_t Vsafe){
        int ret = 0;
        uint8_t reg = fan5405_CON6;
        uint8_t val = (Isafe << 4) | Vsafe;
        uint8_t mask = 0xff;    /*CONTROL1[7:0]: */
        uint8_t shift = 0;
        uint8_t write_data = 0;

        if(Enable_BATDRV_LOG >= 1)
		printk(KERN_INFO "jrd %s: Isafe:0x%x, Vsafe:0x%x!\n",__func__, Isafe, Vsafe);
#if 0
        write_data = fan5405_get_reg_value(fan_struct, reg);    /*read data from reg first*/
        
        write_data &= ~(mask << shift);
#endif
        write_data |= (val << shift);

        ret = fan5405_write_byte(reg, write_data);
        #if 0
        if(ret < 0)
        #endif

        return 0;
}

static int8_t fan5405_disable_charging(struct fan5405_struct *fan_struct, uint8_t disable){

        if(Enable_BATDRV_LOG >= 1)
	        printk(KERN_INFO "jrd %s: enter disable:%d!\n",__func__,disable);
	mt_set_gpio_mode(GPIO202, 0);	/*mode 0 as gpio*/
        mt_set_gpio_dir(GPIO202, GPIO_DIR_OUT);		/*griffin:(DPIDE)GPIO30; martell:GPIO202 FIXME*/
        mt_set_gpio_out(GPIO202, (disable == 0)? GPIO_OUT_ZERO : GPIO_OUT_ONE); /*disable == 0: charging controlled by i2c line FIXME*/
}

static int8_t fan5405_check_chgstat_pin(struct fan5405_struct *fan_struct){
        int8_t ret = 0;

        if(Enable_BATDRV_LOG >= 1)
	        printk(KERN_INFO "jrd %s: enter!\n",__func__);
	mt_set_gpio_mode(GPIO200, 0);	/*mode 0 as gpio*/
        mt_set_gpio_dir(GPIO200, GPIO_DIR_IN);  /*griffin:(DPIVSYNC)GPIO21; martell:GPIO200; get STAT pin status FIXME*/
        ret = mt_get_gpio_in(GPIO200);		/*FIXME*/

        return ret;
}

static int8_t fan5405_set_otg_pin(struct fan5405_struct *fan_struct, uint8_t otg_level){

        if(Enable_BATDRV_LOG >= 1)
                printk(KERN_INFO "jrd %s: enter, set otgpin:%d!\n",__func__, otg_level);

        mt_set_gpio_mode(GPIO192, 0);    /*mode 0 as gpio*/
        mt_set_gpio_dir(GPIO192, GPIO_DIR_OUT);  /*griffin:GPIO27(DPIR4); martell:GPIO192; otg */
        mt_set_gpio_out(GPIO192, (otg_level == 0)? GPIO_OUT_ZERO : GPIO_OUT_ONE);           /**/
}

static const struct fan5405_ops fan_ops = {
        .get_reg_value  = fan5405_get_reg_value,
        .set_con0_val   = fan5405_set_con0_val,
        .set_con1_value = fan5405_set_con1_val,
        .set_con2_val   = fan5405_set_con2_val,
//        .get_ic_info    = fan5405_get_ic_info,
        .set_con4_val   = fan5405_set_con4_val,
        .set_con5_val   = fan5405_set_con5_val,
        .set_con6_val   = fan5405_set_con6_val,
	
	.disable_charging	= fan5405_disable_charging,
	.check_chgstat_pin	= fan5405_check_chgstat_pin,
	.set_otg_pin	= fan5405_set_otg_pin,
};


int8_t fan5405_hw_init(struct fan5405_struct *fan_struct)
{	
	int ic_info = 0;
	int vender_code = 0;
	int8_t ret = 0;
	uint8_t regnum;
	uint8_t reg_val;

	fan_struct->ops->set_con6_val(fan_struct, 0x07, 0x09);	/*1.25A, 4.42V*/
	regnum = fan5405_CON6;	
	reg_val = fan_struct->ops->get_reg_value(fan_struct, regnum);
	
	regnum = fan5405_CON3;	
	reg_val = fan_struct->ops->get_reg_value(fan_struct, regnum);
	printk("%s: reg:0x%x, reg_val:0x%x, reg_ic:0x%x!\n",__func__, regnum, reg_val, reg_val & 0xE0);
	if((reg_val & 0xE0) == 0x80)
		printk(KERN_INFO "jrd %s get fan5405 ic_info:0x%x success!\n",__func__, reg_val);
	else
		ret = -1;

	return ret;
}

static int fan5405_driver_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{         
    strcpy(info->type, "fan5405");                                                         

	printk("[fan5405_driver_detect] \n");
	
    return 0;                                                                                       
}

static int fan5405_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
	struct class_device *class_dev = NULL;
    int err=0; 
    struct fan5405_struct *fan_struct;

    printk("[fan5405_driver_probe] \n");

	if (!(new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
	printk("%s: kmalloc client fail!\n",__func__);
        err = -ENOMEM;
        goto exit;
    }	
    memset(new_client, 0, sizeof(struct i2c_client));

	new_client = client;

    fan_struct = kzalloc(sizeof(struct fan5405_struct), GFP_KERNEL);
    if (!fan_struct){
	printk("%s: kzalloc fan_struct fail!\n",__func__);
        err = -ENOMEM;
        goto exit_kfree;
    }

//    fan_struct->dev_readaddr = FAN5405_SLAVE_ADDR;
    fan_struct->ops = &fan_ops;
//    atomic_set(&fan_struct->config_count, 1);	/* re-setting chg param flag*/
    fan_struct->config_count = 1;	/* re-setting chg param flag*/
//    atomic_set(&fan_struct->disable_count, 1);	/*set fan5405 to HZ mode flag*/
    fan_struct->disable_count = 1;	/*set fan5405 to HZ mode flag*/
    fan_struct->work_mode = FAN5405_CHARGE_MODE;	/*set fan5405 to HZ mode flag*/
    fan_struct->boost_fault_status = BOOST_NORMAL_STATUS;
    fan_struct->chg_fault_status =  CHARGE_NORMAL_STATUS;

    the_fan5405_struct = fan_struct;
	
    if(fan5405_hw_init(fan_struct) == -1)
	{	
	printk("%s: failed!\n",__func__);
	goto exit_kfree_fan;
	}
    else
	printk("%s: success!\n",__func__);

//	fan5405_dump_register();

    return 0;  
                                                                                     
exit_kfree_fan:
    kfree(fan_struct);
    the_fan5405_struct = NULL;	
exit_kfree:
    kfree(new_client);
exit:
    return err;

}

int g_reg_value_fan5405=0;
static ssize_t show_fan5405_access(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret=0;
	printk("[show_fan5405_access] 0x%x\n", g_reg_value_fan5405);
	return sprintf(buf, "%u\n", g_reg_value_fan5405);
}
static ssize_t store_fan5405_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int ret=0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;
	
	printk("[store_fan5405_access] \n");
	
	if(buf != NULL && size != 0)
	{
		printk("[store_fan5405_access] buf is %s and size is %d \n",buf,size);
		reg_address = simple_strtoul(buf,&pvalue,16);
		
		if(size > 3)
		{		
			reg_value = simple_strtoul((pvalue+1),NULL,16);		
			printk("[store_fan5405_access] write fan5405 reg 0x%x with value 0x%x !\n",reg_address,reg_value);
//			ret=fan5405_config_interface(reg_address, reg_value, 0xFF, 0x0);
		}
		else
		{	
//			ret=fan5405_read_interface(reg_address, &g_reg_value_fan5405, 0xFF, 0x0);
			printk("[store_fan5405_access] read fan5405 reg 0x%x with value 0x%x !\n",reg_address,g_reg_value_fan5405);
			printk("[store_fan5405_access] Please use \"cat fan5405_access\" to get value\r\n");
		}		
	}	
	return size;
}
static DEVICE_ATTR(fan5405_access, 0664, show_fan5405_access, store_fan5405_access); //664

static int fan5405_user_space_probe(struct platform_device *dev)	
{	
	int ret_device_file = 0;

	printk("******** fan5405_user_space_probe!! ********\n" );
	
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_fan5405_access);
	
	return 0;
}

struct platform_device fan5405_user_space_device = {
    .name   = "fan5405-user",
    .id	    = -1,
};

static struct platform_driver fan5405_user_space_driver = {
    .probe		= fan5405_user_space_probe,
    .driver     = {
        .name = "fan5405-user",
    },
};

#define FAN5405_BUSNUM 0

static int __init fan5405_init(void)
{	
	int ret=0;
	
	printk("[fan5405_init] init start\n");
//debug liao
//	i2c_register_board_info(FAN5405_BUSNUM, &fan5405_dev, 1);
#if INSMOD_DEBUG
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif

#if 1	/* FIXME */
	if(i2c_add_driver(&fan5405_driver)!=0)
	{
		printk("[fan5405_init] failed to register fan5405 i2c driver.\n");
	}
	else
	{
		printk("[fan5405_init] Success to register fan5405 i2c driver.\n");
	}
#endif

#if 1
	// fan5405 user space access interface
	ret = platform_device_register(&fan5405_user_space_device);
    if (ret) {
		printk("****[fan5405_init] Unable to device register(%d)\n", ret);
		return ret;
    }	
    ret = platform_driver_register(&fan5405_user_space_driver);
    if (ret) {
		printk("****[fan5405_init] Unable to register driver (%d)\n", ret);
		return ret;
    }
#endif	
	return 0;		
}

static void __exit fan5405_exit(void)
{
	i2c_del_driver(&fan5405_driver);
}

module_init(fan5405_init);
module_exit(fan5405_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C fan5405 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");

