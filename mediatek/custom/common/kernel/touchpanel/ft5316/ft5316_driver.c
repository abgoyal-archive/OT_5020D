
 
#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "tpd_custom_ft5316.h"

//#ifdef MT6575
//#include <mach/mt6575_pm_ldo.h>
//#include <mach/mt6575_typedefs.h>
//#include <mach/mt6575_boot.h>
//#endif



//#ifdef MT6577
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
//#endif

#include "cust_gpio_usage.h"
#include <linux/kobject.h>
#include <linux/platform_device.h>

#define APK_UPGRADE

#ifdef APK_UPGRADE
#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#endif


extern struct tpd_device *tpd;
 
struct i2c_client *this_client = NULL;
extern struct task_struct *thread_dual;  //changed by zhengdao
static struct task_struct *charger_detect_thread = NULL;
static struct hrtimer charger_detect_timer;
static int charger_detect_flag = 0;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(charger_detect_waiter);
 
static void tpd_eint_interrupt_handler(void);
 
//#ifdef MT6575 
 extern void mt65xx_eint_unmask(unsigned int line);
 extern void mt65xx_eint_mask(unsigned int line);
 extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
 extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
 extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
									  kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
									  kal_bool auto_umask);
//#endif
#ifdef MT6577
	extern void mt65xx_eint_unmask(unsigned int line);
	extern void mt65xx_eint_mask(unsigned int line);
	extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
	extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
	extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
#endif

extern kal_bool upmu_is_chr_det(void);
 
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 

static int tpd_flag = 0;
static int point_num = 0;
static int p_point_num = 0;

//#define TPD_CLOSE_POWER_IN_SLEEP
#define TPD_OK 0
//register define

#define DEVICE_MODE 0x00
#define GEST_ID 0x01
#define TD_STATUS 0x02

#define TOUCH1_XH 0x03
#define TOUCH1_XL 0x04
#define TOUCH1_YH 0x05
#define TOUCH1_YL 0x06

#define TOUCH2_XH 0x09
#define TOUCH2_XL 0x0A
#define TOUCH2_YH 0x0B
#define TOUCH2_YL 0x0C

#define TOUCH3_XH 0x0F
#define TOUCH3_XL 0x10
#define TOUCH3_YH 0x11
#define TOUCH3_YL 0x12
//register define

#define TPD_RESET_ISSUE_WORKAROUND

#define TPD_MAX_RESET_COUNT 3

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#define VELOCITY_CUSTOM_FT5316
#ifdef VELOCITY_CUSTOM_FT5316
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

// for magnify velocity********************************************
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)

int g_v_magnify_x =TPD_VELOCITY_CUSTOM_X;
int g_v_magnify_y =TPD_VELOCITY_CUSTOM_Y;
static int tpd_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}
static int tpd_misc_release(struct inode *inode, struct file *file)
{
	//file->private_data = NULL;
	return 0;
}

static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	void __user *data;
	
	long err = 0;
	
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case TPD_GET_VELOCITY_CUSTOM_X:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

	   case TPD_GET_VELOCITY_CUSTOM_Y:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		default:
			printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


static struct file_operations tpd_fops = {
//	.owner = THIS_MODULE,
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

//**********************************************
#endif

static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
			.timing = this_client->timing,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
			.timing = this_client->timing,
		},
	};
	
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}

static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
			.timing = this_client->timing,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}

static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];

	buf[0] = addr;    //register address
	
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[0].timing = this_client->timing;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = buf;
	msgs[1].timing = this_client->timing;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	*pdata = buf[0];
	return ret;
  
}

static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
	return(ver);
}

static unsigned char ft5x0x_read_hw_type(void)
{
	unsigned char hw_type;
	ft5x0x_read_reg(FT5X0X_REG_HW, &hw_type);
	return(hw_type);
}

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL               0x0
#define FTS_TRUE               0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0x70

unsigned char tp_vendor;

void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}

FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;   
    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);
    if(ret <= 0)
    {
        printk("[FTS]i2c_read_interface error\n");
        return FTS_FALSE;
    }
  
    return FTS_TRUE;
}

FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret = i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        printk("[FTS]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{ 
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}


FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


#define    FTS_PACKET_LENGTH        128


static unsigned char CTPM_FW_MEGANE_FT6306_ID85[]=
{
	#include "Megane_ID85_V18_20130510_app.i"
};


static unsigned char CTPM_FW_MEGANE_FT6306_V13[]=
{
	#include "Megane_ID85_V18_20130510_app.i"
};

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE  bt_ecc;
    int       i_ret;

	this_client->timing = 400;	   //when in upgrade mode ,the clock shall be changed to 400
    /*********Step 1:Reset  CTPM *****/
	if(tp_vendor == 0x85)
	{
		/*write 0xaa to register 0xbc*/
		ft5x0x_write_reg(0xbc,0xaa);
		delay_qt_ms(80);
		/*write 0x55 to register 0xbc*/
		ft5x0x_write_reg(0xbc,0x55);
		printk("[FT6306] Step 1: Reset CTPM test\n");
		delay_qt_ms(30);
	}
	else
	{
		/*write 0xaa to register 0xfc*/
		ft5x0x_write_reg(0xfc,0xaa);
		delay_qt_ms(50);
		/*write 0x55 to register 0xfc*/
		ft5x0x_write_reg(0xfc,0x55);
		printk("[FT5316] Step 1: Reset CTPM test\n");
		delay_qt_ms(30);
	}

    /*********Step 2:Enter upgrade mode *****/
	printk("[FT5316] Step 2: Enter upgrade mode\n");
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        delay_qt_ms(5);
    }while(i_ret <= 0 && i < 5 );

	/*********Step 3:check READ-ID***********************/
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);

    if (reg_val[0] == 0x79 )
    {
	if(reg_val[1] == 0x07)
		printk("[FT5316] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	else if(reg_val[1] == 0x08)
		printk("[FT6306] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
        return ERR_READID;
        //i_is_new_protocol = 1;
    }
    cmd_write(0xcd,0x0,0x00,0x00,1);
    byte_read(reg_val,1);
    printk("[FT5316] bootloader version = 0x%x\n", reg_val[0]);

     /*********Step 4:erase app and panel paramenter area ********************/
    cmd_write(0x61,0x00,0x00,0x00,1);  //erase app area
    delay_qt_ms(1500); 
    cmd_write(0x63,0x00,0x00,0x00,1);  //erase panel parameter area
    delay_qt_ms(100);
    printk("[FT5316] Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[FT5316] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j = 0;j < packet_number;j ++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i = 0;i < FTS_PACKET_LENGTH;i ++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        delay_qt_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("[FT5316] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        delay_qt_ms(20);
    }

    //send the last six byte
    for (i = 0; i < 6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);  
        delay_qt_ms(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);
    printk("[FT5316] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

    msleep(300);  //make sure CTP startup normally
    
    return ERR_OK;
}


int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i;

    printk("[FT5316] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);  
    delay_qt_ms(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);  //write command to start calibration
    delay_qt_ms(300);
    for(i=0;i<100;i++)
    {
        ft5x0x_read_reg(0,&uc_temp);
        if ( ((uc_temp&0x70) >> 4) == 0x0)  //return to normal mode, calibration finish
        {
            break;
        }
        delay_qt_ms(50);
        printk("[FT5316] waiting calibration %d\n",i);
        
    }
    printk("[FT5316] calibration OK.\n");
    
    msleep(300);
    ft5x0x_write_reg(0, 0x40);  //goto factory mode
    delay_qt_ms(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);  //store CLB result
    delay_qt_ms(300);
    ft5x0x_write_reg(0, 0x0); //return to normal mode 
    msleep(300);
    printk("[FT5316] store CLB result OK.\n");

	this_client->timing = 100;   // return to normal clock hz
    return 0;
}

int fts_ctpm_fw_upgrade_with_i_file(void)
{
	FTS_BYTE*  pbt_buf = FTS_NULL;
	unsigned int ui_sz;
	int i_ret;

    //=ven========FW upgrade========================*/

		pbt_buf = CTPM_FW_MEGANE_FT6306_ID85;
		ui_sz = sizeof(CTPM_FW_MEGANE_FT6306_ID85);

   
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,ui_sz);
   if (i_ret != 0)
   {
       printk("[FT5316] upgrade failed i_ret = %d.\n", i_ret);
       //error handling ...
       //TBD
   }
   else
   {
       printk("[FT5316] upgrade successfully.\n");
	if(tp_vendor != 0x80)
       	fts_ctpm_auto_clb();  //start auto CLB
   }
   return i_ret;
}

unsigned char fts_ctpm_get_i_file_ver(void)
{
    unsigned int ui_sz;
		ui_sz = sizeof(CTPM_FW_MEGANE_FT6306_ID85);

    if (ui_sz > 2)
    {

	  return CTPM_FW_MEGANE_FT6306_ID85[ui_sz - 2];

    }
    else
        //TBD, error handling?
        return 0xff; //default value
}

#define    FTS_SETTING_BUF_LEN        128

int fts_ctpm_get_panel_factory_setting(void)    //return factory ID
{
    unsigned char uc_i2c_addr;             //I2C slave address (8 bit address)
    unsigned char uc_io_voltage;           //IO Voltage 0---3.3v;    1----1.8v
    unsigned char uc_panel_factory_id;     //TP panel factory ID

    unsigned char buf[FTS_SETTING_BUF_LEN];
    unsigned char reg_val[2] = {0};
    unsigned char  auc_i2c_write_buf[10];
    unsigned char  packet_buf[FTS_SETTING_BUF_LEN + 6];
	int reg_addr[] = {0xfc,0xbc};
	int delay_time1[] = {50,100};
	int delay_time2[] = {30,30};

    int i = 0;
	int j = 0;
    int i_ret;

    uc_i2c_addr = 0x70;
    uc_io_voltage = 0x0;
    uc_panel_factory_id = 0x5a;

    printk("[FT5316] Enter fts_ctpm_get_panel_factory_setting to get factory ID\n");

    this_client->timing = 400;       //when in upgrade mode ,the clock shall be changed to 400
    /*********Step 1:Reset  CTPM *****/
	for(j = 0;j < sizeof(reg_addr)/sizeof(reg_addr[0]);j++)
	{
		/*write 0xaa to register 0xfc(ft5316) or 0xbc(ft6306)*/
		ft5x0x_write_reg(reg_addr[j],0xaa);
		delay_qt_ms(delay_time1[j]);
		/*write 0x55 to register 0xfc(ft5316) or 0xbc(ft6306)*/
		ft5x0x_write_reg(reg_addr[j],0x55);
		printk("[FT5316] Step 1: Reset CTPM test\n");

		delay_qt_ms(delay_time2[j]);

		/*********Step 2:Enter upgrade mode *****/
		printk("[FT5316] Step 2: Enter upgrade mode\n");
		auc_i2c_write_buf[0] = 0x55;
		auc_i2c_write_buf[1] = 0xaa;
		do
		{
			i ++;
			i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
			delay_qt_ms(5);
		}while(i_ret <= 0 && i < 5 );
		i= 0;
		/*********Step 3:check READ-ID***********************/
		delay_qt_ms(10);
		cmd_write(0x90,0x00,0x00,0x00,4);
		byte_read(reg_val,2);
		if (reg_val[0] == 0x79)
		{
			if(reg_val[1] == 0x7)
				printk("[FT5316] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			else if(reg_val[1] == 0x8)
				printk("[FT6306] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			break;
		}
		else if(j == sizeof(reg_addr)/sizeof(reg_addr[0]) - 1)
			return ERR_READID;
	}

    //cmd_write(0xcd,0x0,0x00,0x00,1);
    //byte_read(reg_val,1);
    //printk("bootloader version = 0x%x\n", reg_val[0]);

    /* --------- read current project setting  ---------- */
    //set read start address
    buf[0] = 0x3;
    buf[1] = 0x0;
    buf[2] = 0x78;
    buf[3] = 0x0;
  //  i2c_master_send(i2c_client, &buf[0], 4);
    cmd_write(0x3,0x0,0x78,0x0,4);
    this_client->addr = this_client->addr & I2C_MASK_FLAG;
    i2c_master_recv(this_client, &buf, 8);

    printk("[FT5316] old setting: uc_i2c_addr = 0x%x, uc_io_voltage = %d, uc_panel_factory_id = 0x%x\n",
        buf[0],  buf[2], buf[4]);

    /********* reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

    msleep(200);

    this_client->timing = 100;     // return to normal clock hz
    return buf[4];

}
int fts_ctpm_auto_upg(void)
{
    unsigned char uc_host_fm_ver;
    unsigned char uc_tp_fm_ver;
    unsigned char version_list_megane_ft6306_ID85[] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17};	////Keep the ft6306 tp firmware old version list that allows to be updated.
    int i,i_ret = -1;

    uc_tp_fm_ver = ft5x0x_read_fw_ver();
	ft5x0x_read_reg(FT5X0X_REG_FT5316ID, &tp_vendor);
	
	if(tp_vendor != 0x85)
	{
		tp_vendor = fts_ctpm_get_panel_factory_setting();
	}


	if(tp_vendor == 0x85){
		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		printk("[FT6036] tp vendor is JUNDA(0x%x),uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",tp_vendor, uc_tp_fm_ver, uc_host_fm_ver);
		for(i = 0;i < sizeof(version_list_megane_ft6306_ID85)/sizeof(version_list_megane_ft6306_ID85[0]); i++)
		{
			if(uc_tp_fm_ver == version_list_megane_ft6306_ID85[i])
			{
				i_ret = fts_ctpm_fw_upgrade_with_i_file();
				if(!i_ret)
					printk("[FT6306]  tp firmware upgrade to new version 0x%x\n", uc_host_fm_ver);
				else
					printk("[FT6306]  tp firmware upgrade failed ret=%d.\n", i_ret);
				break;
			}
		}
	}
	
	else
		printk("[FT5316] Unknown tp vendor(0x%x).\n", uc_tp_fm_ver);
    return 0;
}


struct touch_info {
    int y[5];
    int x[5];
    int p[5];
    int id[5];
    int count;
};
 
static const struct i2c_device_id ft5316_tpd_id[] = {{"ft5316",0},{}};
static struct i2c_board_info __initdata ft5316_i2c_tpd={ I2C_BOARD_INFO("ft5316", (0x70>>1))};
 
 
static struct i2c_driver tpd_i2c_driver = {
	.driver = {
	.name = "ft5316",
//	 .owner = THIS_MODULE,
	},
	.probe = tpd_probe,
	.remove = __devexit_p(tpd_remove),
	.id_table = ft5316_tpd_id,
	.detect = tpd_detect,
//  .address_data = &addr_data,
};

static  void tpd_down(int x, int y, int p) {
	// input_report_abs(tpd->dev, ABS_PRESSURE, p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	//printk("D[%4d %4d %4d] ", x, y, p);
	 /* track id Start 0 */
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	input_mt_sync(tpd->dev);
	if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	{   
		tpd_button(x, y, 1);  
	}
	if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	{
		msleep(50);
		printk("D virtual key \n");
	}
	TPD_EM_PRINT(x, y, x, y, p-1, 1);
}
 
static  void tpd_up(int x, int y,int p) {    //changed by zhengdao
	//if(*count>0) {
	//input_report_abs(tpd->dev, ABS_PRESSURE, 0);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0); //added by zhengdao
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);  //added by zhengdao
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);  //added by zhengdao
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); //added by zhengdao
	//printk("U[%4d %4d %4d] ", x, y, 0);
	input_mt_sync(tpd->dev);
	TPD_EM_PRINT(x, y, x, y, 0, 0);
	// (*count)--;
     if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
     {   
        tpd_button(x, y, 0); 
     }   		 
}

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
	int i = 0;	
	char data[33] = {0};
    u16 high_byte,low_byte;
//	u8 report_rate = 0;
	u8 uc_reg_value;

	p_point_num = point_num;
    memcpy(pinfo, cinfo, sizeof(struct touch_info));  //added by zhengdao
    memset(cinfo, 0, sizeof(struct touch_info));     //added by zhengdao


	i2c_smbus_read_i2c_block_data(this_client, 0x00, 8, &(data[0]));
//	i2c_smbus_read_i2c_block_data(this_client, 0x88, 1, &report_rate);
	//TPD_DEBUG("FW version=%x]\n",data[24]);
	
	//TPD_DEBUG("received raw data from touch panel as following:\n");
	//TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
	//TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
	//TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);

	 //we have  to re update report rate
    // TPD_DMESG("report rate =%x\n",report_rate);
	/* Device Mode[2:0] == 0 :Normal operating Mode*/
	if((data[0] & 0x70) != 0) return false; 

	/*get the number of the touch points*/
	point_num= data[2] & 0x0f;

	if(point_num > 1)
	{
		i2c_smbus_read_i2c_block_data(this_client, 0x08, 8, &(data[8]));
	/*add ft6306 report point*/
		if(tp_vendor != 0x85)
		{
			i2c_smbus_read_i2c_block_data(this_client, 0x10, 8, &(data[16]));
			i2c_smbus_read_i2c_block_data(this_client, 0x18, 8, &(data[24]));
			i2c_smbus_read_i2c_block_data(this_client, 0x20, 1, &(data[32]));
		}
	}
	else if(point_num > 5)
	{
		printk("error: num is big than max num 5\n");
		return false;
	};
	//TPD_DEBUG("point_num =%d\n",point_num);
	
//	if(point_num == 0) return false;

	//TPD_DEBUG("Procss raw data...\n");
	for(i = 0; i < point_num; i++)
	{
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 
		cinfo->id[i] = data[3+6*i+2]>>4; //touch id
		/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] = high_byte |low_byte;
		//cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra
		
		/*get the Y coordinate, 2 bytes*/
			
		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;
		//cinfo->y[i]=  cinfo->y[i] * 800 >> 11;
		cinfo->count++;		
	}
//	printk(" cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
//	printk(" cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);		
//	printk(" cinfo->x[2]= %d, cinfo->y[2]= %d, cinfo->p[2] = %d\n", cinfo->x[2], cinfo->y[2], cinfo->p[2]);	  
	return true;
 };

 static int touch_event_handler(void *unused)
 {
	struct touch_info cinfo, pinfo;
	int i = 0;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	do
	{
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		set_current_state(TASK_INTERRUPTIBLE); 
		wait_event_interruptible(waiter,tpd_flag!=0);
						 
		tpd_flag = 0;
			 
		set_current_state(TASK_RUNNING);
		 
		if (tpd_touchinfo(&cinfo, &pinfo)) 
		{
		   //TPD_DEBUG("point_num = %d\n",point_num);
			TPD_DEBUG_SET_TIME;
			if(point_num >0) 
			{
			    for(i =0;i<point_num ; i++)
			    {
			         tpd_down(cinfo.x[i], cinfo.y[i], cinfo.id[i]);     
			    }
			    input_sync(tpd->dev);
			}
			else  
            {
			    //tpd_up(cinfo.x[0], cinfo.y[0], 0);  changed by zhengdao
                //TPD_DEBUG("release --->\n"); 
                //input_mt_sync(tpd->dev);
                 if(p_point_num > 0)  //added by zhengdao begin
				 {
					 for(i = 0; i < p_point_num; i++)
					 {
						 tpd_up(pinfo.x[i], pinfo.y[i], pinfo.id[i]);
					 }
               	}                     //added by zhengdao end
                input_sync(tpd->dev);
            }
        }

        if(tpd_mode==12)
        {
			//power down for desence debug
			//power off, need confirm with SA
			hwPowerDown(MT65XX_POWER_LDO_VGP2,  "TP");
			hwPowerDown(MT65XX_POWER_LDO_VGP,  "TP");
			msleep(20);
        }

 	}while(!kthread_should_stop());
	return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
 {
	strcpy(info->type, TPD_DEVICE);	
	return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
	 //TPD_DEBUG("TPD interrupt has been triggered\n");
	 tpd_flag = 1;
	 wake_up_interruptible(&waiter);
	 
 }

int charger_detect_thread_handler(void *unused)
{
    ktime_t ktime;
	u8 charger_in = 0;
    do
    {
    	ktime = ktime_set(1, 0);            
        wait_event_interruptible(charger_detect_waiter, charger_detect_flag != 0);
    
    	charger_detect_flag = 0;
        if( upmu_is_chr_det() == KAL_TRUE)
        {
        	if(charger_in == 0){
				charger_in = 1;
				i2c_smbus_write_i2c_block_data(this_client, FT5X0X_REG_CHARGER_DETECT, 1, &charger_in);
				printk("mtk_tpd: +++++ charger in: set FT5X0X_REG_CHARGER_DETECT to 0x%x ++++\n",charger_in);
        	}
        }
		else
		{
			if(charger_in == 1){
				charger_in = 0;
				i2c_smbus_write_i2c_block_data(this_client, FT5X0X_REG_CHARGER_DETECT, 1, &charger_in);
				printk("mtk_tpd: +++++ charger out: set FT5X0X_REG_CHARGER_DETECT to 0x%x ++++\n",charger_in);
			}
		}
        hrtimer_start(&charger_detect_timer, ktime, HRTIMER_MODE_REL);    	
	} while (!kthread_should_stop());
    
    return 0;
}

enum hrtimer_restart charger_detect(struct hrtimer *timer)
{
	charger_detect_flag = 1; 
	wake_up_interruptible(&charger_detect_waiter);
	
    return HRTIMER_NORESTART;
}
static ssize_t ft6306_show_version(struct device *dev,struct device_attribute *attr, char *buf)
{
	u8 uc_reg_value;
	i2c_smbus_read_i2c_block_data(this_client, FT5X0X_REG_FIRMID, 1, &uc_reg_value);
	return sprintf(buf,"mtk_tpd: Firmware version = 0x%02x\n", uc_reg_value);
}

static ssize_t ft6306_store_updata(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;
	FTS_BYTE*  pbt_buf = FTS_NULL;
	unsigned int ui_sz;
	int i_ret;
	sscanf(buf,"%d", &mode);
		printk("mode = %d\n",mode);
	if(!mode){
	//=ven========FW upgrade========================*/
		pbt_buf = CTPM_FW_MEGANE_FT6306_V13;
		ui_sz = sizeof(CTPM_FW_MEGANE_FT6306_V13);
	/*call the upgrade function*/
		i_ret =  fts_ctpm_fw_upgrade(pbt_buf,ui_sz);
	}
	if (i_ret != 0)
		printk("[FT5316] upgrade failed i_ret = %d.\n", i_ret);

	return count;
}

static DRIVER_ATTR(updata,    S_IWUSR | S_IRUGO,   NULL,  ft6306_store_updata);
static DRIVER_ATTR(version,   S_IWUSR | S_IRUGO,   ft6306_show_version, NULL);

#ifdef APK_UPGRADE
/*****************************************APK UPDRADE**************************************/
static struct mutex g_device_mutex;

static int ft5x0x_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	printk("[FT6306_llf] Get firmware size begain\n");
	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

static int ft5x0x_ReadFirmware(char *firmware_name,
			       unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	printk("[FT6306_llf] Read firmware begain!\n");
	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
				       char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret;
	int fwsize = ft5x0x_GetFirmwareSize(firmware_name);

	printk("[FT6306_llf] Get firmware size ok size=%d \n",fwsize);
	if (fwsize <= 0) {
		dev_err(&client->dev, "%s ERROR:Get firmware size failed\n",__func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 32 * 1024) {
		dev_dbg(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (ft5x0x_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n",__func__);
		kfree(pbt_buf);
		return -EIO;
	}
	printk("[FT6306_llf] Get firmware file ok\n");

	printk("[FT6306_llf] File just is 0x%x 0x%x 0x%x \n",(pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]),
		(pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]),(pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]));

	if ((pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]) == 0xFF
		&& (pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]) == 0xFF
		&& (pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]) == 0xFF) {
		/*call the upgrade function */
		printk("[FT6306_llf] Begain to Upgrade firmware \n");
		i_ret = fts_ctpm_fw_upgrade(pbt_buf, fwsize);
		if (i_ret != 0){
			kfree(pbt_buf);
			dev_dbg(&client->dev, "%s() - ERROR:[FTS] upgrade failed..\n",__func__);
			}
		else {
			kfree(pbt_buf);
			if(tp_vendor != 0x80)
				fts_ctpm_auto_clb();	/*start auto CLB*/
		 }
	} else {
		dev_dbg(&client->dev, "%s:FW format error\n", __func__);
		printk("[FT6306_llf] Firmware file format error\n");
		kfree(pbt_buf);
		return -EIO;
	}
	return i_ret;
}

static ssize_t ft5x0x_fwupgradeapp_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}


/*upgrade from app.bin*/
static ssize_t ft5x0x_fwupgradeapp_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	unsigned char fm_ver;

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count - 1] = '\0';

	printk("[FT6306_llf]Input file name is %s \n",fwname);

	fm_ver = ft5x0x_read_fw_ver();
	printk("[FT6306_llf] current firmware version is  0x%x\n", fm_ver);

	ft5x0x_read_reg(0xA8, &tp_vendor);
	//ID detect
	if(tp_vendor != 0x85){
		//destory #######################################
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		printk("[%s] can't find ft6306,vendor:0x%x\n",__func__,tp_vendor);
		return -1;
	}

	mutex_lock(&g_device_mutex);
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	fts_ctpm_fw_upgrade_with_app_file(client, fwname);

	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	mutex_unlock(&g_device_mutex);

	printk("[FT6306_llf] Firmware Upgrade Successful!\n");

	fm_ver = ft5x0x_read_fw_ver();

	printk("[FT6306_llf]  After Upgrade firmware version is  0x%x\n", fm_ver);

	return count;
}
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, ft5x0x_fwupgradeapp_show,
			ft5x0x_fwupgradeapp_store);
#endif


static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0,ret;
	int reset_count = 0;
	u8 uc_reg_value; 

reset_proc:   
	this_client = client;

#if 0  
#ifdef MT6575
    //power on, need confirm with SA
    hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
    hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");      
#endif	

#ifdef MT6577
		//power on, need confirm with SA
		hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
		hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");	  
#endif	

	#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
	hwPowerDown(TPD_POWER_SOURCE,"TP");
	hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
	msleep(100);
	#else
	#ifdef MT6573
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
  mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
	msleep(100);
	#endif
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif
#endif
	printk("xxxxxxxxxxxxxxxxx%sxxxxxxxxxxxxxxxxxx\n", __FUNCTION__);
	mdelay(10);
	hwPowerOn(MT65XX_POWER_LDO_VGP2,VOL_2800,"MSG_TP");
	mdelay(10);


	//rst pin
	mt_set_gpio_mode(GPIO17, 0);
	mt_set_gpio_dir(GPIO17, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO17, GPIO_OUT_ONE);

	msleep(10);		
    mt_set_gpio_out(GPIO17, GPIO_OUT_ZERO);
	msleep(20);	
    mt_set_gpio_out(GPIO17, GPIO_OUT_ONE);
	msleep(200);     //add for timing settting.
	//enable
	//mt_set_gpio_mode(GPIO12, 0);
	//mt_set_gpio_dir(GPIO12, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO12, GPIO_OUT_ONE);
    mdelay(20);
	//wake
	//mt_set_gpio_mode(GPIO52, 0);
	//mt_set_gpio_dir(GPIO52, GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO52, GPIO_OUT_ONE);

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1); 
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
 
	msleep(100);
 
	if((i2c_smbus_read_i2c_block_data(this_client, 0x00, 1, &data))< 0)
	{
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
#ifdef TPD_RESET_ISSUE_WORKAROUND
        if ( reset_count < TPD_MAX_RESET_COUNT )
        {
            reset_count++;
            goto reset_proc;
        }
#endif
		return -1; 
	}
    //get some register information
    ret = i2c_smbus_read_i2c_block_data(this_client, FT5X0X_REG_FIRMID, 1, &uc_reg_value);
    printk("mtk_tpd: Firmware version = 0x%x\n", uc_reg_value);
	ret = i2c_smbus_read_i2c_block_data(this_client, FT5X0X_REG_PERIODACTIVE, 1, &uc_reg_value);
    printk("mtk_tpd: report rate is %dHz.\n", uc_reg_value * 10);
	ret = i2c_smbus_read_i2c_block_data(this_client, FT5X0X_REG_THGROUP, 1, &uc_reg_value);
    printk("mtk_tpd: touch threshold is %d.\n", uc_reg_value * 4);
	ret = i2c_smbus_read_i2c_block_data(this_client, FT5X0X_REG_FT5316ID, 1, &uc_reg_value);
    if(uc_reg_value == 0x85)
	{
		printk("mtk_tpd: tp vendor is JUNDA.\n");
	}
	else
		printk("mtk_tpd: tp vendor is 0x%02x which is unknown.\n",uc_reg_value);

	 fts_ctpm_auto_upg();

	tpd_load_status = 1;

#ifdef VELOCITY_CUSTOM_FT5316
	if((err = misc_register(&tpd_misc_device)))
	{
		printk("mtk_tpd: tpd_misc_device register failed\n");	
	}
#endif

	thread_dual = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread_dual))
	{ 
		retval = PTR_ERR(thread_dual);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread_dual: %d\n", retval);
	}

	err = device_create_file(&client->dev, &driver_attr_updata);
	err = device_create_file(&client->dev, &driver_attr_version);

	ktime_t ktime;
	ktime = ktime_set(1, 0);
	hrtimer_init(&charger_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    charger_detect_timer.function = charger_detect;    
    hrtimer_start(&charger_detect_timer, ktime, HRTIMER_MODE_REL);

    charger_detect_thread = kthread_run(charger_detect_thread_handler, 0, "mtk_tpd charger_detect_thread");
    if (IS_ERR(charger_detect_thread))
    {
		retval = PTR_ERR(charger_detect_thread);
		TPD_DMESG(TPD_DEVICE " failed to create mtk_tpd charger_detect_thread: %d\n", retval);
    }

#ifdef APK_UPGRADE
	mutex_init(&g_device_mutex);
	if(device_create_file(&this_client->dev, &dev_attr_ftsfwupgradeapp)>=0)
		{
			printk("device_create_file_ftsfwupgradeapp \n");
		}
#endif
	
	TPD_DMESG("ft5316 Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	return 0;  
}

static int __devexit tpd_remove(struct i2c_client *client)
{
	TPD_DEBUG("TPD removed\n");
	return 0;
}
 
static int tpd_local_init(void)
{
  	TPD_DMESG("Focaltech FT5316 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
   	if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("ft5316 unable to add i2c driver.\n");
      	return -1;
    }
    if(tpd_load_status == 0) 
    {
    	TPD_DMESG("ft5316 add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
	
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;
    return 0; 
}

static void tpd_resume( struct early_suspend *h )
{
  //int retval = TPD_OK;
   printk("xxxxxxxxxxx%sxxxxxxxxx\n", __FUNCTION__);
//rst pin
	mt_set_gpio_mode(GPIO17, 0);
	mt_set_gpio_dir(GPIO17, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO17, GPIO_OUT_ONE);
	mt_set_gpio_out(GPIO17, GPIO_OUT_ZERO);
	mdelay(5);
//rst pin
//	mt_set_gpio_mode(GPIO17, 0);
//	mt_set_gpio_dir(GPIO17, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO17, GPIO_OUT_ONE);

#if 0
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP"); 
#else
#ifdef MT6573
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#endif	
	msleep(100);

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(1);  
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
#endif
#endif
   mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	 //return retval;
}

static void tpd_suspend( struct early_suspend *h )
{
	// int retval = TPD_OK;
	static char data = 0x3;
	printk("xxxxxxxxxxx%sxxxxxxxxx\n", __FUNCTION__);
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	i2c_smbus_write_i2c_block_data(this_client, 0xA5, 1, &data);  //TP enter sleep mode

#if 0
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");
#else
i2c_smbus_write_i2c_block_data(this_client, 0xA5, 1, &data);  //TP enter sleep mode
#ifdef MT6573
mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
#endif

#endif
#endif
	 //return retval;
} 


static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "FT5316",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif		
 };
 /* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	printk("MediaTek FT5316 touch panel driver init\n");
	i2c_register_board_info(0, &ft5316_i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FT5316 driver failed\n");
	 return 0;
}
 
 /* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek FT5316 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

