/**
 * \file phNfc_test.c
 * \brief 
 *
 * Project: Trusted NFC Linux
 *
 */
/* Modifications on Features Request / Changes Request / Problems Report       */
/*=============================================================================*/
/* date    | author          | Key                      | comment              */
/*=========|=================|==========================|======================*/

#include <utils/Log.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <phDal4Nfc_debug.h>
#include <phDal4Nfc_i2c.h>
#include <phNfc_test.h>
#include <phOsalNfc.h>
#include <phNfcStatus.h>
#if defined(ANDROID)
#include <string.h>
#endif

#include <linux/pn544.h>


/*-----------------------------------------------------------------------------

FUNCTION: phNfc_test_rf_on

PURPOSE:  

-----------------------------------------------------------------------------*/
int phNfc_test_rf_on(void)
{
   int ret = NFCSTATUS_SUCCESS;   

   ret=phDal4Nfc_i2c_rf_on();

   ALOGE("phNfc_test_rf_on ret = %d",ret);

   return ret;
}

void phNfc_test_typeAReader_on(void){

	phDal4Nfc_i2c_typeAReader_on();

	ALOGE("phNfc_test_typeAReader_on");
}


void phNfc_test_ResetDetectedTagCount_on(int flag){

	phDal4Nfc_i2c_resetDetectedTagCount_on(flag);

	ALOGE("phNfc_test_ResetDetectedTagCount_on");
}

int phNfc_test_GetDetectedTagCount_on(void){

	return phDal4Nfc_i2c_getDetectedTagCount_on();

	ALOGE("phNfc_test_GetDetectedTagCount_on");
}




