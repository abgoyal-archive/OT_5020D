
#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_sleep.h>

#define WPLL197_FREQ 197000 // 3G PLL frequency is 197Mhz
#define CLKSQ_FREQ   26000  // Default frequency is 26Mhz

unsigned int mt6575_get_bus_freq(void)
{
    unsigned int fout = 0, fbdiv = 0, fbsel = 0, prediv = 0, postdiv = 0, ckdiv = 0;

    if ((DRV_Reg32(TOP_CKMUXSEL) & 0x3) == 0x0) // Using CLKSQ
    {
        fout = CLKSQ_FREQ;
    }
    else if ((DRV_Reg32(TOP_CKMUXSEL) & 0x3) == 0x2) // Using WPLL
    {
        fout = WPLL197_FREQ;
    }
    else
    {
        fbdiv = (DRV_Reg32(MAINPLL_CON0) & 0x7F00) >> 8;

        fbsel = (DRV_Reg32(MAINPLL_CON0) & 0x0030) >> 4;
        if (fbsel == 0)
            fbsel = 1;
        else if (fbsel == 1)
            fbsel = 2;
        else
            fbsel = 4;

        prediv = (DRV_Reg32(MAINPLL_CON0) & 0x00C0) >> 6;
        if (prediv == 0)
            prediv = 1;
        else if (prediv == 1)
            prediv = 2;
        else
            prediv = 4;

        postdiv = (DRV_Reg32(MAINPLL_CON1) & 0x0030) >> 4;
        if (postdiv == 0)
            postdiv = 1;
        else if (postdiv == 1)
            postdiv = 2;
        else
            postdiv = 4;

        fout = CLKSQ_FREQ * (fbdiv + 1) * fbsel / prediv / postdiv; // KHz
    }

    ckdiv = (DRV_Reg32(TOP_CKDIV0) & 0x00000018) >> 3;

    switch (ckdiv)
    {
        case 0:
            fout = fout;     // bus clock will not be divided
            break;
        case 1:
            fout = fout / 4; // bus clock will be divided by 4
            break;
        case 2:
            fout = fout / 5; // bus clock will be divided by 5
            break;
        case 3:
            fout = fout / 6; // bus clock will be divided by 6
            break;
        default:
            break; 
    }

    return fout;
}