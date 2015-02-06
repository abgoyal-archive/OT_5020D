

#ifndef _AF_TUNING_CUSTOM_MT6573_H
#define _AF_TUNING_CUSTOM_MT6573_H

#include "../param/aaa_param_mt6573.h"

//____AF Algorithm Parameters____
static AF_PARAM_T g_rAFParam =
{
    15
}; 

//____AF Statistics Config____
static AF_STAT_CONFIG_T g_rAFStatConfig = 
{
	TRUE,	// enable AF
	TRUE,	// 0:SMD [0 1 -1], 1:TEN [1 2 1]

	{{
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},		
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0},
		{ 0,  0,  0,  0}	
	}},
	
	{7, 5, 3, 2, 8}
};

#endif

