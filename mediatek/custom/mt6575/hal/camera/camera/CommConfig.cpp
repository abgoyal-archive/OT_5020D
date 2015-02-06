
#define LOG_TAG "NSCamCustom/Comm"
//
#include <camera_custom_types.h>
//

namespace NSCamCustom
{
MUINT32
getMinBurstShotToShotTime()
{
    return 350000;
}



MBOOL
isSupportJpegOrientation()
{
    return MTRUE;
}


};  //NSCamCustom

