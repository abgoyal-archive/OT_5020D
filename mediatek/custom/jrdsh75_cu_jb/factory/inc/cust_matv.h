
#ifndef CUST_MATV_H
#define CUST_MATV_H

#include "kal_release.h"
#include "matvctrl.h"


///#define ANALOG_AUDIO

#if 1
#define MATV_TOATL_CH  0x06

//typedef struct 
//{
//	kal_uint32	freq; //khz
//	kal_uint8	sndsys;	/* reference sv_const.h, TV_AUD_SYS_T ...*/
//	kal_uint8	colsys;	/* reference sv_const.h, SV_CS_PAL_N, SV_CS_PAL,SV_CS_NTSC358...*/
//	kal_uint8	flag;
//} matv_ch_entry;
matv_ch_entry MATV_CH_TABLE[]=
{
    //China 4/5/10/12/44/47
    {77250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},
    {85250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},
    {200250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},
    {216250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},    
    {759250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},
    {783250, SV_PAL_DK_FMMONO, SV_CS_PAL , 1},
    {-1, NULL, NULL, NULL}
};
#else
#define MATV_TOATL_CH  0x03

//typedef struct 
//{
//	kal_uint32	freq; //khz
//	kal_uint8	sndsys;	/* reference sv_const.h, TV_AUD_SYS_T ...*/
//	kal_uint8	colsys;	/* reference sv_const.h, SV_CS_PAL_N, SV_CS_PAL,SV_CS_NTSC358...*/
//	kal_uint8	flag;
//} matv_ch_entry;
matv_ch_entry MATV_CH_TABLE[]=
{
    //Taiwan Ch42/44/46
    {639250, SV_MTS, SV_CS_NTSC358 , 1},
    {651250, SV_MTS, SV_CS_NTSC358 , 1},
    {663250, SV_MTS, SV_CS_NTSC358 , 1},      
    {-1, NULL, NULL, NULL}
};
#endif
#endif /* CUST_FM_H */

