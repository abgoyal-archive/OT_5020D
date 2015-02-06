
 
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

#include "tpd_custom_mstar.h"
//#ifdef MT6575
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
//#endif
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include "cust_gpio_usage.h"

#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
#define FW_UPDATE_ADDR_MSG21XX   (0x92>>1)
#define TP_DEBUG	printk

volatile static u8 Fmr_Loader[1024];

extern struct tpd_device *tpd;
 
struct i2c_client *i2c_client = NULL;
 struct task_struct *thread_dual = NULL;  //changed by zhengdao

#define TP_FIRMWARE_UPDATE
//#define EN_PIN_1_8_V
#define GPIO_CTP_MSG2133_EN_PIN           GPIO_CTP_RST_PIN
#define GPIO_CTP_MSG2133_EN_PIN_M_GPIO    GPIO_MODE_00

#define GPIO_CTP_MSG2133_EINT_PIN           GPIO_CTP_EINT_PIN
#define GPIO_CTP_MSG2133_EINT_PIN_M_GPIO   GPIO_CTP_EINT_PIN_M_EINT

#define TPD_POINT_INFO_LEN      8
#define TPD_MAX_POINTS          2


static DECLARE_WAIT_QUEUE_HEAD(waiter);
 
 
static void tpd_eint_interrupt_handler(void);
 
 
 extern void mt65xx_eint_unmask(unsigned int line);
 extern void mt65xx_eint_mask(unsigned int line);
 extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
 extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
 extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
									  kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
									  kal_bool auto_umask);

 
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
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
static u8 auto_updata = 0;

//update tp firmware v2.04 for optimize performance
unsigned char MSG_FIRMWARE_MEGANE_V204[94*1024] =
{
#include "Megane-jdc-v2.04.h"
};

unsigned char MSG_FIRMWARE_MEGANE_YJV06[94*1024] =
{
#include "MSG2133_YJ_TCL_Megane_V0x0314.0x0006.h"
};

//wangdongliang 2012 for mstar
#define u8         unsigned char
#define u32        unsigned int
#define s32        signed int

#define MAX_TOUCH_FINGER 2
typedef struct
{
    u16 X;
    u16 Y;
} TouchPoint_t;

typedef struct
{
    u8 nTouchKeyMode;
    u8 nTouchKeyCode;
    u8 nFingerNum;
    TouchPoint_t Point[MAX_TOUCH_FINGER];
} TouchScreenInfo_t;



#define REPORT_PACKET_LENGTH    (8)
#define MSG21XX_INT_GPIO       (42)
#define MSG21XX_RESET_GPIO     (22)
#define MS_TS_MSG21XX_X_MAX   (480)
#define MS_TS_MSG21XX_Y_MAX   (800)

//end wangdongliang

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

struct touch_info {
    int y[3];
    int x[3];
    int p[3];
    int count;
};
 
 static const struct i2c_device_id tpd_id[] = {{TPD_DEVICE,0},{}};
 //unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
 //static const unsigned short * const forces[] = { force, NULL };
 //static struct i2c_client_address_data addr_data = { .forces = forces, };
 static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("mtk-tpd", (0x4c>>1))};
 
 
 static struct i2c_driver tpd_i2c_driver = {
  .driver = {
	 .name = TPD_DEVICE,
//	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = tpd_id,
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
	 input_mt_sync(tpd->dev);
   if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
   {   
       tpd_button(x, y, 1);  
   }	 
	 TPD_EM_PRINT(x, y, x, y, 0, 1);
 }
 
static  void tpd_up(int x, int y,int *count) {
	 //if(*count>0) {
		 //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);

		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_report_key(tpd->dev, KEY_HOME, 0);
		input_report_key(tpd->dev, KEY_SEARCH, 0);

		 //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //printk("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 TPD_EM_PRINT(x, y, x, y, 0, 0);
	//	 (*count)--;
     if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
     {   
        tpd_button(x, y, 0); 
     }   		 

 }

 static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
 {

	int i = 0;
	
	char data[30] = {0};

    u16 high_byte,low_byte;
	u8 report_rate =0;

	p_point_num = point_num;

	i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(data[0]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(data[8]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(data[16]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0xa6, 1, &(data[24]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x88, 1, &report_rate);
	TPD_DEBUG("FW version=%x]\n",data[24]);
	
	TPD_DEBUG("received raw data from touch panel as following:\n");
	TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
	TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
	TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);


    //    
	 //we have  to re update report rate
    // TPD_DMESG("report rate =%x\n",report_rate);
	 if(report_rate < 8)
	 {
	   report_rate = 0x8;
	   if((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate))< 0)
	   {
		   TPD_DMESG("I2C read report rate error, line: %d\n", __LINE__);
	   }
	 }
	 
	
	/* Device Mode[2:0] == 0 :Normal operating Mode*/
	if(data[0] & 0x70 != 0) return false; 

	/*get the number of the touch points*/
	point_num= data[2] & 0x0f;
	
	TPD_DEBUG("point_num =%d\n",point_num);
	
//	if(point_num == 0) return false;

	   TPD_DEBUG("Procss raw data...\n");

		
		for(i = 0; i < point_num; i++)
		{
			cinfo->p[i] = data[3+6*i] >> 6; //event flag 

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
		TPD_DEBUG(" cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
		TPD_DEBUG(" cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);		
		TPD_DEBUG(" cinfo->x[2]= %d, cinfo->y[2]= %d, cinfo->p[2] = %d\n", cinfo->x[2], cinfo->y[2], cinfo->p[2]);	
		  
	 return true;

 };



 //wangdongliang for mstar tp
 static u8 Calculate_8BitsChecksum( u8 *msg, s32 s32Length )
 {
	 s32 s32Checksum = 0;
	 s32 i;
 
	 for ( i = 0 ; i < s32Length; i++ )
	 {
		 s32Checksum += msg[i];
	 }
 
	 return (u8)( ( -s32Checksum ) & 0xFF );
 }
 

 static void msg21xx_data_disposal(void)
 {
       u8 val[8] = {0};
       u8 Checksum = 0;
	u8 i;
	u32 delta_x = 0, delta_y = 0;
	u32 u32X = 0;
	u32 u32Y = 0;
	u8 touchkeycode = 0;
	TouchScreenInfo_t  touchData;
	static u32 preKeyStatus=0;
	//static u32 preFingerNum=0;

#define SWAP_X_Y   (1)
//#define FLIP_X         (1)
//#define FLIP_Y         (1)
#ifdef SWAP_X_Y
	int tempx;
	int tempy;
#endif
	static u16 test_x = 0;
	static u16 test_y = 0;


	i2c_master_recv(i2c_client,&val[0],REPORT_PACKET_LENGTH);
     Checksum = Calculate_8BitsChecksum(&val[0], 7); //calculate checksum
    if ((Checksum == val[7]) && (val[0] == 0x52))   //check the checksum  of packet
    {
        u32X = (((val[1] & 0xF0) << 4) | val[2]);         //parse the packet to coordinates
        u32Y = (((val[1] & 0x0F) << 8) | val[3]);

        delta_x = (((val[4] & 0xF0) << 4) | val[5]);
        delta_y = (((val[4] & 0x0F) << 8) | val[6]);

#ifdef SWAP_X_Y
		tempy = u32X;
		tempx = u32Y;
        u32X = tempx;
        u32Y = tempy;

		tempy = delta_x;
		tempx = delta_y;
        delta_x = tempx;
        delta_y = tempy;
#endif
#ifdef FLIP_X
       u32X = 2047 - u32X;
       delta_x = 4095 -delta_x;
#endif
#ifdef FLIP_Y
       u32Y = 2047 - u32Y;
       delta_y = 4095 -delta_y;
#endif
      //printk("[HAL] u32X = %x, u32Y = %x", u32X, u32Y);
      //printk("[HAL] delta_x = %x, delta_y = %x\n", delta_x, delta_y);

        if ((val[1] == 0xFF) && (val[2] == 0xFF) && (val[3] == 0xFF) && (val[4] == 0xFF) && (val[6] == 0xFF))
        {
            touchData.Point[0].X = 0; // final X coordinate
            touchData.Point[0].Y = 0; // final Y coordinate

           if((val[5]==0x0)||(val[5]==0xFF))
            {
                touchData.nFingerNum = 0; //touch end
                touchData.nTouchKeyCode = 0; //TouchKeyMode
                touchData.nTouchKeyMode = 0; //TouchKeyMode
            }
            else
            {
                touchData.nTouchKeyMode = 1; //TouchKeyMode
				        touchData.nTouchKeyCode = val[5]; //TouchKeyCode
                touchData.nFingerNum = 1;
            }
        }
		else
		{
		    touchData.nTouchKeyMode = 0; //Touch on screen...

			if (
#ifdef FLIP_X
					(delta_x == 4095)
#else
					(delta_x == 0)
#endif
					&&
#ifdef FLIP_Y
					(delta_y == 4095)
#else
					(delta_y == 0)
#endif
			   )
			{
				touchData.nFingerNum = 1; //one touch
				touchData.Point[0].X = (u32X * MS_TS_MSG21XX_X_MAX) / 2048;
				touchData.Point[0].Y = (u32Y * MS_TS_MSG21XX_Y_MAX) / 2048;
			}
			else
			{
				u32 x2, y2;
				touchData.nFingerNum = 2; //two touch

				/* Finger 1 */
				touchData.Point[0].X = (u32X * MS_TS_MSG21XX_X_MAX) / 2048;
				touchData.Point[0].Y = (u32Y * MS_TS_MSG21XX_Y_MAX) / 2048;

				/* Finger 2 */
				if (delta_x > 2048)     //transform the unsigh value to sign value
				{
					delta_x -= 4096;
				}
				if (delta_y > 2048)
				{
					delta_y -= 4096;
				}

				x2 = (u32)(u32X + delta_x);
				y2 = (u32)(u32Y + delta_y);

				touchData.Point[1].X = (x2 * MS_TS_MSG21XX_X_MAX) / 2048;
				touchData.Point[1].Y = (y2 * MS_TS_MSG21XX_Y_MAX) / 2048;
			}
		}
		
		//report...
		if(touchData.nTouchKeyMode)
		{
			if (touchData.nTouchKeyCode == 1)
				touchkeycode = KEY_BACK;
			if (touchData.nTouchKeyCode == 2)
				touchkeycode = KEY_HOMEPAGE;
			if (touchData.nTouchKeyCode == 4)
				touchkeycode = KEY_MENU;
			if (touchData.nTouchKeyCode == 8)
				touchkeycode = KEY_HOME;
			

			if(preKeyStatus!=touchkeycode)
			{
				preKeyStatus=touchkeycode;
				input_report_key(tpd->dev, touchkeycode, 1);
			}
			input_sync(tpd->dev);
		}
        else
        {
		    preKeyStatus=0; //clear key status..

            if((touchData.nFingerNum) == 0)   //touch end
            {
 //           printk("xxxx release xxxx\n");
				//preFingerNum=0;
				input_report_key(tpd->dev, KEY_MENU, 0);
				input_report_key(tpd->dev, KEY_HOME, 0);
				input_report_key(tpd->dev, KEY_BACK, 0);
				input_report_key(tpd->dev, KEY_HOMEPAGE, 0);


				input_report_key(tpd->dev, BTN_TOUCH, 0);
//				input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
				input_mt_sync(tpd->dev);
				input_sync(tpd->dev);
            }
            else //touch on screen
            {
			    /*
				if(preFingerNum!=touchData.nFingerNum)   //for one touch <--> two touch issue
				{
					printk("langwenlong number has changed\n");
					preFingerNum=touchData.nFingerNum;
					input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
				    input_mt_sync(tpd->dev);
				    input_sync(tpd->dev);
				}*/
		if((touchData.nFingerNum == 1) && (test_x == touchData.Point[0].X) && (test_y == touchData.Point[0].Y))
			;
		else
			{
				for(i = 0;i < (touchData.nFingerNum);i++)
					{
						input_report_key(tpd->dev, BTN_TOUCH, 1);
						input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
						input_report_abs(tpd->dev, ABS_MT_POSITION_X, touchData.Point[i].X);
						input_report_abs(tpd->dev, ABS_MT_POSITION_Y, touchData.Point[i].Y);
						input_mt_sync(tpd->dev);
					}

				input_sync(tpd->dev);
			}
			}
		test_x = touchData.Point[0].X;
		test_y = touchData.Point[0].Y;
		}
    }
    else
    {
		printk(KERN_ERR "err status in tp\n");
    }
 }
//end wangdongliang

 static int touch_event_handler(void *unused)
 {
  
    struct touch_info cinfo, pinfo;

	 struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	 sched_setscheduler(current, SCHED_RR, &param);
 
	 do
	 {
	  mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		 set_current_state(TASK_INTERRUPTIBLE); 
		  wait_event_interruptible(waiter,tpd_flag!=0);
						 
			 tpd_flag = 0;
			 
		 set_current_state(TASK_RUNNING);

		 msg21xx_data_disposal();

 }while(!kthread_should_stop());
 
	 return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, int kind, struct i2c_board_info *info) 
 {
	 strcpy(info->type, TPD_DEVICE);	
	  return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
	 tpd_flag = 1;
	 wake_up_interruptible(&waiter);
	 
 }
static void MSG2133_En_Pin_Out(u8 flag)
{
	if(flag==1)
	{
#ifdef EN_PIN_1_8_V
		mt_set_gpio_mode(GPIO_CTP_MSG2133_EN_PIN, GPIO_CTP_MSG2133_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_MSG2133_EN_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_CTP_MSG2133_EN_PIN, GPIO_PULL_DISABLE);
		msleep(10);
		if(mt_get_gpio_in(GPIO_CTP_MSG2133_EN_PIN)==0)
		{
			mt_set_gpio_dir(GPIO_CTP_MSG2133_EN_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_CTP_MSG2133_EN_PIN, GPIO_OUT_ONE);
			mt_set_gpio_pull_enable(GPIO_CTP_MSG2133_EN_PIN, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(GPIO_CTP_MSG2133_EN_PIN, GPIO_PULL_UP);
		}
#else
		mt_set_gpio_mode(GPIO_CTP_MSG2133_EN_PIN, GPIO_CTP_MSG2133_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_MSG2133_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_MSG2133_EN_PIN, GPIO_OUT_ONE);
#endif
	}
	else
	{
		mt_set_gpio_mode(GPIO_CTP_MSG2133_EN_PIN, GPIO_CTP_MSG2133_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_MSG2133_EN_PIN, GPIO_DIR_OUT);
#ifdef EN_PIN_1_8_V
		mt_set_gpio_pull_enable(GPIO_CTP_MSG2133_EN_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_pull_select(GPIO_CTP_MSG2133_EN_PIN, GPIO_PULL_DOWN);
#endif
		mt_set_gpio_out(GPIO_CTP_MSG2133_EN_PIN, GPIO_OUT_ZERO);
	}
}



#ifdef TP_FIRMWARE_UPDATE
/*FW UPDATE*/
#define DBG 		//printk
#define FW_ADDR_MSG2133   0x62//device address of msg2133
#define FW_UPDATE_ADDR_MSG2133   0x49
#define   DOWNLOAD_FIRMWARE_BUF_SIZE   94*1024
static U8 *download_firmware_buf = NULL;
static int FwDataCnt;
struct class *firmware_class;
struct device *firmware_cmd_dev;
static  char *fw_version;
static int update_switch = 0;

#define MAX_CMD_LEN 255
static int i2c_master_send_ext(struct i2c_client *client,const char *buf ,int count)
{
	u8 *buf_dma = NULL;
	u32 old_addr = 0;
	int ret = 0;
	int retry = 3;

	if (count > MAX_CMD_LEN) {
		printk("[i2c_master_send_ext] exceed the max write length \n");
		return -1;
	}

	buf_dma= kmalloc(count,GFP_KERNEL);

	old_addr = client->addr;
	client->addr |= I2C_ENEXT_FLAG ;

	memcpy(buf_dma, buf, count);

	do {
		ret = i2c_master_send(client, (u8*)buf_dma, count);
		retry --;
		if (ret != count) {
			printk("[i2c_master_send_ext] Error sent I2C ret = %d\n", ret);
		}
	}while ((ret != count) && (retry > 0));

	client->addr = old_addr;
	kfree(buf_dma);

	return ret;

}


static int i2c_master_recv_ext(struct i2c_client *client, char *buf ,int count)
{
	u32 phyAddr = 0;
	u8  buf_dma[8] = {0};
	u32 old_addr = 0;
	int ret = 0;
	int retry = 3;
	int i = 0;
	u8  *buf_test ;
	buf_test = &buf_dma[0];

	old_addr = client->addr;
	client->addr |= I2C_ENEXT_FLAG ;

	DBG("[i2c_master_recv_ext] client->addr = %x\n", client->addr);

	do {
		ret = i2c_master_recv(client, buf_dma, count);
		retry --;
		if (ret != count) {
			DBG("[i2c_master_recv_ext] Error sent I2C ret = %d\n", ret);
		}
	}while ((ret != count) && (retry > 0));

	memcpy(buf, buf_dma, count);

	client->addr = old_addr;

	return ret;
}
void HalTscrCDevWriteI2CSeq(u8 addr, u8* data, u16 size)
{
	u8 addr_bak;

	addr_bak = i2c_client->addr;
	i2c_client->addr = addr;
	i2c_client->addr |= I2C_ENEXT_FLAG;
	i2c_master_send_ext(i2c_client,data,size);
	i2c_client->addr = addr_bak;
}


void HalTscrCReadI2CSeq(u8 addr, u8* read_data, u8 size)
{
	u8 addr_bak;

	addr_bak = i2c_client->addr;
	i2c_client->addr = addr;
	i2c_client->addr |= I2C_ENEXT_FLAG;
	i2c_master_recv_ext(i2c_client,read_data,size);
	i2c_client->addr = addr_bak;
}


void Get_Chip_Version(void)
{

	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[2];

	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0xCE;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, &dbbus_tx_data[0], 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG2133, &dbbus_rx_data[0], 2);
	if (dbbus_rx_data[1] == 0)
	{
		// it is Catch2
		DBG("*** Catch2 ***\n");
		//FwVersion  = 2;// 2 means Catch2
	}
	else
	{
		// it is catch1
		DBG("*** Catch1 ***\n");
		//FwVersion  = 1;// 1 means Catch1
	}

}

void dbbusDWIICEnterSerialDebugMode()
{
	U8 data[5];

	// Enter the Serial Debug Mode
	data[0] = 0x53;
	data[1] = 0x45;
	data[2] = 0x52;
	data[3] = 0x44;
	data[4] = 0x42;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 5);
}

void dbbusDWIICStopMCU()
{
	U8 data[1];

	// Stop the MCU
	data[0] = 0x37;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);
}

void dbbusDWIICIICUseBus()
{
	U8 data[1];

	// IIC Use Bus
	data[0] = 0x35;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);
}

void dbbusDWIICIICReshape()
{
	U8 data[1];

	// IIC Re-shape
	data[0] = 0x71;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);
}

void dbbusDWIICIICNotUseBus()
{
	U8 data[1];

	// IIC Not Use Bus
	data[0] = 0x34;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);
}

void dbbusDWIICNotStopMCU()
{
	U8 data[1];

	// Not Stop the MCU
	data[0] = 0x36;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);
}

void dbbusDWIICExitSerialDebugMode()
{
	U8 data[1];

	// Exit the Serial Debug Mode
	data[0] = 0x45;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, data, 1);

	// Delay some interval to guard the next transaction
	//udelay ( 200 );        // delay about 0.2ms
}

void drvISP_EntryIspMode(void)
{
	U8 bWriteData[5] =
	{
		0x4D, 0x53, 0x54, 0x41, 0x52
	};

	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 5);

	//udelay ( 1000 );        // delay about 1ms
}

U8 drvISP_Read(U8 n, U8* pDataToRead)    //First it needs send 0x11 to notify we want to get flash data back.
{
	U8 Read_cmd = 0x11;
	unsigned char dbbus_rx_data[2] = {0};
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &Read_cmd, 1);
	udelay(100);//10 zzf
	if (n == 1)
	{
		HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG2133, &dbbus_rx_data[0], 2);
		*pDataToRead = dbbus_rx_data[0];
	}
	else
		HalTscrCReadI2CSeq(FW_UPDATE_ADDR_MSG2133, pDataToRead, n);

}

void drvISP_WriteEnable(void)
{
	U8 bWriteData[2] =
	{
		0x10, 0x06
	};
	U8 bWriteData1 = 0x12;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);
}


void drvISP_ExitIspMode(void)
{
	U8 bWriteData = 0x24;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData, 1);
}

U8 drvISP_ReadStatus()
{
	U8 bReadData = 0;
	U8 bWriteData[2] =
	{
		0x10, 0x05
	};
	U8 bWriteData1 = 0x12;

	udelay(100);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 2);
	udelay(100);
	drvISP_Read(1, &bReadData);
	mdelay(10);//3->10 zzf
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);
	return bReadData;
}


void drvISP_BlockErase(U32 addr)
{
	U8 bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	U8 bWriteData1 = 0x12;
	u32 timeOutCount=0;
	drvISP_WriteEnable();

	//Enable write status register
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x50;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);

	//Write Status
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x01;
	bWriteData[2] = 0x00;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 3);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);

	//Write disable
	bWriteData[0] = 0x10;
	bWriteData[1] = 0x04;
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);

	udelay(100);
	timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 10000 )
			break; /* around 1 sec timeout */
	}
	drvISP_WriteEnable();

	drvISP_ReadStatus();//zzf

	bWriteData[0] = 0x10;
	bWriteData[1] = 0xc7;//0xD8;        //Block Erase
	//bWriteData[2] = ((addr >> 16) & 0xFF) ;
	//bWriteData[3] = ((addr >> 8) & 0xFF) ;
	//bWriteData[4] = (addr & 0xFF) ;
	//HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData, 5);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, bWriteData, 2);
	HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);

	udelay(100);
	timeOutCount=0;
	while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
	{
		timeOutCount++;
		if ( timeOutCount >= 50000 )
			break; /* around 5 sec timeout */
	}
}

void drvISP_Program(U16 k, U8* pDataToWrite)
{
	U16 i = 0;
	U16 j = 0;
	//U16 n = 0;
	U8 TX_data[133];
	U8 bWriteData1 = 0x12;
	U32 addr = k * 1024;
	U32 timeOutCount = 0;

	for (j = 0; j < 8; j++)   //256*4 cycle
	{
		TX_data[0] = 0x10;
		TX_data[1] = 0x02;// Page Program CMD
		TX_data[2] = (addr + 128 * j) >> 16;
		TX_data[3] = (addr + 128 * j) >> 8;
		TX_data[4] = (addr + 128 * j);
		for (i = 0; i < 128; i++)
		{
			TX_data[5 + i] = pDataToWrite[j * 128 + i];
		}

		udelay(100);
		timeOutCount=0;
		while ( ( drvISP_ReadStatus() & 0x01 ) == 0x01 )
		{
			timeOutCount++;
			if ( timeOutCount >= 10000 )
				break; /* around 1 sec timeout */
		}

		drvISP_WriteEnable();
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, TX_data, 133);   //write 256 byte per cycle
		HalTscrCDevWriteI2CSeq(FW_UPDATE_ADDR_MSG2133, &bWriteData1, 1);
	}
}

void masterBUT_LoadFwToTarget(unsigned char *pfirmware, long n)
{
	U8 i;
	unsigned short j;
	U8 dbbus_tx_data[4];
	unsigned char dbbus_rx_data[2] = {0};
	int fd;
	//add mask interrupt
	update_switch = 1;
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, 			NULL, 1);

	MSG2133_En_Pin_Out(0);
	mdelay(10);
	MSG2133_En_Pin_Out(1);
	mdelay(300);
	drvISP_EntryIspMode();
	drvISP_BlockErase(0x00000);
	mdelay(300);
	MSG2133_En_Pin_Out(0);
	mdelay(10);
	MSG2133_En_Pin_Out(1);

	mdelay(300);

#if 1
	// Enable slave's ISP ECO mode
	dbbusDWIICEnterSerialDebugMode();
	dbbusDWIICStopMCU();
	dbbusDWIICIICUseBus();
	dbbusDWIICIICReshape();

	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x08;
	dbbus_tx_data[2] = 0x0c;
	dbbus_tx_data[3] = 0x08;

	// Disable the Watchdog
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);


	//Get_Chip_Version();

	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	dbbus_tx_data[3] = 0x00;

	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x60;
	dbbus_tx_data[3] = 0x55;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x3C;
	dbbus_tx_data[2] = 0x61;
	dbbus_tx_data[3] = 0xAA;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	//Stop MCU
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x0F;
	dbbus_tx_data[2] = 0xE6;
	dbbus_tx_data[3] = 0x01;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	//Enable SPI Pad
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG2133, &dbbus_rx_data[0], 2);
	DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);  //Set Bit 5
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x25;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG2133, &dbbus_rx_data[0], 2);
	DBG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xFC;  //Clear Bit 1,0
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	//WP overwrite
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x0E;
	dbbus_tx_data[3] = 0x02;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	//set pin high
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x1E;
	dbbus_tx_data[2] = 0x10;
	dbbus_tx_data[3] = 0x08;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	//set FRO to 50M
	dbbus_tx_data[0] = 0x10;
	dbbus_tx_data[1] = 0x11;
	dbbus_tx_data[2] = 0xE2;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 3);
	dbbus_rx_data[0] = 0;
	dbbus_rx_data[1] = 0;
	HalTscrCReadI2CSeq(FW_ADDR_MSG2133, &dbbus_rx_data[0], 2);
	//printk("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
	dbbus_tx_data[3] = dbbus_rx_data[0] & 0xF7;  //Clear Bit 1,0
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG2133, dbbus_tx_data, 4);

	dbbusDWIICIICNotUseBus();
	dbbusDWIICNotStopMCU();
	dbbusDWIICExitSerialDebugMode();

	///////////////////////////////////////
	// Start to load firmware
	///////////////////////////////////////
	drvISP_EntryIspMode();
	//drvISP_BlockErase(0x00000);//////zzf
	//DBG("FwVersion=2");

	for (i = 0; i < 94; i++)   // total  94 KB : 1 byte per R/W
	{
		printk("i = %d ,auto_updata = %d\n",i,auto_updata);
		if(auto_updata){
			for ( j = 0; j < 1024; j++ )        //Read 1k bytes
			{
				Fmr_Loader[j] = pfirmware[(i*1024)+j]; // Read the bin files of slave firmware from the baseband file system
			}
			drvISP_Program(i,Fmr_Loader);
		}
		else
			drvISP_Program(i,&download_firmware_buf[i*1024]);
		mdelay(1);
	}
#endif
	drvISP_ExitIspMode();
	FwDataCnt = 0;
	if (download_firmware_buf != NULL)
	{
		kfree(download_firmware_buf);
		download_firmware_buf = NULL;
	}

	i2c_client->addr = FW_ADDR_MSG21XX_TP;
	MSG2133_En_Pin_Out(0);
	msleep(100);
	MSG2133_En_Pin_Out(1);
	msleep(500);
	update_switch = 0;
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_HIGH, tpd_eint_interrupt_handler,1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	return 0;
}

static ssize_t firmware_update_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_update_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	auto_updata = 0;
	masterBUT_LoadFwToTarget(NULL,0);
	return size;
}

static DEVICE_ATTR(update, 0644, firmware_update_show, firmware_update_store);

static ssize_t firmware_version_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[4] ;
	unsigned short major=0, minor=0;
	fw_version = kzalloc(sizeof(char), GFP_KERNEL);

	//Get_Chip_Version();
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x74;
	HalTscrCDevWriteI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_tx_data[0], 3);
	HalTscrCReadI2CSeq(FW_ADDR_MSG21XX_TP, &dbbus_rx_data[0], 4);
	major = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
	minor = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];
	DBG("***major = %d ***\n", major);
	DBG("***minor = %d ***\n", minor);
	sprintf(fw_version,"%03d%03d", major, minor);
	DBG("***fw_version = %s ***\n", fw_version);

	return size;

}
static DEVICE_ATTR(version, 0644, firmware_version_show, firmware_version_store);


static ssize_t firmware_data_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	DBG("***FwDataCnt = %d ***\n", FwDataCnt);
	int i;
	if (download_firmware_buf == NULL) {
		download_firmware_buf = kzalloc(DOWNLOAD_FIRMWARE_BUF_SIZE, GFP_KERNEL);
		if (download_firmware_buf == NULL)
			return NULL;
	}
	if(FwDataCnt<94)
	{
		memcpy(&download_firmware_buf[FwDataCnt*1024], buf, 1024);
	}
	FwDataCnt++;
	return size;
}
static DEVICE_ATTR(data, 0644, firmware_data_show, firmware_data_store);
#endif

static ssize_t msg2133_show_updata(struct device *dev,struct device_attribute *attr, char *buf)
{
	printk("cat updata\n");
	return 0;
}
static ssize_t msg2133_store_updata(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;
	char strbuf[30];
	auto_updata = 1;
	hwPowerDown(MT65XX_POWER_LDO_VGP2,"MSG_TP");
	msleep(100);
	hwPowerOn(MT65XX_POWER_LDO_VGP2,VOL_2800,"MSG_TP");
	sscanf(buf,"%d", &mode);
		printk("mode = %d\n",mode);
	if(!mode){
		printk("MSG_FIRMWARE_MEGANE_YJV04\n");
		masterBUT_LoadFwToTarget(MSG_FIRMWARE_MEGANE_YJV06,sizeof(MSG_FIRMWARE_MEGANE_YJV06));
		}
	else if(mode == 1){
		printk("MSG_FIRMWARE_MEGANE_V204\n");
		masterBUT_LoadFwToTarget(MSG_FIRMWARE_MEGANE_V204,sizeof(MSG_FIRMWARE_MEGANE_V204));
		}
	else
		printk("no this mode\n");
	return count;
}

static ssize_t msg2133_show_version(struct device *dev,struct device_attribute *attr, char *buf)
{
	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[4];
	unsigned short current_major_version=0, current_minor_version=0;

	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x74;
	i2c_master_send(i2c_client, &dbbus_tx_data[0], 3);
	i2c_master_recv(i2c_client, &dbbus_rx_data[0], 4);

	current_major_version = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
	current_minor_version = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];

	return sprintf(buf,"current_major_version = 0x%04x\ncurrent_minor_version = 0x%04x\n",current_major_version,current_minor_version);
}

static DRIVER_ATTR(updata,    S_IWUSR | S_IRUGO,   msg2133_show_updata,  msg2133_store_updata);
static DRIVER_ATTR(version,    S_IWUSR | S_IRUGO,   msg2133_show_version, NULL);

 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	u8 test[0] = {0x45};
	i2c_client = client;
	int error;

	unsigned char dbbus_tx_data[3];
	unsigned char dbbus_rx_data[4] ;
	unsigned short current_major_version=0, current_minor_version=0;
	unsigned short wanted_major_version=0,wanted_minor_version=0;
	char version[2][10] = {"YEJI", "JUNDA"};
	char *pversion;
	printk("xxxxxxxxxxxxxxxxx%sxxxxxxxxxxxxxxxxxx\n", __FUNCTION__);
	i2c_client = client;
	i2c_client->addr |= I2C_ENEXT_FLAG; //I2C_HS_FLAG;
	i2c_client->timing = 100;

	mdelay(10);
	hwPowerOn(MT65XX_POWER_LDO_VGP2,VOL_2800,"MSG_TP");
	mdelay(10);

	MSG2133_En_Pin_Out(1);
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1); 
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	mdelay(300);

/**************enter update****************/
	//get version
	auto_updata = 1;
	dbbus_tx_data[0] = 0x53;
	dbbus_tx_data[1] = 0x00;
	dbbus_tx_data[2] = 0x74;
	error = i2c_master_send(i2c_client, &dbbus_tx_data[0], 3);
	if(error < 0)						//add error handing about tp not exsit which is used for meta test
	{
		i2c_client->addr = FW_ADDR_MSG21XX;
		error = i2c_master_send(i2c_client, &test[0], 1);
		if(error < 0){
			printk("the device of tp do not exist.\n");
			return -1;
		}
		else{
			masterBUT_LoadFwToTarget(MSG_FIRMWARE_MEGANE_V204,sizeof(MSG_FIRMWARE_MEGANE_V204));
			dbbus_tx_data[0] = 0x53;
			dbbus_tx_data[1] = 0x00;
			dbbus_tx_data[2] = 0x74;
			i2c_master_send(i2c_client, &dbbus_tx_data[0], 3);
		}
	}
	i2c_master_recv(i2c_client, &dbbus_rx_data[0], 4);


	current_major_version = (dbbus_rx_data[1]<<8)+dbbus_rx_data[0];
	current_minor_version = (dbbus_rx_data[3]<<8)+dbbus_rx_data[2];

	printk("current_major_version = 0x%04x\ncurrent_minor_version = 0x%04x\n",current_major_version,current_minor_version);

	/*JUNDA*/
	if(current_major_version == 0x0002)
	{
		printk("the VID is %s\n",version[1]);
		if(current_minor_version != 0x0004)
			masterBUT_LoadFwToTarget(MSG_FIRMWARE_MEGANE_V204,sizeof(MSG_FIRMWARE_MEGANE_V204));
	}
	/*YEJI*/
	else if(current_major_version == 0x0314)
	{
		printk("the VID is %s\n",version[0]);
		if(current_minor_version !=0x0006)
			masterBUT_LoadFwToTarget(MSG_FIRMWARE_MEGANE_YJV06,sizeof(MSG_FIRMWARE_MEGANE_YJV06));
		}
	else
		printk("the VID is unknown\n");

/****************end update****************/

	tpd_load_status = 1;

	thread_dual = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread_dual))
	{ 
		retval = PTR_ERR(thread_dual);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread_dual: %d\n", retval);
	}

#ifdef TP_FIRMWARE_UPDATE
	firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");

	if(IS_ERR(firmware_class))
	{
		pr_err("Failed to create class(firmware)!\n");
	}

	firmware_cmd_dev = device_create(firmware_class,NULL, 0, NULL, "device");

	if(IS_ERR(firmware_cmd_dev))
	{
		pr_err("Failed to create device(firmware_cmd_dev)!\n");
	}

	// version
	if(device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
	}

	// update
	if(device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
	}

	// data
	if(device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);
	}

	dev_set_drvdata(firmware_cmd_dev, NULL);
#endif

	error = device_create_file(&client->dev, &driver_attr_updata);
	error = device_create_file(&client->dev, &driver_attr_version);

	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	return 0;
   
 }

 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
   
	 TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
 
 
 static int tpd_local_init(void)
 {


  TPD_DMESG("Focaltech msg2133 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);

 
   if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("unable to add i2c driver.\n");
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

 static int tpd_resume(struct i2c_client *client)
 {
	printk("TPD wake up\n");
	hwPowerOn(MT65XX_POWER_LDO_VGP2,VOL_2800,"MSG_TP");
#ifdef TP_FIRMWARE_UPDATE
	if(update_switch==1)
	{
		return 0;
	}
#endif
	MSG2133_En_Pin_Out(1);
	msleep(300);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	return 0;
 }
 
 static int tpd_suspend(struct i2c_client *client, pm_message_t message)
 {
#ifdef TP_FIRMWARE_UPDATE
	if(update_switch==1)
	{
		return 0;
	}
#endif
	
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	MSG2133_En_Pin_Out(0);
	mdelay(20);

	hwPowerDown(MT65XX_POWER_LDO_VGP2,"MSG_TP");
	 return 0;
 } 


 static struct tpd_driver_t tpd_device_driver = {
		 .tpd_device_name = "msg2133",
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
	 printk("MediaTek msg2133 touch panel driver init\n");
	   i2c_register_board_info(0, &i2c_tpd, 1);
		 if(tpd_driver_add(&tpd_device_driver) < 0)
			 TPD_DMESG("add msg2133 driver failed\n");
	 return 0;
 }
 
 /* should never be called */
 static void __exit tpd_driver_exit(void) {
	 TPD_DMESG("MediaTek msg2133 touch panel driver exit\n");
	 //input_unregister_device(tpd->dev);
	 tpd_driver_remove(&tpd_device_driver);
 }
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);


