

#ifndef _APP_AF_FACTOR_H
#define _APP_AF_FACTOR_H

#include "MTK3ACommon.h"

//____AF Algorithm Parameters____
static AF_PARAM_T g_rAFParam =
{

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
		{ 0,  0,  0,  0}	
	}},
	
	{2, 3, 5, 7, 8}
};

#endif

