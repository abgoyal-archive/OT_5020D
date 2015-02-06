/*
 * LC898215 voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "OV8850AF.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 1
#define OV8850AF_DRVNAME "OV8850AF"

static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO(OV8850AF_DRVNAME, 0xe4>>1)};

#define OV8850AF_DEBUG
#ifdef OV8850AF_DEBUG
#define OV8850AFDB printk
#else
#define OV8850AFDB(x,...)
#endif

static spinlock_t g_OV8850AF_SpinLock;

static struct i2c_client * g_pstOV8850AF_I2Cclient = NULL;

static dev_t g_OV8850AF_devno;
static struct cdev * g_pOV8850AF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4OV8850AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4OV8850AF_INF = 0;
static unsigned long g_u4OV8850AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode);
extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut);
extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir);

#define GPIO_CAM_AF_EN 214

#define OV8850AF_VCM_WRITE_ID           0xE4

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);


int s4OV8850AF_read_byte(u8 addr, u8 *data)
{
#if 1
	u8 buf;
	int ret = 0;

	buf = addr;
	
	ret = i2c_master_send(g_pstOV8850AF_I2Cclient, (const char*)&buf, 1);
	if (ret < 0) {
		OV8850AFDB("s4OV8850AF_read_byte send command error!!  ret=%d\n",ret);
		return -EFAULT;
	}
	ret = i2c_master_recv(g_pstOV8850AF_I2Cclient, (char*)&buf, 1);
	if (ret < 0) {
		OV8850AFDB("s4OV8850AF_read_byte reads data error!!  ret=%d\n",ret);
		return -EFAULT;
	}

	*data = buf;
	return 0;
	#else
	int  i4RetValue = 0;
	i4RetValue = iReadReg(addr,data,OV8850AF_VCM_WRITE_ID);
	if (i4RetValue < 0) 	{
		OV8850AFDB("s4OV8850AF_read_byte failed--Mbyte!! \n");
		return -1;
	}

	return 0;
	#endif
}

int s4OV8850AF_write_byte( u8 addr, u8 data)
{
#if 1
	u8 buf[] = {addr, data};
	int ret = 0;

	ret = i2c_master_send(g_pstOV8850AF_I2Cclient, (const char*)buf, sizeof(buf));
	if (ret < 0) {
	    OV8850AFDB("s4OV8850AF_write_byte send data error!!   ret=%d\n",ret);
	    return -EFAULT;
	}

	return 0;
#else
	int  i4RetValue = 0;
	i4RetValue = iWriteReg(addr,(u32)data,1,OV8850AF_VCM_WRITE_ID);
	if (i4RetValue < 0) {
	    OV8850AFDB("s4OV8850AF_write_byte  send data error!!   i4RetValue=%d\n",i4RetValue);
	    return -EFAULT;
	}
	return 0;
#endif
}

static int s4OV8850AF_ReadReg(unsigned short * a_pu2Result)
{
	int  i4RetValue = 0;
	char pBuff[2];
	
	i4RetValue = s4OV8850AF_read_byte(0x04, &pBuff[0]);
	 if (i4RetValue < 0) 	{
		OV8850AFDB("s4OV8850AF_ReadReg 0x04 failed!! \n");
		return -1;
	}
	i4RetValue = s4OV8850AF_read_byte(0x05, &pBuff[1]);
	if (i4RetValue < 0) 	{
		OV8850AFDB("s4OV8850AF_ReadReg 0x04 failed!! \n");
		return -1;
	}

	*a_pu2Result =((u16)(pBuff[0]&0x03))<<8 + (u16)(pBuff[1]);

	return 0;
}

static int s4OV8850AF_WriteReg(u16 a_u2Data) 
{
	int  i4RetValue = 0;
	
	i4RetValue = s4OV8850AF_write_byte(0x4, ((u8)(a_u2Data>>8)&0x03));
	if (i4RetValue < 0) 	{
		OV8850AFDB("s4OV8850AF_WriteReg 0x04 failed!! \n");
		return -1;
	}
	i4RetValue = s4OV8850AF_write_byte(0x5, ((u8)a_u2Data));
	if (i4RetValue < 0) 	{
		OV8850AFDB("s4OV8850AF_WriteReg 0x05 failed!! \n");
		return -1;
	}

	return 0;
}

inline static int getOV8850AFInfo(__user stOV8850AF_MotorInfo * pstMotorInfo)
{
    stOV8850AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4OV8850AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4OV8850AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4OV8850AF_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stOV8850AF_MotorInfo)))
    {
        OV8850AFDB("[OV8850AF] copy to user failed when getting motor information \n");
    }

    return 0;
}

inline static int moveOV8850AF(unsigned long a_u4Position)
{
    int ret = 0;
    
    if((a_u4Position > g_u4OV8850AF_MACRO) || (a_u4Position < g_u4OV8850AF_INF))
    {
        OV8850AFDB("[OV8850AF] out of range \n");
        return -EINVAL;
    }

    if (g_s4OV8850AF_Opened == 1)
    {
        unsigned short InitPos;
        ret = s4OV8850AF_ReadReg(&InitPos);
	    
        spin_lock(&g_OV8850AF_SpinLock);
        if(ret == 0)
        {
            OV8850AFDB("[OV8850AF] Init Pos %6d \n", InitPos);
            g_u4CurrPosition = (unsigned long)InitPos;
        }
        else
        {		
            g_u4CurrPosition = 0;
        }
        g_s4OV8850AF_Opened = 2;
        spin_unlock(&g_OV8850AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_OV8850AF_SpinLock);	
        g_i4Dir = 1;
        spin_unlock(&g_OV8850AF_SpinLock);	
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_OV8850AF_SpinLock);	
        g_i4Dir = -1;
        spin_unlock(&g_OV8850AF_SpinLock);			
    }
    else
	{return 0;}

    spin_lock(&g_OV8850AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_OV8850AF_SpinLock);	

    //OV8850AFDB("[OV8850AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_OV8850AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_OV8850AF_SpinLock);	
		
            if(s4OV8850AF_WriteReg((unsigned short)g_u4TargetPosition) == 0)
            {
                spin_lock(&g_OV8850AF_SpinLock);		
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_OV8850AF_SpinLock);				
            }
            else
            {
                OV8850AFDB("[OV8850AF] set I2C failed when moving the motor \n");			
                spin_lock(&g_OV8850AF_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_OV8850AF_SpinLock);				
            }

    return 0;
}

inline static int setOV8850AFInf(unsigned long a_u4Position)
{
    spin_lock(&g_OV8850AF_SpinLock);
    g_u4OV8850AF_INF = a_u4Position;
    spin_unlock(&g_OV8850AF_SpinLock);	
    return 0;
}

inline static int setOV8850AFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_OV8850AF_SpinLock);
    g_u4OV8850AF_MACRO = a_u4Position;
    spin_unlock(&g_OV8850AF_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long OV8850AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    OV8850AFDB("OV8850AF_Ioctl, a_u4Command=0x%x,a_u4Param=%d\n",a_u4Command,a_u4Param);

    switch(a_u4Command)
    {
        case OV8850AFIOC_G_MOTORINFO :
            i4RetValue = getOV8850AFInfo((__user stOV8850AF_MotorInfo *)(a_u4Param));
        break;

        case OV8850AFIOC_T_MOVETO :
            i4RetValue = moveOV8850AF(a_u4Param);
        break;
 
        case OV8850AFIOC_T_SETINFPOS :
            i4RetValue = setOV8850AFInf(a_u4Param);
        break;

        case OV8850AFIOC_T_SETMACROPOS :
            i4RetValue = setOV8850AFMacro(a_u4Param);
        break;
		
        default :
      	    OV8850AFDB("[OV8850AF] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    return i4RetValue;
}

//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
// 3.Update f_op pointer.
// 4.Fill data structures into private_data
//CAM_RESET
static int OV8850AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
	OV8850AFDB("OV8850AF_Open\n");
	spin_lock(&g_OV8850AF_SpinLock);

	if(g_s4OV8850AF_Opened)
	{
		spin_unlock(&g_OV8850AF_SpinLock);
		OV8850AFDB("[OV8850AF] the device is opened \n");
		return -EBUSY;
	}

	g_s4OV8850AF_Opened = 1;

	spin_unlock(&g_OV8850AF_SpinLock);

	mt_set_gpio_mode(GPIO_CAM_AF_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAM_AF_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAM_AF_EN, GPIO_OUT_ONE);
	mdelay(10);
	s4OV8850AF_write_byte(0x01,0x01); //softreset 
	s4OV8850AF_write_byte(0x01,0x00); 
	s4OV8850AF_write_byte(0x03,0x7A); 
	mdelay(100);
	s4OV8850AF_write_byte(0x06,0x03);
	s4OV8850AF_write_byte(0x07,0xff);
	// decrease AF pwm disturb camera 
	s4OV8850AF_write_byte(0x20,0x34);
	s4OV8850AF_write_byte(0x31,0xD0);
	s4OV8850AF_write_byte(0x33,0xF1);
	s4OV8850AF_write_byte(0x34,0x00);
	return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int OV8850AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
	OV8850AFDB("OV8850AF_Release\n");
    if (g_s4OV8850AF_Opened)
    {
        OV8850AFDB("[OV8850AF] feee \n");
        g_sr = 5;
	    s4OV8850AF_WriteReg(200);
        msleep(10);
	    s4OV8850AF_WriteReg(100);
        msleep(10);
            	            	    	    
        spin_lock(&g_OV8850AF_SpinLock);
        g_s4OV8850AF_Opened = 0;
        spin_unlock(&g_OV8850AF_SpinLock);
        
        mt_set_gpio_mode(GPIO_CAM_AF_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_CAM_AF_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAM_AF_EN, GPIO_OUT_ZERO);
    }

    return 0;
}

static const struct file_operations g_stOV8850AF_fops = 
{
    .owner = THIS_MODULE,
    .open = OV8850AF_Open,
    .release = OV8850AF_Release,
    .unlocked_ioctl = OV8850AF_Ioctl
};

inline static int Register_OV8850AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_OV8850AF_devno, 0, 1,OV8850AF_DRVNAME) )
    {
        OV8850AFDB("[OV8850AF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pOV8850AF_CharDrv = cdev_alloc();

    if(NULL == g_pOV8850AF_CharDrv)
    {
        unregister_chrdev_region(g_OV8850AF_devno, 1);

        OV8850AFDB("[OV8850AF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pOV8850AF_CharDrv, &g_stOV8850AF_fops);

    g_pOV8850AF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pOV8850AF_CharDrv, g_OV8850AF_devno, 1))
    {
        OV8850AFDB("[OV8850AF] Attatch file operation failed\n");

        unregister_chrdev_region(g_OV8850AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        OV8850AFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_OV8850AF_devno, NULL, OV8850AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    return 0;
}

inline static void Unregister_OV8850AF_CharDrv(void)
{
    //Release char driver
    cdev_del(g_pOV8850AF_CharDrv);

    unregister_chrdev_region(g_OV8850AF_devno, 1);
    
    device_destroy(actuator_class, g_OV8850AF_devno);

    class_destroy(actuator_class);
}

//////////////////////////////////////////////////////////////////////

static int OV8850AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int OV8850AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id OV8850AF_i2c_id[] = {{OV8850AF_DRVNAME,0},{}};   
struct i2c_driver OV8850AF_i2c_driver = {                       
    .probe = OV8850AF_i2c_probe,                                   
    .remove = OV8850AF_i2c_remove,                           
    .driver.name = OV8850AF_DRVNAME,                 
    .id_table = OV8850AF_i2c_id,                             
};  

#if 0 
static int OV8850AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, OV8850AF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int OV8850AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int OV8850AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	OV8850AFDB("[OV8850AF] Attach I2C \n");

	OV8850AFDB("OV8850AF_i2c_probe client->timing=%x,addr=%x \n",client->timing,client->addr);

	g_pstOV8850AF_I2Cclient = client;	

	//Register char driver
	i4RetValue = Register_OV8850AF_CharDrv();
	if(i4RetValue){
		OV8850AFDB("[OV8850AF] register char device failed!\n");
		return i4RetValue;
	}

	spin_lock_init(&g_OV8850AF_SpinLock);

	OV8850AFDB("[OV8850AF] Attached!! \n");

	return 0;
}

static int OV8850AF_probe(struct platform_device *pdev)
{
	OV8850AFDB("OV8850AF_probe \n");
	return i2c_add_driver(&OV8850AF_i2c_driver);
}

static int OV8850AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&OV8850AF_i2c_driver);
	return 0;
}

static int OV8850AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int OV8850AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stOV8850AF_Driver = {
    .probe		= OV8850AF_probe,
    .remove	= OV8850AF_remove,
    .suspend	= OV8850AF_suspend,
    .resume	= OV8850AF_resume,
    .driver		= {
        .name	= "lens_actuator",
        .owner	= THIS_MODULE,
    }
};

static int __init OV8850AF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);

    OV8850AFDB("OV8850AF_i2C_init \n");
	
    if(platform_driver_register(&g_stOV8850AF_Driver)){
        OV8850AFDB("failed to register OV8850AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit OV8850AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stOV8850AF_Driver);
}

module_init(OV8850AF_i2C_init);
module_exit(OV8850AF_i2C_exit);

MODULE_DESCRIPTION("OV8850AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


