/*******************************************************************************************/


/*******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov8850mipiraw_Sensor.h"
#include "ov8850mipiraw_Camera_Sensor_para.h"
#include "ov8850mipiraw_CameraCustomized.h"

/*********For OTP feature start**********/
#define OTP_DATA_ADDR         0x3D00
#define OTP_LOAD_ADDR         0x3D81
#define OTP_BANK_ADDR         0x3D84

#define LENC_START_ADDR       0x5800
#define LENC_REG_SIZE         62

#define OTP_LENC_GROUP_ADDR   0x3D00

#define OTP_WB_GROUP_ADDR     0x3D00
#define OTP_WB_GROUP_SIZE     16

#define GAIN_RH_ADDR          0x3400
#define GAIN_RL_ADDR          0x3401
#define GAIN_GH_ADDR          0x3402
#define GAIN_GL_ADDR          0x3403
#define GAIN_BH_ADDR          0x3404
#define GAIN_BL_ADDR          0x3405

#define GAIN_DEFAULT_VALUE    0x0400 // 1x gain

#define OTP_MID               0x02
/*********For OTP feature end**********/

static DEFINE_SPINLOCK(ov8850mipiraw_drv_lock);

#define OV8850_DEBUG
#define OV8850_DEBUG_DETAIL

#ifdef OV8850_DEBUG
	#define OV8850DB(fmt, arg...) printk( "[OV8850Raw] "  fmt, ##arg)
#else
	#define OV8850DB(x,...)
#endif

#ifdef OV8850_DEBUG_DETAIL
	#define OV8850DBDETAIL(fmt, arg...) printk( "[OV8850Raw] "  fmt, ##arg)
#else
	#define OV8850DBDETAIL(x,...)
#endif

#define mDELAY(ms)  mdelay(ms)

MSDK_SENSOR_CONFIG_STRUCT OV8850SensorConfigData;

kal_uint32 OV8850_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM OV8850CurrentScenarioId = ACDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT OV8850SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT OV8850SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static OV8850_PARA_STRUCT ov8850;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define OV8850_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, OV8850MIPI_WRITE_ID)

kal_uint16 OV8850_read_cmos_sensor(kal_uint32 addr)
{
kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,OV8850MIPI_WRITE_ID);
    return get_byte;
}

/****************For OTP feature start*************************/
// R/G and B/G of current camera module
unsigned char rg_ratio = 0;
unsigned char bg_ratio = 0;

unsigned char otp_lenc_data[62];


// Enable OTP read function
void otp_read_enable(void)
{
	OV8850_write_cmos_sensor(OTP_LOAD_ADDR, 0x01);
	mdelay(15); // sleep > 10ms
}

// Disable OTP read function
void otp_read_disable(void)
{
	OV8850_write_cmos_sensor(OTP_LOAD_ADDR, 0x00);
	mdelay(15); // sleep > 10ms
}

void otp_read(unsigned short otp_addr, unsigned char* otp_data)
{
	otp_read_enable();
	*otp_data = OV8850_read_cmos_sensor(otp_addr);
	otp_read_disable();
}

/*******************************************************************************
* Function    :  otp_clear
* Description :  Clear OTP buffer 
* Parameters  :  none
* Return      :  none
*******************************************************************************/	
void otp_clear(void)
{
	// After read/write operation, the OTP buffer should be cleared to avoid accident write
	unsigned char i;
	for (i=0; i<16; i++) 
	{
		OV8850_write_cmos_sensor(OTP_DATA_ADDR+i, 0x00);
	}
}

/*******************************************************************************
* Function    :  otp_check_wb_group
* Description :  Check OTP Space Availability
* Parameters  :  [in] index : index of otp group (0, 1, 2)
* Return      :  0, group index is empty
                 1, group index has invalid data
                 2, group index has valid data
                -1, group index error
*******************************************************************************/	
signed char otp_check_wb_group(unsigned char index)
{   
	unsigned short otp_addr = OTP_WB_GROUP_ADDR;
	unsigned char  flag;

    if (index > 2)
	{
	//	TRACE("OTP input wb group index %d error\n", index);
		return -1;
	}
		
	// select bank 1-3
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, 0xc0 | (index+1));

	otp_read(otp_addr, &flag);
	otp_clear();

	// Check all bytes of a group. If all bytes are '0', then the group is empty. 
	// Check from group 1 to group 2, then group 3.
	
	flag &= 0xc0;
	if (!flag)
	{
	//	TRACE("wb group %d is empty\n", index);
		return 0;
	}
	else if (flag == 0x40)
	{
	//	TRACE("wb group %d has valid data\n", index);
		return 2;
	}
	else
	{
	//	TRACE("wb group %d has invalid data\n", index);
		return 1;
	}
}

/*******************************************************************************
* Function    :  otp_read_wb_group
* Description :  Read group value and store it in OTP Struct 
* Parameters  :  [in] index : index of otp group (0, 1, 2)
* Return      :  group index (0, 1, 2)
                 -1, error
*******************************************************************************/	
signed char otp_read_wb_group(signed char index)
{
	unsigned short otp_addr;
	unsigned char  mid;

	if (index == -1)
	{
		// Check first OTP with valid data
		for (index=0; index<3; index++)
		{
			if (otp_check_wb_group(index) == 2)
			{
			//	TRACE("read wb from group %d\n", index);
				break;
			}
		}

		if (index > 2)
		{
		//	TRACE("no group has valid data\n");
			return -1;
		}
	}
	else
	{
		if (otp_check_wb_group(index) != 2)
		{
		//	TRACE("read wb from group %d failed\n", index);
			return -1;
		}
	}

	otp_addr = OTP_WB_GROUP_ADDR;

	// select bank 1-3
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, 0xc0 | (index+1));

	otp_read(otp_addr+1, &mid);
	if (mid != OTP_MID)
	{
		return -1;
	}

	otp_read(otp_addr+6, &rg_ratio);
	otp_read(otp_addr+7, &bg_ratio);
	otp_clear();

//	TRACE("read wb finished\n");
	return index;
}

#ifdef SUPPORT_FLOATING //Use this if support floating point values
/*******************************************************************************
* Function    :  otp_apply_wb
* Description :  Calcualte and apply R, G, B gain to module
* Parameters  :  [in] golden_rg : R/G of golden camera module
                 [in] golden_bg : B/G of golden camera module
* Return      :  1, success; 0, fail
*******************************************************************************/	
bool otp_apply_wb(unsigned char golden_rg, unsigned char golden_bg)
{
	unsigned short gain_r = GAIN_DEFAULT_VALUE;
	unsigned short gain_g = GAIN_DEFAULT_VALUE;
	unsigned short gain_b = GAIN_DEFAULT_VALUE;

	double ratio_r, ratio_g, ratio_b;
	double cmp_rg, cmp_bg;

	if (!golden_rg || !golden_bg)
	{
	//	TRACE("golden_rg / golden_bg can not be zero\n");
		return 0;
	}

	// Calcualte R, G, B gain of current module from R/G, B/G of golden module
        // and R/G, B/G of current module
	cmp_rg = 1.0 * rg_ratio / golden_rg;
	cmp_bg = 1.0 * bg_ratio / golden_bg;

	if ((cmp_rg<1) && (cmp_bg<1))
	{
		// R/G < R/G golden, B/G < B/G golden
		ratio_g = 1;
		ratio_r = 1 / cmp_rg;
		ratio_b = 1 / cmp_bg;
	}
	else if (cmp_rg > cmp_bg)
	{
		// R/G >= R/G golden, B/G < B/G golden
		// R/G >= R/G golden, B/G >= B/G golden
		ratio_r = 1;
		ratio_g = cmp_rg;
		ratio_b = cmp_rg / cmp_bg;
	}
	else
	{
		// B/G >= B/G golden, R/G < R/G golden
		// B/G >= B/G golden, R/G >= R/G golden
		ratio_b = 1;
		ratio_g = cmp_bg;
		ratio_r = cmp_bg / cmp_rg;
	}

	// write sensor wb gain to registers
	// 0x0400 = 1x gain
	if (ratio_r != 1)
	{
		gain_r = (unsigned short)(GAIN_DEFAULT_VALUE * ratio_r);
		OV8850_write_cmos_sensor(GAIN_RH_ADDR, gain_r >> 8);
		OV8850_write_cmos_sensor(GAIN_RL_ADDR, gain_r & 0x00ff);
	}

	if (ratio_g != 1)
	{
		gain_g = (unsigned short)(GAIN_DEFAULT_VALUE * ratio_g);
		OV8850_write_cmos_sensor(GAIN_GH_ADDR, gain_g >> 8);
		OV8850_write_cmos_sensor(GAIN_GL_ADDR, gain_g & 0x00ff);
	}

	if (ratio_b != 1)
	{
		gain_b = (unsigned short)(GAIN_DEFAULT_VALUE * ratio_b);
		OV8850_write_cmos_sensor(GAIN_BH_ADDR, gain_b >> 8);
		OV8850_write_cmos_sensor(GAIN_BL_ADDR, gain_b & 0x00ff);
	}

//	TRACE("cmp_rg=%f, cmp_bg=%f\n", cmp_rg, cmp_bg);
//	TRACE("ratio_r=%f, ratio_g=%f, ratio_b=%f\n", ratio_r, ratio_g, ratio_b);
//	TRACE("gain_r=0x%x, gain_g=0x%x, gain_b=0x%x\n", gain_r, gain_g, gain_b);
	return 1;
}

#else //Use this if not support floating point values

#define OTP_MULTIPLE_FAC	10000
bool otp_apply_wb(unsigned char golden_rg, unsigned char golden_bg)
{
	unsigned short gain_r = GAIN_DEFAULT_VALUE;
	unsigned short gain_g = GAIN_DEFAULT_VALUE;
	unsigned short gain_b = GAIN_DEFAULT_VALUE;

	unsigned short ratio_r, ratio_g, ratio_b;
	unsigned short cmp_rg, cmp_bg;

	if (!golden_rg || !golden_bg)
	{
	//	TRACE("golden_rg / golden_bg can not be zero\n");
		return 0;
	}

	// Calcualte R, G, B gain of current module from R/G, B/G of golden module
    // and R/G, B/G of current module
	cmp_rg = OTP_MULTIPLE_FAC * rg_ratio / golden_rg;
	cmp_bg = OTP_MULTIPLE_FAC * bg_ratio / golden_bg;

	if ((cmp_rg < 1 * OTP_MULTIPLE_FAC) && (cmp_bg < 1 * OTP_MULTIPLE_FAC))
	{
		// R/G < R/G golden, B/G < B/G golden
		ratio_g = 1 * OTP_MULTIPLE_FAC;
		ratio_r = 1 * OTP_MULTIPLE_FAC * OTP_MULTIPLE_FAC / cmp_rg;
		ratio_b = 1 * OTP_MULTIPLE_FAC * OTP_MULTIPLE_FAC / cmp_bg;
	}
	else if (cmp_rg > cmp_bg)
	{
		// R/G >= R/G golden, B/G < B/G golden
		// R/G >= R/G golden, B/G >= B/G golden
		ratio_r = 1 * OTP_MULTIPLE_FAC;
		ratio_g = cmp_rg;
		ratio_b = OTP_MULTIPLE_FAC * cmp_rg / cmp_bg;
	}
	else
	{
		// B/G >= B/G golden, R/G < R/G golden
		// B/G >= B/G golden, R/G >= R/G golden
		ratio_b = 1 * OTP_MULTIPLE_FAC;
		ratio_g = cmp_bg;
		ratio_r = OTP_MULTIPLE_FAC * cmp_bg / cmp_rg;
	}

	// write sensor wb gain to registers
	// 0x0400 = 1x gain
	if (ratio_r != 1 * OTP_MULTIPLE_FAC)
	{
		gain_r = GAIN_DEFAULT_VALUE * ratio_r / OTP_MULTIPLE_FAC;
		OV8850_write_cmos_sensor(GAIN_RH_ADDR, gain_r >> 8);
		OV8850_write_cmos_sensor(GAIN_RL_ADDR, gain_r & 0x00ff);
	}

	if (ratio_g != 1 * OTP_MULTIPLE_FAC)
	{
		gain_g = GAIN_DEFAULT_VALUE * ratio_g / OTP_MULTIPLE_FAC;
		OV8850_write_cmos_sensor(GAIN_GH_ADDR, gain_g >> 8);
		OV8850_write_cmos_sensor(GAIN_GL_ADDR, gain_g & 0x00ff);
	}

	if (ratio_b != 1 * OTP_MULTIPLE_FAC)
	{
		gain_b = GAIN_DEFAULT_VALUE * ratio_b / OTP_MULTIPLE_FAC;
		OV8850_write_cmos_sensor(GAIN_BH_ADDR, gain_b >> 8);
		OV8850_write_cmos_sensor(GAIN_BL_ADDR, gain_b & 0x00ff);
	}

//	TRACE("cmp_rg=%d, cmp_bg=%d\n", cmp_rg, cmp_bg);
//	TRACE("ratio_r=%d, ratio_g=%d, ratio_b=%d\n", ratio_r, ratio_g, ratio_b);
//	TRACE("gain_r=0x%x, gain_g=0x%x, gain_b=0x%x\n", gain_r, gain_g, gain_b);
	return 1;
}
#endif /* SUPPORT_FLOATING */

/*******************************************************************************
* Function    :  otp_update_wb
* Description :  Update white balance settings from OTP
* Parameters  :  [in] golden_rg : R/G of golden camera module
                 [in] golden_bg : B/G of golden camera module
* Return      :  1, success; 0, fail
*******************************************************************************/	
bool otp_update_wb(unsigned char golden_rg, unsigned char golden_bg) 
{
//	TRACE("start wb update\n");

	if (otp_read_wb_group(-1) != -1)
	{
		if (otp_apply_wb(golden_rg, golden_bg) == 1)
		{
		//	TRACE("wb update finished\n");
			return 1;
		}
	}

//	TRACE("wb update failed\n");
	return 0;
}

/*******************************************************************************
* Function    :  otp_check_lenc_group
* Description :  Check OTP Space Availability
* Parameters  :  [in] BYTE index : index of otp group (0, 1, 2)
* Return      :  0, group index is empty
                 1, group index has invalid data
                 2, group index has valid data
                -1, group index error
*******************************************************************************/	
signed char otp_check_lenc_group(BYTE index)
{   
	unsigned short otp_addr = OTP_LENC_GROUP_ADDR;
	unsigned char  flag;
	unsigned char  bank;

    if (index > 2)
	{
	//	TRACE("OTP input lenc group index %d error\n", index);
		return -1;
	}
		
	// select bank: index*4 + 4
	bank = 0xc0 | (index*4 + 4);
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, bank);

	otp_read(otp_addr, &flag);
	otp_clear();

	flag &= 0xc0;

	// Check all bytes of a group. If all bytes are '0', then the group is empty. 
	// Check from group 1 to group 2, then group 3.
	if (!flag)
	{
	//	TRACE("lenc group %d is empty\n", index);
		return 0;
	}
	else if (flag == 0x40)
	{
	//	TRACE("lenc group %d has valid data\n", index);
		return 2;
	}
	else
	{
	//	TRACE("lenc group %d has invalid data\n", index);
		return 1;
	}
}

/*******************************************************************************
* Function    :  otp_read_lenc_group
* Description :  Read group value and store it in OTP Struct 
* Parameters  :  [in] int index : index of otp group (0, 1, 2)
* Return      :  group index (0, 1, 2)
                 -1, error
*******************************************************************************/	
signed char otp_read_lenc_group(int index)
{
	unsigned short otp_addr;
	unsigned char  bank;
	unsigned char  i;

	if (index == -1)
	{
		// Check first OTP with valid data
		for (index=0; index<3; index++)
		{
			if (otp_check_lenc_group(index) == 2)
			{
			//	TRACE("read lenc from group %d\n", index);
				break;
			}
		}

		if (index > 2)
		{
		//	TRACE("no group has valid data\n");
			return -1;
		}
	}
	else
	{
		if (otp_check_lenc_group(index) != 2) 
		{
		//	TRACE("read lenc from group %d failed\n", index);
			return -1;
		}
	}

	// select bank: index*4 + 4
	bank = 0xc0 | (index*4 + 4);
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, bank);

	otp_addr = OTP_LENC_GROUP_ADDR+1;

	otp_read_enable();
	for (i=0; i<15; i++) 
	{
		otp_lenc_data[i] = OV8850_read_cmos_sensor(otp_addr);
		otp_addr++;
	}
	otp_read_disable();
	otp_clear();

	// select next bank
	bank++;
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, bank);

	otp_addr = OTP_LENC_GROUP_ADDR;

	otp_read_enable();
	for (i=15; i<31; i++) 
	{
		otp_lenc_data[i] = OV8850_read_cmos_sensor(otp_addr);
		otp_addr++;
	}
	otp_read_disable();
	otp_clear();
	
	// select next bank
	bank++;
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, bank);

	otp_addr = OTP_LENC_GROUP_ADDR;

	otp_read_enable();
	for (i=31; i<47; i++) 
	{
		otp_lenc_data[i] = OV8850_read_cmos_sensor(otp_addr);
		otp_addr++;
	}
	otp_read_disable();
	otp_clear();
	
	// select next bank
	bank++;
	OV8850_write_cmos_sensor(OTP_BANK_ADDR, bank);

	otp_addr = OTP_LENC_GROUP_ADDR;

	otp_read_enable();
	for (i=47; i<62; i++) 
	{
		otp_lenc_data[i] = OV8850_read_cmos_sensor(otp_addr);
		otp_addr++;
	}
	otp_read_disable();
	otp_clear();
	
//	TRACE("read lenc finished\n");
	return index;
}

/*******************************************************************************
* Function    :  otp_apply_lenc
* Description :  Apply lens correction setting to module
* Parameters  :  none
* Return      :  none
*******************************************************************************/	
void otp_apply_lenc(void)
{
	// write lens correction setting to registers
//	TRACE("apply lenc setting\n");

	unsigned char i;


	for (i=0; i<LENC_REG_SIZE; i++)
	{
		OV8850_write_cmos_sensor(LENC_START_ADDR+i, otp_lenc_data[i]);
	//	TRACE("0x%x, 0x%x\n", LENC_START_ADDR+i, otp_lenc_data[i]);
	}
}

/*******************************************************************************
* Function    :  otp_update_lenc
* Description :  Get lens correction setting from otp, then apply to module
* Parameters  :  none
* Return      :  1, success; 0, fail
*******************************************************************************/	
bool otp_update_lenc(void) 
{
//	TRACE("start lenc update\n");

	if (otp_read_lenc_group(-1) != -1)
	{
		otp_apply_lenc();
	//	TRACE("lenc update finished\n");
		return 1;
	}

//	TRACE("lenc update failed\n");
	return 0;
}

/****************For OTP feature end*************************/

#define Sleep(ms) mdelay(ms)

void OV8850_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = OV8850_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;
	
	OV8850DBDETAIL("OV8850_write_shutter , shutter=%d!!!!!\n", shutter);
	// write shutter, in number of line period

	
	OV8850DBDETAIL("!!ov8850.sensorMode=%d!!!!!\n", ov8850.sensorMode);
	if(ov8850.OV8850AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == ov8850.sensorMode )  //(g_iOV8850_Mode == OV8850_MODE_PREVIEW)	//SXGA size output
		{
			line_length = OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;
			max_shutter = OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines - 4; 
		}
		else	
		{
			line_length = OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;
			max_shutter = OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines - 4; 
		}

		//#if defined(MT6575)||defined(MT6577)

		#ifndef MT6573
		switch(OV8850CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				OV8850DBDETAIL("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  0!!\n");
				#if defined(ZSD15FPS)
				min_framelength = (ov8850.capPclk*10000) /(OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/148*10 ;
				#else
				min_framelength = (ov8850.capPclk*10000) /(OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/130*10 ;//13fps				
				#endif
				break;
			default:
				min_framelength = (ov8850.pvPclk*10000) /(OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/296*10 ; 
    			break;
		}
		#else
			min_framelength = (ov8850.pvPclk*10000) /(OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/296*10 ;
		#endif
		
		OV8850DBDETAIL("AutoFlickerMode!!! min_framelength for AutoFlickerMode = %d (0x%x)\n",min_framelength,min_framelength);
		OV8850DBDETAIL("max framerate(10 base) autofilker = %d\n",(ov8850.pvPclk*10000)*10 /line_length/min_framelength);

		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter)
			extra_lines = shutter - max_shutter; // + 4;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == ov8850.sensorMode )	//SXGA size output
		{
			frame_length = OV8850_PV_PERIOD_LINE_NUMS+ ov8850.DummyLines + extra_lines ; 
		}
		else				//QSXGA size output
		{ 
			frame_length = OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines + extra_lines ; 
		}
		OV8850DBDETAIL("frame_length 0= %d\n",frame_length);
		
		if (frame_length < min_framelength)
		{
			//shutter = min_framelength - 4;
			//#if defined(MT6575)||defined(MT6577)
			#ifndef MT6573
			switch(OV8850CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				extra_lines = min_framelength- (OV8850_FULL_PERIOD_LINE_NUMS+ ov8850.DummyLines);
				OV8850DB("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  1  extra_lines!!\n",extra_lines);
				break;
			default:
				extra_lines = min_framelength- (OV8850_PV_PERIOD_LINE_NUMS+ ov8850.DummyLines); 
    			break;
			}
			#else
			extra_lines = min_framelength- (OV8850_PV_PERIOD_LINE_NUMS+ ov8850.DummyLines);
			#endif
			frame_length = min_framelength;
		}	
		
		OV8850DBDETAIL("frame_length 1= %d\n",frame_length);
		
		ASSERT(line_length < OV8850_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < OV8850_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		OV8850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
		OV8850_write_cmos_sensor(0x380f, frame_length & 0xFF);
		spin_lock_irqsave(&ov8850mipiraw_drv_lock,flags);
		ov8850.maxExposureLines = frame_length;
		ov8850.framelength = frame_length;
		spin_unlock_irqrestore(&ov8850mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		OV8850_write_cmos_sensor(0x3500, (shutter>>12) & 0x07);
		OV8850_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV8850_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */
		
		OV8850DBDETAIL("frame_length 2= %d\n",frame_length);
		OV8850DB("framerate(10 base) = %d\n",(ov8850.pvPclk*10000)*10 /line_length/frame_length);
		
		OV8850DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	
	}
	else
	{
		//Max shutter = VTS-4
		if ( SENSOR_MODE_PREVIEW == ov8850.sensorMode )  //(g_iOV8850_Mode == OV8850_MODE_PREVIEW)	//SXGA size output
		{
			max_shutter = OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines - 4 ; 
		}
		else	
		{
			max_shutter = OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines - 4; 
		}
		OV8850DB("max_shutter=%d, ov8850.DummyLines=%d, shutter=%d, frame_length=%d\n", max_shutter, ov8850.DummyLines, shutter, frame_length);
		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter)
			extra_lines = shutter - max_shutter;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == ov8850.sensorMode )	//SXGA size output
		{
			line_length = OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels; 
			frame_length = OV8850_PV_PERIOD_LINE_NUMS+ ov8850.DummyLines + extra_lines ; 
		}
		else				//QSXGA size output
		{
			line_length = OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels; 
			frame_length = OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines + extra_lines ; 
		}
		//if(shutter == 1919){
			//frame_length += 4;
			//}

		ASSERT(line_length < OV8850_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < OV8850_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		OV8850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
		OV8850_write_cmos_sensor(0x380f, frame_length & 0xFF);
		spin_lock_irqsave(&ov8850mipiraw_drv_lock,flags);
		ov8850.maxExposureLines = frame_length -4;
		ov8850.framelength = frame_length;
		spin_unlock_irqrestore(&ov8850mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		//0x3500:B[2:0] => shutter[18:16]
		//0x3501:=>shutter[15:8]
		//0x3502:=>shutter[7:0]
		
		//OV8850_write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		OV8850_write_cmos_sensor(0x3500, (shutter>>12) & 0x07);
		OV8850_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV8850_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */
		
		OV8850DB("framerate(10 base) = %d\n",(ov8850.pvPclk*10000)*10 /line_length/frame_length);
		
		OV8850DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}
	
}   /* write_OV8850_shutter */

/*******************************************************************************
* 
********************************************************************************/
 static kal_uint16 OV8850Reg2Gain(const kal_uint16 iReg)
 {
	 //real gain is float type, from 1x to 32x, we recommend to use 1x ~ 8x gain for good noise performance.
	 //here use real gain
	 //kal_uint8 iI;
	 kal_uint16 iGain;    // 1x-gain base

	 iGain = iReg * ov8850.ispBaseGain/16;
	 

	 OV8850DBDETAIL("[OV8850Reg2Gain]real gain= %d\n",iGain);
	 return iGain ; //ov8850.realGain
 }

 kal_uint16 OV8850Gain2Reg(const kal_uint16 iGain)
{
    kal_uint16 iReg = 0x00;

	//iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
	iReg = iGain *16 / ov8850.ispBaseGain; //BASEGAIN;

	iReg = iReg & 0xFF;
//#ifdef OV8830_DRIVER_TRACE
	OV8850DB("OV8830Gain2Reg:iGain:%x; iReg:%x \n",iGain,iReg);
//#endif
    return iReg;
}





/*******************************************************************************
* 
********************************************************************************/

void write_OV8850_gain(kal_uint16 gain)
{

	//OV8850_write_cmos_sensor(0x350a,(gain>>8));
	OV8850_write_cmos_sensor(0x350b,(gain&0xff));

	return;
}

/*************************************************************************
* FUNCTION
*    OV8850_SetGain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    gain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8850_SetGain(UINT16 iGain)
{
	UINT16 gain_reg=0;
	unsigned long flags;
	spin_lock_irqsave(&ov8850mipiraw_drv_lock,flags);
	ov8850.realGain = iGain;
	ov8850.sensorGlobalGain = OV8850Gain2Reg(iGain);
	spin_unlock_irqrestore(&ov8850mipiraw_drv_lock,flags);
	
	if (ov8850.sensorGlobalGain < 0x10) //MINI gain is 0x10   16 = 1x
	 {
		 ov8850.sensorGlobalGain = 0x10;
	 }else if(ov8850.sensorGlobalGain > 0xFF) //max gain is 0xFF, it means nearly 16x, actually we use 8x for max gain.
	 {
		 ov8850.sensorGlobalGain = 0xFF;
	 }
	
	write_OV8850_gain(ov8850.sensorGlobalGain);	
	OV8850DB("[OV8850_SetGain]ov8850.sensorGlobalGain=0x%x,ov8850.realGain=%d\n",ov8850.sensorGlobalGain,ov8850.realGain);
	
}   /*  OV8850_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_OV8850_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 read_OV8850_gain(void)
{
    kal_uint8  temp_reg;
	kal_uint16 sensor_gain =0, read_gain=0;

	//read_gain=(((OV8850_read_cmos_sensor(0x350a)&0x01) << 8) | OV8850_read_cmos_sensor(0x350b));
	read_gain=(((OV8850_read_cmos_sensor(0x350a)&0x03) << 8) | OV8850_read_cmos_sensor(0x350b));

	sensor_gain = OV8850Reg2Gain(read_gain);
	//spin_lock(&ov8850mipiraw_drv_lock);
	//ov8850.sensorGlobalGain = read_gain; 
	//ov8850.realGain = OV8850Reg2Gain(ov8850.sensorGlobalGain);
	//spin_unlock(&ov8850mipiraw_drv_lock);
	
	//OV8850DB("ov8850.sensorGlobalGain=0x%x,ov8850.realGain=%d\n",ov8850.sensorGlobalGain,ov8850.realGain);
	OV8850DB("sensor_gain=0x%x,read_gain=%d\n",sensor_gain,read_gain);
	return sensor_gain;
}  /* read_OV8850_gain */


void OV8850_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=OV8850SensorReg[i].Addr; i++)
    {
        OV8850_write_cmos_sensor(OV8850SensorReg[i].Addr, OV8850SensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=OV8850SensorReg[i].Addr; i++)
    {
        OV8850_write_cmos_sensor(OV8850SensorReg[i].Addr, OV8850SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        OV8850_write_cmos_sensor(OV8850SensorCCT[i].Addr, OV8850SensorCCT[i].Para);
    }
}


/*************************************************************************
* FUNCTION
*    OV8850_sensor_to_camera_para
*
* DESCRIPTION
*    // update camera_para from sensor register
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8850_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=OV8850SensorReg[i].Addr; i++)
    {
         temp_data = OV8850_read_cmos_sensor(OV8850SensorReg[i].Addr);
		 spin_lock(&ov8850mipiraw_drv_lock);
		 OV8850SensorReg[i].Para =temp_data;
		 spin_unlock(&ov8850mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=OV8850SensorReg[i].Addr; i++)
    {
        temp_data = OV8850_read_cmos_sensor(OV8850SensorReg[i].Addr);
		spin_lock(&ov8850mipiraw_drv_lock);
		OV8850SensorReg[i].Para = temp_data;
		spin_unlock(&ov8850mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    OV8850_get_sensor_group_count
*
* DESCRIPTION
*    //
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_int32  OV8850_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void OV8850_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
   switch (group_idx)
   {
        case PRE_GAIN:
            sprintf((char *)group_name_ptr, "CCT");
            *item_count_ptr = 2;
            break;
        case CMMCLK_CURRENT:
            sprintf((char *)group_name_ptr, "CMMCLK Current");
            *item_count_ptr = 1;
            break;
        case FRAME_RATE_LIMITATION:
            sprintf((char *)group_name_ptr, "Frame Rate Limitation");
            *item_count_ptr = 2;
            break;
        case REGISTER_EDITOR:
            sprintf((char *)group_name_ptr, "Register Editor");
            *item_count_ptr = 2;
            break;
        default:
            ASSERT(0);
}
}

void OV8850_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0;
    kal_uint16 temp_gain=0, temp_addr=0, temp_para=0;
    
    switch (group_idx)
    {
        case PRE_GAIN:
           switch (item_idx)
          {
              case 0:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-R");
                  temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gr");
                  temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gb");
                  temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-B");
                  temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                 sprintf((char *)info_ptr->ItemNamePtr,"SENSOR_BASEGAIN");
                 temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

            temp_para= OV8850SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/ov8850.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= OV8850_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= OV8850_MAX_ANALOG_GAIN * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");
                
                    //temp_reg=MT9P017SensorReg[CMMCLK_CURRENT_INDEX].Para;
                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                    {
                        info_ptr->ItemValue=2;
                    }
                    else if(temp_reg==ISP_DRIVING_4MA)
                    {
                        info_ptr->ItemValue=4;
                    }
                    else if(temp_reg==ISP_DRIVING_6MA)
                    {
                        info_ptr->ItemValue=6;
                    }
                    else if(temp_reg==ISP_DRIVING_8MA)
                    {
                        info_ptr->ItemValue=8;
                    }
                
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    5998; //111;  //MT9P017_MAX_EXPOSURE_LINES;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"Min Frame Rate");
                    info_ptr->ItemValue=5; //12;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Addr.");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Value");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                default:
                ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
}



kal_bool OV8850_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//   kal_int16 temp_reg;
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;

   switch (group_idx)
    {
        case PRE_GAIN:
            switch (item_idx)
            {
              case 0:
                temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

		 temp_gain=((ItemValue*BASEGAIN+500)/1000);			//+500:get closed integer value

		  if(temp_gain>=1*BASEGAIN && temp_gain<=16*BASEGAIN)
          {
//             temp_para=(temp_gain * ov8850.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }          
          else
			  ASSERT(0);

			 OV8850DBDETAIL("OV8850????????????????????? :\n ");
		  spin_lock(&ov8850mipiraw_drv_lock);
          OV8850SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&ov8850mipiraw_drv_lock);
          OV8850_write_cmos_sensor(OV8850SensorCCT[temp_addr].Addr,temp_para);

            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    //no need to apply this item for driving current
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            ASSERT(0);
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
					spin_lock(&ov8850mipiraw_drv_lock);
                    OV8850_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&ov8850mipiraw_drv_lock);
                    break;
                case 1:
                    OV8850_write_cmos_sensor(OV8850_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
    return KAL_TRUE;
}

static void OV8850_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == ov8850.sensorMode )	//SXGA size output
	{
		line_length = OV8850_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV8850_PV_PERIOD_LINE_NUMS + iLines;
	}
	else				//QSXGA size output
	{
		line_length = OV8850_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV8850_FULL_PERIOD_LINE_NUMS + iLines;
	}
	
	//if(ov8850.maxExposureLines > frame_length -4 )
	//	return;
	
	//ASSERT(line_length < OV8850_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < OV8850_MAX_FRAME_LENGTH);	//0xFFFF
	
	//Set total frame length
	OV8850_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
	OV8850_write_cmos_sensor(0x380f, frame_length & 0xFF);
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.maxExposureLines = frame_length -4;
	spin_unlock(&ov8850mipiraw_drv_lock);

	//Set total line length
	OV8850_write_cmos_sensor(0x380c, (line_length >> 8) & 0xFF);
	OV8850_write_cmos_sensor(0x380d, line_length & 0xFF);
	
}   /*  OV8850_SetDummy */

void OV8850PreviewSetting(void)
{	
    OV8850DB("OV8850PreviewSetting enter :\n ");
	
	//;//OV8830_1632*1224_setting_2lanes_520Mbps/lane_72.22MSCLK30fps
	//;//Base_on_OV8850_APP_R1.0
	//;//2012_2_27
	//;//Tony Li
	//;;;;;;;;;;;;;Any modify please inform to OV FAE;;;;;;;;;;;;;;;


	//OV8850_1632_1224_2Lane__208Msclk_641.333Mbps_30fps
	//Base_on_OV8830_APP_R1.05(OV8850_R1AM17D.ovd)
	//2012_7_23
	//Tony Li
	//;;;;;;;;;;;;;Any modify please inform to OV FAE;;;;;;;;;;;;;;;
	
	//Slave_ID=0x20;
	
	OV8850_write_cmos_sensor(0x0100, 0x00); //software standby
	OV8850_write_cmos_sensor(0x3708, 0xe6); //analog control	
	OV8850_write_cmos_sensor(0x3709, 0x03); //analog control	
	OV8850_write_cmos_sensor(0x3800, 0x00); //X Start			
	OV8850_write_cmos_sensor(0x3801, 0x00); //X Start			
	OV8850_write_cmos_sensor(0x3802, 0x00); //Y Start			
	OV8850_write_cmos_sensor(0x3803, 0x00); //Y Start			
	OV8850_write_cmos_sensor(0x3804, 0x0c); //X End 			
	OV8850_write_cmos_sensor(0x3805, 0xcf); //X End 			
	OV8850_write_cmos_sensor(0x3806, 0x09); //Y End 			
	OV8850_write_cmos_sensor(0x3807, 0x9f); //Y End 			
	OV8850_write_cmos_sensor(0x3808, 0x06); //X Output Size =	1632
	OV8850_write_cmos_sensor(0x3809, 0x60); //X Output Size 	
	OV8850_write_cmos_sensor(0x380a, 0x04); //Y Output Size =	1224
	OV8850_write_cmos_sensor(0x380b, 0xc8); //Y Output Size 	
	OV8850_write_cmos_sensor(0x380c, 0x0e); //HTS = 	3608		
	OV8850_write_cmos_sensor(0x380d, 0x18); //HTS				
	OV8850_write_cmos_sensor(0x380e, 0x07); //VTS = 	1920		
	OV8850_write_cmos_sensor(0x380f, 0x80); //c2 ; VTS				 
	OV8850_write_cmos_sensor(0x3814, 0x31); //X INC 			
	OV8850_write_cmos_sensor(0x3815, 0x31); //X INC 			
	OV8850_write_cmos_sensor(0x3820, 0x11); //flip off, bin on	
	OV8850_write_cmos_sensor(0x3821, 0x0f); //mirror on, bin on 
	OV8850_write_cmos_sensor(0x3a04, 0x03); //					
	OV8850_write_cmos_sensor(0x3a05, 0xb0); //					
	OV8850_write_cmos_sensor(0x4004, 0x04); //					
	OV8850_write_cmos_sensor(0x5002, 0x08); //

	//OV8850_write_cmos_sensor(0x4005, 0x18); // add from tony 0905
	//OV8850_write_cmos_sensor(0x404f, 0x90); // add from tony 0905
	OV8850_write_cmos_sensor(0x4005, 0x1A); // add from tony 0910
	OV8850_write_cmos_sensor(0x404f, 0x7F); // add from tony 0910
	
	OV8850_write_cmos_sensor(0x3011, 0x21); // 2 Lane, MIPI enable
	OV8850_write_cmos_sensor(0x3015, 0xca); //MIPI mode, disable lane 4 and lane 3
	OV8850_write_cmos_sensor(0x3090, 0x02); //PLL
	OV8850_write_cmos_sensor(0x3091, 0x10); //;12); //PLL
	OV8850_write_cmos_sensor(0x3092, 0x00); //PLL
	OV8850_write_cmos_sensor(0x3093, 0x00); //PLL
	OV8850_write_cmos_sensor(0x3099, 0x1c);
	OV8850_write_cmos_sensor(0x30b3, 0x4a); // ;50 ; PLL
	OV8850_write_cmos_sensor(0x30b4, 0x03); //PLL
	OV8850_write_cmos_sensor(0x30b5, 0x04); //PLL
	OV8850_write_cmos_sensor(0x30b6, 0x01); //PLL
	OV8850_write_cmos_sensor(0x4837, 0x0c); //0d ; MIPI pclk period
	OV8850_write_cmos_sensor(0x0100, 0x01); //wake up from software standby
	
	//102 80 1
	//102 82 660
	//102 83 4c8
	//102 3601 BB8;30fps

	//mDELAY(5);
	
    OV8850DB("OV8850PreviewSetting exit :\n ");
}

void OV8850CaptureSetting(void)
{	
    OV8850DB("OV8850CaptureSetting enter :\n ");
	//;//OV8850_3264_2448_2Lane__138.667Msclk_641.333Mbps_15fps
	//;//Base_on_OV8830_APP_R1.05(OV8850_R1AM17D.ovd)
	//;//2012_7_23
	//;//Tony Li
	//;;;;;;;;;;;;;Any modify please inform to OV FAE;;;;;;;;;;;;;;;

	//;//Slave_ID=0x20;


	OV8850_write_cmos_sensor(0x0100, 0x00); // software standby
	OV8850_write_cmos_sensor(0x3708, 0xe4); // analog control    
	OV8850_write_cmos_sensor(0x3709, 0xc3); // analog control    
	OV8850_write_cmos_sensor(0x3800, 0x00); // X Start           
	OV8850_write_cmos_sensor(0x3801, 0x04); // X Start           
	OV8850_write_cmos_sensor(0x3802, 0x00); // Y Start           
	OV8850_write_cmos_sensor(0x3803, 0x0c); // Y Start           
	OV8850_write_cmos_sensor(0x3804, 0x0c); // X End             
	OV8850_write_cmos_sensor(0x3805, 0xcb); // X End             
	OV8850_write_cmos_sensor(0x3806, 0x09); // Y End             
	OV8850_write_cmos_sensor(0x3807, 0xa3); // Y End             
	OV8850_write_cmos_sensor(0x3808, 0x0c); // X Output Size =   3264
	OV8850_write_cmos_sensor(0x3809, 0xc0); // X Output Size     
	OV8850_write_cmos_sensor(0x380a, 0x09); // Y Output Size =   2448
	OV8850_write_cmos_sensor(0x380b, 0x90); // Y Output Size     
	OV8850_write_cmos_sensor(0x380c, 0x0e); // HTS =             3608
	OV8850_write_cmos_sensor(0x380d, 0x18); // HTS               
	OV8850_write_cmos_sensor(0x380e, 0x09); // VTS =             2559
	OV8850_write_cmos_sensor(0x380f, 0xff); //b4 ; VTS               
	OV8850_write_cmos_sensor(0x3814, 0x11); // X INC             
	OV8850_write_cmos_sensor(0x3815, 0x11); // X INC             
	OV8850_write_cmos_sensor(0x3820, 0x10); // flip off, bin on  
	OV8850_write_cmos_sensor(0x3821, 0x0e); // mirror on, bin on 
	OV8850_write_cmos_sensor(0x3a04, 0x09); //                   
	OV8850_write_cmos_sensor(0x3a05, 0xa9); //                   
	OV8850_write_cmos_sensor(0x4004, 0x08); //                   
	OV8850_write_cmos_sensor(0x5002, 0x88); //      

	
	OV8850_write_cmos_sensor(0x4005, 0x1A); // add from tony 0905

	OV8850_write_cmos_sensor(0x3011, 0x21); // 2 Lane, MIPI enable
	OV8850_write_cmos_sensor(0x3015, 0xca); // MIPI mode, disable lane 4 and lane 3
	OV8850_write_cmos_sensor(0x3090, 0x03); // PLL
	OV8850_write_cmos_sensor(0x3091, 0x20); // PLL
	OV8850_write_cmos_sensor(0x3092, 0x01); // PLL
	OV8850_write_cmos_sensor(0x3093, 0x00); // PLL
	OV8850_write_cmos_sensor(0x3099, 0x1c);
	OV8850_write_cmos_sensor(0x30b3, 0x4a); //50 ; PLL
	OV8850_write_cmos_sensor(0x30b4, 0x03); // PLL
	OV8850_write_cmos_sensor(0x30b5, 0x04); // PLL
	OV8850_write_cmos_sensor(0x30b6, 0x01); // PLL
	OV8850_write_cmos_sensor(0x4837, 0x0c); //0d ; MIPI pclk period
	OV8850_write_cmos_sensor(0x0100, 0x01); // wake up from software standby

	//;;//102 80 1
	//;;//102 82 cc0
	//;;//102 83 990
	//;;//102 3601 960;24fps
	//mDELAY(20);
	OV8850DB("OV8850CaptureSetting exit :\n ");
}

static void OV8850_Sensor_Init(void)
{
	OV8850DB("OV8850_Sensor_Init enter :\n ");	
	
	//OV8850_write_cmos_sensor(0x0103,0x01 );//software reset
	//Sleep(5);//; delay(5ms)



	//OV8850_Global_setting
	//Base_on_OV8830_APP_R1.05(OV8850_R1AM17D.ovd)
	//2012_7_23
	//Tony Li
	//;;;;;;;;;;;;;Any modify please inform to OV FAE;;;;;;;;;;;;;;;
	
	//Slave_ID=0x6c;
	//@@OV8850_Initial
	
	OV8850_write_cmos_sensor(0x0103, 0x01); //software reset
	mDELAY(5); 
	OV8850_write_cmos_sensor(0x3625, 0x40); //ADC and analog 
	OV8850_write_cmos_sensor(0x3602, 0x50); //ADC and analog 
	OV8850_write_cmos_sensor(0x3620, 0x64); //ADC and analog 
	OV8850_write_cmos_sensor(0x3623, 0x68); //ADC and analog;;;; 
	OV8850_write_cmos_sensor(0x3634, 0x03); //ADC and analog;;;; 
	OV8850_write_cmos_sensor(0x3022, 0x02); //pd_pixelvdd_sel
	OV8850_write_cmos_sensor(0x3631, 0xb3); //ADC and analog 
	OV8850_write_cmos_sensor(0x3633, 0x34); //ADC and analog 
	OV8850_write_cmos_sensor(0x3754, 0xc0); //sensor control 
	OV8850_write_cmos_sensor(0x3703, 0x2e); //sensor control 
	OV8850_write_cmos_sensor(0x3756, 0x2a); //sensor control 
	OV8850_write_cmos_sensor(0x3622, 0x0f); //ADC and analog 
	OV8850_write_cmos_sensor(0x3739, 0x72); //sensor control 
	OV8850_write_cmos_sensor(0x3014, 0x4a);
	OV8850_write_cmos_sensor(0x4d00, 0x04);
	OV8850_write_cmos_sensor(0x4d01, 0x52); 	 
	OV8850_write_cmos_sensor(0x4d03, 0xf1); 	 
	OV8850_write_cmos_sensor(0x4d04, 0x19);
	OV8850_write_cmos_sensor(0x4d05, 0x99);
	OV8850_write_cmos_sensor(0x4008, 0x24);
	
	OV8850_write_cmos_sensor(0x4000, 0x80); //BLC bypass		   
	
	OV8850_write_cmos_sensor(0x376b, 0x44); //sensor control											
	OV8850_write_cmos_sensor(0x379c, 0x0c); //PSRAM control 											
	OV8850_write_cmos_sensor(0x3781, 0x0c); //PSRAM control 											
	OV8850_write_cmos_sensor(0x366f, 0x20); //ADC and analog											
	OV8850_write_cmos_sensor(0x4002, 0xc5); //BCL redo when format change, BLC auto, do BLC for 2 frames
	OV8850_write_cmos_sensor(0x3081, 0x02); //PLL														
	OV8850_write_cmos_sensor(0x3083, 0x01); //PLL														
	OV8850_write_cmos_sensor(0x3503, 0x07);
	OV8850_write_cmos_sensor(0x3500, 0x00);
	OV8850_write_cmos_sensor(0x3501, 0x00);
	OV8850_write_cmos_sensor(0x3502, 0x80);
	OV8850_write_cmos_sensor(0x350a, 0x00);
	OV8850_write_cmos_sensor(0x350b, 0x0f);
	OV8850_write_cmos_sensor(0x3002, 0x08); //Vsync, Frex input, STROBE output
	OV8850_write_cmos_sensor(0x3004, 0x00);
	OV8850_write_cmos_sensor(0x3005, 0x00); //STROBE output, IL_PWM input
	OV8850_write_cmos_sensor(0x3104, 0xa1); //PLL						 
	OV8850_write_cmos_sensor(0x3106, 0x01);
	OV8850_write_cmos_sensor(0x3011, 0x41); //MIPI 2 lane, MIPI enable					 
	OV8850_write_cmos_sensor(0x3012, 0x08); //MIPI_pad									 
	OV8850_write_cmos_sensor(0x3015, 0x0a); //MIPI data lane enable, enable MIPI/disable 
	OV8850_write_cmos_sensor(0x3090, 0x02); //PLL										 
	OV8850_write_cmos_sensor(0x3091, 0x12); //PLL										 
	OV8850_write_cmos_sensor(0x3092, 0x00); //PLL										 
	OV8850_write_cmos_sensor(0x3094, 0x00); //PLL
	OV8850_write_cmos_sensor(0x3099, 0x1e); // ;;;
	OV8850_write_cmos_sensor(0x309c, 0x00); // ;;;									   
	OV8850_write_cmos_sensor(0x30b3, 0x64); //PLL multiplier							 
	OV8850_write_cmos_sensor(0x30b4, 0x03); //PLL1_pre_pll_div							 
	OV8850_write_cmos_sensor(0x30b5, 0x04); //PLL1_op_pix_div							 
	OV8850_write_cmos_sensor(0x30b6, 0x01); //PLL1_op_sys_div							 
	OV8850_write_cmos_sensor(0x364c, 0x00); 						 
	OV8850_write_cmos_sensor(0x364d, 0x00);
	OV8850_write_cmos_sensor(0x364e, 0x00);
	OV8850_write_cmos_sensor(0x364f, 0x00);
	OV8850_write_cmos_sensor(0x3660, 0x80);
	OV8850_write_cmos_sensor(0x3662, 0x10); //ADC and analog
	OV8850_write_cmos_sensor(0x3665, 0x00); //ADC and analog
	OV8850_write_cmos_sensor(0x3666, 0x00); //ADC and analog
	OV8850_write_cmos_sensor(0x3680, 0xe0); //ADC and analog
	OV8850_write_cmos_sensor(0x3702, 0xdb); //sensor control
	OV8850_write_cmos_sensor(0x3704, 0x71); //sensor control
	OV8850_write_cmos_sensor(0x371f, 0x18);
	OV8850_write_cmos_sensor(0x3739, 0x30);
	OV8850_write_cmos_sensor(0x373c, 0x38);
	OV8850_write_cmos_sensor(0x373a, 0x51);
	OV8850_write_cmos_sensor(0x3759, 0x0f);
	OV8850_write_cmos_sensor(0x3781, 0x0c); //PSRAM control
	OV8850_write_cmos_sensor(0x3786, 0x16); //PSRAM control
	OV8850_write_cmos_sensor(0x3795, 0x00);
	OV8850_write_cmos_sensor(0x3796, 0x78);
	OV8850_write_cmos_sensor(0x3800, 0x00);
	OV8850_write_cmos_sensor(0x3804, 0x0c);
	OV8850_write_cmos_sensor(0x380c, 0x0e);
	OV8850_write_cmos_sensor(0x380d, 0x18);
	OV8850_write_cmos_sensor(0x3810, 0x00);
	OV8850_write_cmos_sensor(0x3811, 0x04);
	OV8850_write_cmos_sensor(0x3812, 0x00);
	OV8850_write_cmos_sensor(0x3813, 0x04);
	OV8850_write_cmos_sensor(0x3826, 0x00);
	OV8850_write_cmos_sensor(0x4000, 0x10); //BLC enable	
	OV8850_write_cmos_sensor(0x4001, 0x02); //BLC start line
	OV8850_write_cmos_sensor(0x4100, 0x04);
	OV8850_write_cmos_sensor(0x4101, 0x04);
	OV8850_write_cmos_sensor(0x4102, 0x04);
	OV8850_write_cmos_sensor(0x4104, 0x04);
	OV8850_write_cmos_sensor(0x4109, 0x04);
	OV8850_write_cmos_sensor(0x4300, 0xff); //data max[9:8]    
	OV8850_write_cmos_sensor(0x4301, 0x00); //data max[7:0]    
	OV8850_write_cmos_sensor(0x4315, 0x00); //Vsync delay[15:8]
	OV8850_write_cmos_sensor(0x4837, 0x08); //MIPI PCLK period 
	OV8850_write_cmos_sensor(0x4a00, 0xaa); //LVDS			   
	OV8850_write_cmos_sensor(0x4a03, 0x01); //LVDS			   
	OV8850_write_cmos_sensor(0x4a05, 0x08); //LVDS	

	OV8850_write_cmos_sensor(0x3021, 0x00);
	
	OV8850_write_cmos_sensor(0x5000, 0x86); //lenc off, wbc on
	OV8850_write_cmos_sensor(0x5001, 0x01); //MWB
	OV8850_write_cmos_sensor(0x3400, 0x04); //red gain h
	OV8850_write_cmos_sensor(0x3401, 0x00); //red gain l
	OV8850_write_cmos_sensor(0x3402, 0x04); //green gain h
	OV8850_write_cmos_sensor(0x3403, 0x00); //green gain l
	OV8850_write_cmos_sensor(0x3404, 0x04); //blue gain h
	OV8850_write_cmos_sensor(0x3405, 0x00); //blue gain l
	OV8850_write_cmos_sensor(0x3406, 0x01);
	OV8850_write_cmos_sensor(0x4000, 0x18); //BLC gain trigger
	OV8850_write_cmos_sensor(0x4002, 0x45); //BLC same when format change, BLC auto
	OV8850_write_cmos_sensor(0x4005, 0x18); // [bit 0] 1: BLC update every frame. 0: BLC trig by gain change
	//OV8850_write_cmos_sensor(0x4005, 0x19); // [bit 0] 1: BLC update every frame. 0: BLC trig by gain change
	OV8850_write_cmos_sensor(0x4009, 0x40); //BLC target 0x10
	OV8850_write_cmos_sensor(0x3503, 0x07); //Gain delay 1 frame latch, use 0x3503[5] to control whether delay 1 frame latch,
	OV8850_write_cmos_sensor(0x3500, 0x00); //Gain delay 1 frame latch, use 0x3503[5] to control whether delay 1 frame latch,
				 // VTS auto(??), AGC auto(?), AEC auto(?)
	OV8850_write_cmos_sensor(0x3501, 0x29); //exposure h
	OV8850_write_cmos_sensor(0x3502, 0x00); //exposure l
	OV8850_write_cmos_sensor(0x350b, 0x78); //gain 7.5x20 371f 18

	OV8850_write_cmos_sensor(0x3509, 0x10);  //use real gain by hesong 0829
	
	OV8850_write_cmos_sensor(0x0100, 0x01);

	//OV8850_write_cmos_sensor(0x3608, 0x40);////////////close internel dvdd
		
    OV8850DB("OV8850_Sensor_Init exit :\n ");
	
}   /*  OV8850_Sensor_Init  */

/*************************************************************************
* FUNCTION
*   OV8850Open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/

UINT32 OV8850Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;
	
	OV8850DB("OV8850Open enter :\n ");
	OV8850_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(5);

	//  Read sensor ID to adjust I2C is OK?
	for(i=0;i<3;i++)
	{
		sensor_id = (OV8850_read_cmos_sensor(0x300A)<<8)|OV8850_read_cmos_sensor(0x300B);
		OV8850DB("OOV8850 READ ID :%x",sensor_id);
		if(sensor_id != OV8850_SENSOR_ID)
		{
			return ERROR_SENSOR_CONNECT_FAIL;
		}else
			break;
	}
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.sensorMode = SENSOR_MODE_INIT;
	ov8850.OV8850AutoFlickerMode = KAL_FALSE;
	ov8850.OV8850VideoMode = KAL_FALSE;
	spin_unlock(&ov8850mipiraw_drv_lock);
	OV8850_Sensor_Init();

	/************For OTP feature start***************/
	otp_update_wb(0x58,0x4A);
	otp_update_lenc();
	
	/************For OTP feature end***************/
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.DummyLines= 0;
	ov8850.DummyPixels= 0;
	
	ov8850.pvPclk =  (20800); //(13867);
	spin_unlock(&ov8850mipiraw_drv_lock);
	#ifndef MT6573
	//#if defined(MT6575)||defined(MT6577)
    	switch(OV8850CurrentScenarioId)
		{
			case MSDK_SCENARIO_ID_CAMERA_ZSD:				
				#if defined(ZSD15FPS)
				spin_lock(&ov8850mipiraw_drv_lock);
				ov8850.capPclk = (13867); //(13867);//15fps
				spin_unlock(&ov8850mipiraw_drv_lock);
				#else
				spin_lock(&ov8850mipiraw_drv_lock);
				ov8850.capPclk = (13867); //(13867);//13fps////////////need check
				spin_unlock(&ov8850mipiraw_drv_lock);
				#endif
				break;        			
        	default:
				spin_lock(&ov8850mipiraw_drv_lock);
				ov8850.capPclk = (13867);//(13867);
				spin_unlock(&ov8850mipiraw_drv_lock);
				break;  
          }
	#else
		spin_lock(&ov8850mipiraw_drv_lock);
		ov8850.capPclk = (13867);//(13867);
		spin_unlock(&ov8850mipiraw_drv_lock);
	#endif
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.shutter = 0x4EA;
	ov8850.pvShutter = 0x4EA;
	ov8850.maxExposureLines =OV8850_PV_PERIOD_LINE_NUMS -4;
	
	ov8850.ispBaseGain = BASEGAIN;//0x40
	ov8850.sensorGlobalGain = 0x1e;//sensor gain read from 0x350a 0x350b; 0x1f as 3.875x
	ov8850.pvGain = 0x1e;
	ov8850.realGain = OV8850Reg2Gain(0x1e);//ispBaseGain as 1x
	spin_unlock(&ov8850mipiraw_drv_lock);
	//OV8850DB("OV8850Reg2Gain(0x1f)=%x :\n ",OV8850Reg2Gain(0x1f));
	
	OV8850DB("OV8850Open exit :\n ");
	
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   OV8850GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8850GetSensorID(UINT32 *sensorID) 
{
    int  retry = 1; 
	
	OV8850DB("OV8850GetSensorID enter :\n ");
	OV8850_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);
	
    // check if sensor ID correct
    do {
        *sensorID = (OV8850_read_cmos_sensor(0x300A)<<8)|OV8850_read_cmos_sensor(0x300B);
		OV8850DB("Sensor ID = 0x%04x\n", *sensorID);
        if (*sensorID == OV8850_SENSOR_ID)
        	{
        		OV8850DB("Sensor ID = 0x%04x\n", *sensorID);
            	break; 
        	}
        OV8850DB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);

	// *sensorID = (OV8850_read_cmos_sensor_1(0x300A)<<8)|OV8850_read_cmos_sensor_1(0x300B);
	//OV8850DB("Sensor ID = 0x%04x\n", *sensorID);
    if (*sensorID != OV8850_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   OV8850_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of OV8850 to change exposure time.
*
* PARAMETERS
*   shutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8850_SetShutter(kal_uint32 iShutter)
{
	OV8850DB("OV8850_SetShutter enter : iShutter = %d\n ", iShutter);
	if(ACDK_SCENARIO_ID_CAMERA_ZSD == OV8850CurrentScenarioId )
	{
		//OV8850DB("always UPDATE SHUTTER when ov8850.sensorMode == SENSOR_MODE_CAPTURE\n");
	}
	else{
		if(ov8850.sensorMode == SENSOR_MODE_CAPTURE) 
		{
			//OV8850DB("capture!!DONT UPDATE SHUTTER!!\n"); 
			//return;
		}
	}
	//if(ov8850.shutter == iShutter)
		//return;
   spin_lock(&ov8850mipiraw_drv_lock);
   ov8850.shutter= iShutter;
   spin_unlock(&ov8850mipiraw_drv_lock);
   OV8850_write_shutter(iShutter);
   return;
}   /*  OV8850_SetShutter   */



/*************************************************************************
* FUNCTION
*   OV8850_read_shutter
*
* DESCRIPTION
*   This function to  Get exposure time.
*
* PARAMETERS
*   None
*
* RETURNS
*   shutter : exposured lines
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8850_read_shutter(void)
{

	// read shutter, in number of line period
	//max exposure time <=VTS-4
	//0x3500:B[2:0] => shutter[18:16]
	//0x3501:=>shutter[15:8]
	//0x3502:=>shutter[7:0]
	
	kal_uint16 temp_reg1, temp_reg2 ,temp_reg3;
	UINT32 shutter =0;
	temp_reg1 = OV8850_read_cmos_sensor(0x3500);    // AEC[b19~b16]
	OV8850DB("[OV8850_read_shutter] temp_reg1 = 0x%04x\n", temp_reg1);
	temp_reg2 = OV8850_read_cmos_sensor(0x3501);    // AEC[b15~b8]
	OV8850DB("[OV8850_read_shutter] temp_reg2 = 0x%04x\n", temp_reg2);
	temp_reg3 = OV8850_read_cmos_sensor(0x3502);    // AEC[b7~b0]
	OV8850DB("[OV8850_read_shutter] temp_reg3 = 0x%04x\n", temp_reg3); //ignore the fraction bits
	//read out register value and divide 16;
	shutter  = ((temp_reg1&0x07) <<12)| (temp_reg2<<4)|(temp_reg3>>4);
	OV8850DB("[OV8850_read_shutter] shutter = 0x%04x\n", shutter);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   OV8850_night_mode
*
* DESCRIPTION
*   This function night mode of OV8850.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8850_NightMode(kal_bool bEnable)
{
}/*	OV8850_NightMode */



/*************************************************************************
* FUNCTION
*   OV8850Close
*
* DESCRIPTION
*   This function is to turn off sensor module power.
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8850Close(void)
{
    //  CISModulePowerOn(FALSE);
    //s_porting
    //  DRV_I2CClose(OV8850hDrvI2C);
    //e_porting
    return ERROR_NONE;
}	/* OV8850Close() */

void OV8850SetFlipMirror(kal_int32 imgMirror)
{
	
	kal_int16 mirror=0,flip=0;

	OV8850DB("[OV8850SetFlipMirror] imgMirror = 0x%04x\n", imgMirror);

	//imgMirror = IMAGE_H_MIRROR;
	
	mirror= OV8850_read_cmos_sensor(0x3821);
	flip  = OV8850_read_cmos_sensor(0x3820);
    switch (imgMirror)
    {
        case IMAGE_H_MIRROR://IMAGE_NORMAL: 
            OV8850_write_cmos_sensor(0x3820, (flip & (0xF9)));//Set normal
            OV8850_write_cmos_sensor(0x3821, (mirror & (0xF9)));	//Set normal
            break;
        case IMAGE_NORMAL: //IMAGE_NORMAL://IMAGE_V_MIRROR: 
            OV8850_write_cmos_sensor(0x3820, (flip & (0xF9)));//Set normal
            OV8850_write_cmos_sensor(0x3821, (mirror | (0x06)));	//Set mirror
            break;
        case IMAGE_HV_MIRROR: //IMAGE_HV_MIRROR://IMAGE_H_MIRROR: 
            OV8850_write_cmos_sensor(0x3820, (flip |(0x06)));	//Set flip
            OV8850_write_cmos_sensor(0x3821, (mirror & (0xF9)));	//Set Normal
            break;
        case IMAGE_V_MIRROR: //IMAGE_V_MIRROR://IMAGE_HV_MIRROR: 
            OV8850_write_cmos_sensor(0x3820, (flip |(0x06)));	//Set mirror & flip
            OV8850_write_cmos_sensor(0x3821, (mirror |(0x06)));	//Set mirror & flip
            break;
    }
}


/*************************************************************************
* FUNCTION
*   OV8850Preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8850Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	OV8850DB("OV8850Preview enter:");
	OV8850DB("OV8850Preview enter: ov8850.sensorMode = %d", ov8850.sensorMode);
	// preview size 
	if(ov8850.sensorMode == SENSOR_MODE_PREVIEW) 
	{
		// do nothing	
		// FOR CCT PREVIEW
	}
	else
	{
		//OV8850DB("OV8850Preview setting!!\n");
		OV8850PreviewSetting();

	}
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	spin_unlock(&ov8850mipiraw_drv_lock);
	ov8850.DummyPixels =0;
	ov8850.DummyLines  =0;
	
	//set mirror & flip
	//OV8850DB("[OV8850Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov8850mipiraw_drv_lock);
	OV8850SetFlipMirror(sensor_config_data->SensorImageMirror);

	
	//OV8850_write_shutter(ov8850.shutter); //mask by hesong 08/13
	//write_OV8850_gain(ov8850.pvGain);
	
	OV8850DBDETAIL("[OV8850Preview]frame_len=%x\n", ((OV8850_read_cmos_sensor(0x380e)<<8)+OV8850_read_cmos_sensor(0x380f)));
	
	OV8850DB("OV8850Preview exit:\n");
    return ERROR_NONE;
}	/* OV8850Preview() */

UINT32 OV8850Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = ov8850.shutter;
	kal_uint32 pv_line_length , cap_line_length,temp_gain;

	//return ERROR_NONE;

	if( SENSOR_MODE_CAPTURE== ov8850.sensorMode)
	{
		OV8850DB("OV8850Capture BusrtShot!!!\n");
	}else{
	OV8850DB("OV8850Capture enter:\n");
	
	//Record Preview shutter & gain
	shutter=OV8850_read_shutter();
	temp_gain =  read_OV8850_gain();
	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.pvShutter =shutter;
	//ov8850.sensorGlobalGain = temp_gain;
	//ov8850.pvGain = temp_gain; //ov8850.sensorGlobalGain;
	spin_unlock(&ov8850mipiraw_drv_lock);
	
	OV8850DB("[OV8850Capture]ov8850.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",ov8850.shutter, shutter,temp_gain);
	
	// Full size setting
	OV8850CaptureSetting();
	
	spin_lock(&ov8850mipiraw_drv_lock);		
	ov8850.sensorMode = SENSOR_MODE_CAPTURE;
	//set mirror & flip
	ov8850.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov8850mipiraw_drv_lock);
	OV8850DB("[OV8850Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
	OV8850SetFlipMirror(sensor_config_data->SensorImageMirror);

	
	// set dummy
	//ov8850.DummyPixels =0;
	//ov8850.DummyLines  =0;
	//OV8850_SetDummy( ov8850.DummyPixels, ov8850.DummyLines);
	
	// Full size setting
	#ifndef MT6573
	//#if defined(MT6575)||defined(MT6577)
    if(OV8850CurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
    {
		OV8850DB("OV8850Capture exit ZSD!!\n");
		return;
    }
	#endif
	//calculate shutter
	pv_line_length = OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;
	cap_line_length = OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;	
	
	OV8850DB("[OV8850Capture]pv_line_length =%d,cap_line_length =%d\n",pv_line_length,cap_line_length);
	OV8850DB("[OV8850Capture]pv_shutter =%d\n",shutter );
	
	shutter =  shutter * pv_line_length / cap_line_length;
	shutter = shutter *ov8850.capPclk / ov8850.pvPclk;
	//Do not double shutter here.
	//shutter *= 2; //preview bining///////////////////////////////////////
	
	
	if(shutter < 3)
	    shutter = 3;	

	OV8850_write_shutter(shutter);	
	
	//gain = read_OV8850_gain();
	
	OV8850DB("[OV8850Capture]cap_shutter =%d\n",shutter); //,read_OV8850_gain());
	//write_OV8850_gain(ov8850.sensorGlobalGain);	
	
	OV8850DB("OV8850Capture exit:\n");
	}

    return ERROR_NONE;
}	/* OV8850Capture() */

UINT32 OV8850GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    OV8850DB("OV8850GetResolution!!\n");

	#ifndef MT6573
	//#if defined(MT6575)||defined(MT6577)
		switch(OV8850CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				pSensorResolution->SensorPreviewWidth	= OV8850_IMAGE_SENSOR_FULL_WIDTH;
    			pSensorResolution->SensorPreviewHeight	= OV8850_IMAGE_SENSOR_FULL_HEIGHT;
				break;
			default:
				pSensorResolution->SensorPreviewWidth	= OV8850_IMAGE_SENSOR_PV_WIDTH;
    			pSensorResolution->SensorPreviewHeight	= OV8850_IMAGE_SENSOR_PV_HEIGHT;
    			break;
		}
	#else
	pSensorResolution->SensorPreviewWidth	= OV8850_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= OV8850_IMAGE_SENSOR_PV_HEIGHT;
	#endif

    pSensorResolution->SensorFullWidth		= OV8850_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= OV8850_IMAGE_SENSOR_FULL_HEIGHT; 

//    OV8850DB("SensorPreviewWidth:  %d.\n", pSensorResolution->SensorPreviewWidth);     
//    OV8850DB("SensorPreviewHeight: %d.\n", pSensorResolution->SensorPreviewHeight);     
//    OV8850DB("SensorFullWidth:  %d.\n", pSensorResolution->SensorFullWidth);     
//    OV8850DB("SensorFullHeight: %d.\n", pSensorResolution->SensorFullHeight);    
    return ERROR_NONE;
}   /* OV8850GetResolution() */

UINT32 OV8850GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{	
	OV8850DB("OV8850GetInfo!!\n");
	//#if defined(MT6575)||defined(MT6577)
	#ifndef MT6573
		switch(OV8850CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				pSensorInfo->SensorPreviewResolutionX= OV8850_IMAGE_SENSOR_FULL_WIDTH;
    			pSensorInfo->SensorPreviewResolutionY= OV8850_IMAGE_SENSOR_FULL_HEIGHT;
				break;
			default:
				pSensorInfo->SensorPreviewResolutionX= OV8850_IMAGE_SENSOR_PV_WIDTH; 
    			pSensorInfo->SensorPreviewResolutionY= OV8850_IMAGE_SENSOR_PV_HEIGHT; 
    			break;
		}
	#else
	pSensorInfo->SensorPreviewResolutionX= OV8850_IMAGE_SENSOR_PV_WIDTH; 
    pSensorInfo->SensorPreviewResolutionY= OV8850_IMAGE_SENSOR_PV_HEIGHT; 
	#endif
	
	pSensorInfo->SensorFullResolutionX= OV8850_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= OV8850_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&ov8850mipiraw_drv_lock);
	ov8850.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&ov8850mipiraw_drv_lock);
	//OV8850DB("[OV8850GetInfo]SensorImageMirror:%d\n", ov8850.imgMirror );
	

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gb;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW; 
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    pSensorInfo->SensorDriver3D = 0;   // the sensor driver is 2D
    
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxWidth=CAM_SIZE_2M_WIDTH;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxHeight=CAM_SIZE_2M_HEIGHT;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].ISOSupported=TRUE;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].BinningEnable=FALSE;

    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxWidth=CAM_SIZE_2M_WIDTH;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxHeight=CAM_SIZE_2M_HEIGHT;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].ISOSupported=TRUE;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].BinningEnable=FALSE;

    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxWidth=CAM_SIZE_2M_WIDTH;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxHeight=CAM_SIZE_2M_HEIGHT;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].ISOSupported=FALSE;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].BinningEnable=FALSE;

    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxWidth=CAM_SIZE_05M_WIDTH;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxHeight=CAM_SIZE_1M_HEIGHT;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].ISOSupported=FALSE;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].BinningEnable=TRUE;

    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxWidth=CAM_SIZE_05M_WIDTH;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxHeight=CAM_SIZE_05M_HEIGHT;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].ISOSupported=FALSE;
    pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].BinningEnable=TRUE;

    pSensorInfo->CaptureDelayFrame = 3; //1; 
    pSensorInfo->PreviewDelayFrame = 3; //1; 
    pSensorInfo->VideoDelayFrame = 3; 

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;      
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0; //0 ;//0;     /* The frame of setting sensor gain */  //currently, sensor gain delay 1 frame latch
    pSensorInfo->AEISPGainDelayFrame = 1; //1;	
	   
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV8850_PV_X_START; 
            pSensorInfo->SensorGrabStartY = OV8850_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
		//#if defined(MT6575)||defined(MT6577)
		#ifndef MT6573
			case MSDK_SCENARIO_ID_CAMERA_ZSD:
		#endif
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = OV8850_FULL_X_START;	//2*OV8850_IMAGE_SENSOR_PV_STARTX; 
            pSensorInfo->SensorGrabStartY = OV8850_FULL_Y_START;	//2*OV8850_IMAGE_SENSOR_PV_STARTY;    
            
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = OV8850_PV_X_START; 
            pSensorInfo->SensorGrabStartY = OV8850_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;            
            break;
    }

    memcpy(pSensorConfigData, &OV8850SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* OV8850GetInfo() */


UINT32 OV8850Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		
		OV8850DB("OV8850Control!!\n");
		spin_lock(&ov8850mipiraw_drv_lock);
		OV8850CurrentScenarioId = ScenarioId;
		spin_unlock(&ov8850mipiraw_drv_lock);
		//OV8850DB("ScenarioId=%d\n",ScenarioId);
		OV8850DB("OV8850CurrentScenarioId=%d\n",OV8850CurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
            OV8850Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
		//#if defined(MT6575)||defined(MT6577)
		#ifndef MT6573
			case MSDK_SCENARIO_ID_CAMERA_ZSD:
		#endif
            OV8850Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return TRUE;
} /* OV8850Control() */


UINT32 OV8850SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MAX_Frame_length =0,frameRate=0,extralines=0;
    OV8850DB("[OV8850SetVideoMode] frame rate = %d\n", u2FrameRate);

	if(u2FrameRate==0)
	{
		OV8850DB("Disable Video Mode frame rate free run\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    OV8850DB("error frame rate seting\n");

    if(ov8850.sensorMode == SENSOR_MODE_PREVIEW)
    {
    	if(ov8850.OV8850AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30||u2FrameRate==24)
				frameRate= 296;
			else
				frameRate= 148;//148;
			
			MAX_Frame_length = (ov8850.pvPclk*10000)/(OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/frameRate*10;
    	}
		else
			MAX_Frame_length = (ov8850.pvPclk*10000) /(OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/u2FrameRate;
				
		if((MAX_Frame_length <=OV8850_PV_PERIOD_LINE_NUMS))
		{
			MAX_Frame_length = OV8850_PV_PERIOD_LINE_NUMS;
			OV8850DB("[OV8850SetVideoMode]current fps = %d\n", (ov8850.pvPclk*10000)  /(OV8850_PV_PERIOD_PIXEL_NUMS)/OV8850_PV_PERIOD_LINE_NUMS);	
		}
		OV8850DB("[OV8850SetVideoMode]current fps (10 base)= %d\n", (ov8850.pvPclk*10000)*10/(OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/MAX_Frame_length);
		extralines = MAX_Frame_length - OV8850_PV_PERIOD_LINE_NUMS;
		ov8850.DummyLines = extralines;
	
		OV8850_SetDummy(ov8850.DummyPixels,extralines);
    }
	else if(ov8850.sensorMode == SENSOR_MODE_CAPTURE)
	{
		OV8850DB("-------[OV8850SetVideoMode]ZSD???---------\n");
		if(ov8850.OV8850AutoFlickerMode == KAL_TRUE)
    	{
			#if defined(ZSD15FPS)
			frameRate= 148;//For ZSD	 mode//15fps
			#else
			frameRate= 130;//For ZSD	 mode	13fps
			#endif
			MAX_Frame_length = (ov8850.pvPclk*10000) /(OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/frameRate*10;
    	}
		else
			MAX_Frame_length = (ov8850.capPclk*10000) /(OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/u2FrameRate;
		
		if((MAX_Frame_length <=OV8850_FULL_PERIOD_LINE_NUMS))
		{
			MAX_Frame_length = OV8850_FULL_PERIOD_LINE_NUMS;
			OV8850DB("[OV8850SetVideoMode]current fps = %d\n", (ov8850.capPclk*10000) /(OV8850_FULL_PERIOD_PIXEL_NUMS)/OV8850_FULL_PERIOD_LINE_NUMS);
		
		}
		OV8850DB("[OV8850SetVideoMode]current fps (10 base)= %d\n", (ov8850.pvPclk*10000)*10/(OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels)/MAX_Frame_length);
		
		extralines = MAX_Frame_length - OV8850_FULL_PERIOD_LINE_NUMS;
	
		OV8850_SetDummy(ov8850.DummyPixels,extralines);
		ov8850.DummyLines = extralines;
	}
	OV8850DB("[OV8850SetVideoMode]MAX_Frame_length=%d,ov8850.DummyLines=%d\n",MAX_Frame_length,ov8850.DummyLines);
	
    return KAL_TRUE;
}

UINT32 OV8850SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	kal_uint32 MAX_Frame_length;
	//return TRUE;

    //OV8850DB("[OV8850SetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker   
		spin_lock(&ov8850mipiraw_drv_lock);
		ov8850.OV8850AutoFlickerMode = KAL_TRUE; 
		spin_unlock(&ov8850mipiraw_drv_lock);
    } else {
    	spin_lock(&ov8850mipiraw_drv_lock);
        ov8850.OV8850AutoFlickerMode = KAL_FALSE; 
		spin_unlock(&ov8850mipiraw_drv_lock);
        OV8850DB("Disable Auto flicker\n");    
    }

    return TRUE;
}

UINT32 OV8850SetTestPatternMode(kal_bool bEnable)
{
    OV8850DB("[OV8850SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	
    return TRUE;
}

UINT32 OV8850FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 SensorRegNumber;
    UINT32 i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= OV8850_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= OV8850_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				//#if defined(MT6575)||defined(MT6577)
				#ifndef MT6573
        			switch(OV8850CurrentScenarioId)
        			{
        				//OV8850DB("OV8850FeatureControl:OV8850CurrentScenarioId:%d\n",OV8850CurrentScenarioId);
        				case MSDK_SCENARIO_ID_CAMERA_ZSD:
							OV8850DB("Sensor period:MSDK_SCENARIO_ID_CAMERA_ZSD\n");
		            		*pFeatureReturnPara16++= OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;  
		            		*pFeatureReturnPara16= OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines;	
		            		OV8850DB("Sensor period:%d ,%d\n", OV8850_FULL_PERIOD_PIXEL_NUMS + ov8850.DummyPixels, OV8850_FULL_PERIOD_LINE_NUMS + ov8850.DummyLines); 
		            		*pFeatureParaLen=4;        				
        					break;
        			
        				default:	
		            		*pFeatureReturnPara16++= OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;  
		            		*pFeatureReturnPara16= OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines;
		            		OV8850DB("Sensor period:%d ,%d\n", OV8850_PV_PERIOD_PIXEL_NUMS  + ov8850.DummyPixels, OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines); 
		            		*pFeatureParaLen=4;
	            			break;
          				}
				#else
					*pFeatureReturnPara16++= OV8850_PV_PERIOD_PIXEL_NUMS + ov8850.DummyPixels;  
		            *pFeatureReturnPara16= OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines;		   
		            OV8850DB("Sensor period:%d ,%d\n", OV8850_PV_PERIOD_PIXEL_NUMS  + ov8850.DummyPixels, OV8850_PV_PERIOD_LINE_NUMS + ov8850.DummyLines); 
					*pFeatureParaLen=4;
				#endif
			
          	break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			//#if defined(MT6575)||defined(MT6577)
			#ifndef MT6573
        			switch(OV8850CurrentScenarioId)
        			{
        			//OV8850DB("OV8850FeatureControl:OV8850CurrentScenarioId:%d\n",OV8850CurrentScenarioId);
        				case MSDK_SCENARIO_ID_CAMERA_ZSD:
							OV8850DB("PIXEL_CLOCK_FREQ:MSDK_SCENARIO_ID_CAMERA_ZSD\n");
		            	 	//
		            	 	#if defined(ZSD15FPS)
		            	 	*pFeatureReturnPara32 = 138670000; //138670000; 
							#else
							*pFeatureReturnPara32 = 138670000; //138670000; 		            	 		  
							#endif
		            	 	*pFeatureParaLen=4;		         	
		         			 break;
		         		
		         		default:
		            		*pFeatureReturnPara32 = 208000000; //138670000; 
		            		*pFeatureParaLen=4;
		            		break;
		        	}
			#else
				*pFeatureReturnPara32 = 208000000; //138670000; 
		        *pFeatureParaLen=4;
			#endif
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            OV8850_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            OV8850_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            OV8850_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //OV8850_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            OV8850_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV8850_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov8850mipiraw_drv_lock);
                OV8850SensorCCT[i].Addr=*pFeatureData32++;
                OV8850SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&ov8850mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV8850SensorCCT[i].Addr;
                *pFeatureData32++=OV8850SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov8850mipiraw_drv_lock);
                OV8850SensorReg[i].Addr=*pFeatureData32++;
                OV8850SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&ov8850mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV8850SensorReg[i].Addr;
                *pFeatureData32++=OV8850SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=OV8850_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, OV8850SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, OV8850SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &OV8850SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            OV8850_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV8850_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=OV8850_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV8850_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV8850_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV8850_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_Gr;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_INITIALIZE_AF:
            break;
        case SENSOR_FEATURE_CONSTANT_AF:
            break;
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            OV8850SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV8850GetSensorID(pFeatureReturnPara32); 
            break;             
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV8850SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));            
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV8850SetTestPatternMode((BOOL)*pFeatureData16);        	
            break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* OV8850FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncOV8850=
{
    OV8850Open,
    OV8850GetInfo,
    OV8850GetResolution,
    OV8850FeatureControl,
    OV8850Control,
    OV8850Close
};

UINT32 OV8850_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV8850;

    return ERROR_NONE;
}   /* SensorInit() */

