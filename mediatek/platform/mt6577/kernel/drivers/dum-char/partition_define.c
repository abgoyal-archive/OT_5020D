#include <linux/module.h>
#include "partition_define.h"
struct excel_info PartInfo[PART_NUM]={
			{"preloader",262144,0x0, EMMC, 0,BOOT_1},
			{"dsp_bl",15466496,0x40000, EMMC, 0,BOOT_1},
			{"mbr",16384,0xf00000, EMMC, 0,USER},
			{"ebr1",16384,0xf04000, EMMC, 1,USER},
			{"pmt",4194304,0xf08000, EMMC, 0,USER},
			{"nvram",3145728,0x1308000, EMMC, 0,USER},
			{"seccfg",131072,0x1608000, EMMC, 0,USER},
			{"uboot",393216,0x1628000, EMMC, 0,USER},
			{"bootimg",6291456,0x1688000, EMMC, 0,USER},
			{"recovery",6291456,0x1c88000, EMMC, 0,USER},
			{"sec_ro",6291456,0x2288000, EMMC, 2,USER},
			{"misc",393216,0x2888000, EMMC, 0,USER},
			{"logo",3145728,0x28e8000, EMMC, 0,USER},
			{"expdb",999424,0x2be8000, EMMC, 0,USER},
			{"pro_info",524288,0x2cdc000, EMMC, 0,USER},
			{"custpack",433061888,0x2d5c000, EMMC, 3,USER},
			{"mobile_info",8388608,0x1ca5c000, EMMC, 4,USER},
			{"android",433061888,0x1d25c000, EMMC, 5,USER},
			{"cache",328204288,0x36f5c000, EMMC, 6,USER},
			{"usrdata",2588934144,0x4a85c000, EMMC, 7,USER},
			{"otp",45088768,0xFFFF0200, EMMC, 0,USER},
			{"bmtpool",22020096,0xFFFF00a8, EMMC, 0,USER},
 };
EXPORT_SYMBOL(PartInfo);

#ifdef  MTK_EMMC_SUPPORT
struct MBR_EBR_struct MBR_EBR_px[MBR_COUNT]={
	{"mbr", {1, 2, 3, 4, }},
	{"ebr1", {5, 6, 7, }},
};

EXPORT_SYMBOL(MBR_EBR_px);
#endif

