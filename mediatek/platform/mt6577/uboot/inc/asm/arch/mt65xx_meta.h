
#ifndef __MT65XX_META_H
#define __MT65XX_META_H

#include <common.h>
#include <asm/arch/mt65xx_typedefs.h>

#define META_BOOTARGS  " init=sbin/meta_init rdinit=sbin/meta_init"
#define ADV_META_BOOTARGS  " init=sbin/advanced_meta_init rdinit=sbin/advanced_meta_init"
#define CONFIG_META_BAUDRATE	 115200

extern BOOL meta_detection(void);

#endif /* __MT65XX_META_H */

