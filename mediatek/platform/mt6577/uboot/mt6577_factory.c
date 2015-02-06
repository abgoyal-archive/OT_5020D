
#include <common.h>
#include <video.h>
#include <asm/io.h>
#include <asm/arch/mt65xx.h>
#include <asm/arch/mt65xx_typedefs.h>
#include <asm/arch/mtk_key.h>
#include <asm/arch/boot_mode.h>
#include <asm/arch/mt6577_factory.h>
#include <cust_kpd.h>	// tao.zhang@tcl.com modify 2013-02-18.  use the same config with mt6577_recovery.c.

#define MODULE_NAME "[FACTORY]"


BOOL factory_check_key_trigger(void)
{
	//wait
	ulong begin = get_timer(0);
	printf("\n%s Check factory boot\n",MODULE_NAME);
	printf("%s Wait 50ms for special keys\n",MODULE_NAME);
	
	/* If the boot reason is RESET, than we will NOT enter factory mode. */
	if(mt6577_detect_pmic_just_rst())
	{
	  return false;
	}
	
    while(get_timer(begin)<50)
    {    
		if(mt6577_detect_key(MT65XX_FACTORY_KEY))
		{	
			printf("%s Detect key\n",MODULE_NAME);
			printf("%s Enable factory mode\n",MODULE_NAME);		
			g_boot_mode = FACTORY_BOOT;
			//video_printf("%s : detect factory mode !\n",MODULE_NAME);
			return TRUE;
		}
	}
		
	return FALSE;		
}

extern BOOT_ARGUMENT *g_boot_arg;
BOOL factory_detection(void)
{
    int forbid_mode = 0;
    
    if(g_boot_arg->sec_limit.magic_num == 0x4C4C4C4C)
    {
        if(g_boot_arg->sec_limit.forbid_mode == F_FACTORY_MODE)
        {
            //Forbid to enter factory mode
            printf("%s Forbidden\n",MODULE_NAME);
            return FALSE;
        }
    }

    forbid_mode = g_boot_arg->boot_mode &= 0x000000FF;

    if(factory_check_key_trigger())
    {
    	return TRUE;
    }
	
	return FALSE;
}


