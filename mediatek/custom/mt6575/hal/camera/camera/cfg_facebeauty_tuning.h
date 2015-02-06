
#ifndef _CFG_FACEBEAUTY_TUNING_H_
#define _CFG_FACEBEAUTY_TUNING_H_

#include <cutils/properties.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <utils/Errors.h>	// For errno.
#include <cutils/xlog.h>	// For XLOG?().



MINT8 get_FB_BlurLevel()
{
	MINT8 BlurLevel = 4;
	return BlurLevel;
}


MINT8 get_FB_NRTime()
{
	MINT8 NRTime = 4;
	return NRTime;
}

MINT8 get_FB_ColorTarget()
{
    MINT8 ColorTarget = 2;
    return ColorTarget;
}

#endif //  _CFG_FACEBEAUTY_TUNING_H_

