/*
 * Copyright (C) 2010 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file phDalNfc_i2c.c
 * \brief DAL I2C port implementation for linux
 *
 * Project: Trusted NFC Linux
 *
 */

#define LOG_TAG "NFC_i2c"
#include <cutils/log.h>
#include <hardware/nfc.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>

#include <phDal4Nfc_debug.h>
#include <phDal4Nfc_i2c.h>
#include <phOsalNfc.h>
#include <phNfcStatus.h>
#if defined(ANDROID)
#include <string.h>
#endif

#include <linux/pn544.h>

typedef struct
{
   int  nHandle;
   char nOpened;

} phDal4Nfc_I2cPortContext_t;


/*-----------------------------------------------------------------------------------
                                      VARIABLES
------------------------------------------------------------------------------------*/
static phDal4Nfc_I2cPortContext_t gI2cPortContext;

static int mDetectedTagCount = 0;

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_set_open_from_handle

PURPOSE:  Initialize internal variables

-----------------------------------------------------------------------------*/

void phDal4Nfc_i2c_initialize(void)
{
   memset(&gI2cPortContext, 0, sizeof(phDal4Nfc_I2cPortContext_t));
}


/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_set_open_from_handle

PURPOSE:  The application could have opened the link itself. So we just need
          to get the handle and consider that the open operation has already
          been done.

-----------------------------------------------------------------------------*/

void phDal4Nfc_i2c_set_open_from_handle(phHal_sHwReference_t * pDalHwContext)
{
   gI2cPortContext.nHandle = (int) pDalHwContext->p_board_driver;
   DAL_ASSERT_STR(gI2cPortContext.nHandle >= 0, "Bad passed com port handle");
   gI2cPortContext.nOpened = 1;
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_is_opened

PURPOSE:  Returns if the link is opened or not. (0 = not opened; 1 = opened)

-----------------------------------------------------------------------------*/

int phDal4Nfc_i2c_is_opened(void)
{
   return gI2cPortContext.nOpened;
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_flush

PURPOSE:  Flushes the link ; clears the link buffers

-----------------------------------------------------------------------------*/

void phDal4Nfc_i2c_flush(void)
{
   /* Nothing to do (driver has no internal buffers) */
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_close

PURPOSE:  Closes the link

-----------------------------------------------------------------------------*/

void phDal4Nfc_i2c_close(void)
{
   DAL_PRINT("Closing port\n");
   if (gI2cPortContext.nOpened == 1)
   {
      close(gI2cPortContext.nHandle);
      gI2cPortContext.nHandle = 0;
      gI2cPortContext.nOpened = 0;
   }
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_open_and_configure

PURPOSE:  Closes the link

-----------------------------------------------------------------------------*/

NFCSTATUS phDal4Nfc_i2c_open_and_configure(pphDal4Nfc_sConfig_t pConfig, void ** pLinkHandle)
{
   DAL_ASSERT_STR(gI2cPortContext.nOpened==0, "Trying to open but already done!");

   DAL_DEBUG("Opening port=%s\n", pConfig->deviceNode);

   /* open port */
   gI2cPortContext.nHandle = open(pConfig->deviceNode, O_RDWR | O_NOCTTY);
   if (gI2cPortContext.nHandle < 0)
   {
       DAL_DEBUG("Open failed: open() returned %d\n", gI2cPortContext.nHandle);
      *pLinkHandle = NULL;
      return PHNFCSTVAL(CID_NFC_DAL, NFCSTATUS_INVALID_DEVICE);
   }

   gI2cPortContext.nOpened = 1;
   *pLinkHandle = (void*)gI2cPortContext.nHandle;

   DAL_PRINT("Open succeed\n");

   return NFCSTATUS_SUCCESS;
}


/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_read

PURPOSE:  Reads nNbBytesToRead bytes and writes them in pBuffer.
          Returns the number of bytes really read or -1 in case of error.

-----------------------------------------------------------------------------*/

int phDal4Nfc_i2c_read(uint8_t * pBuffer, int nNbBytesToRead)
{
    int ret;
    int numRead = 0;
    struct timeval tv;
    fd_set rfds;

    DAL_ASSERT_STR(gI2cPortContext.nOpened == 1, "read called but not opened!");
    DAL_DEBUG("_i2c_read() called to read %d bytes", nNbBytesToRead);

    // Read with 2 second timeout, so that the read thread can be aborted
    // when the pn544 does not respond and we need to switch to FW download
    // mode. This should be done via a control socket instead.
    while (numRead < nNbBytesToRead) {
        FD_ZERO(&rfds);
        FD_SET(gI2cPortContext.nHandle, &rfds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        ret = select(gI2cPortContext.nHandle + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            DAL_DEBUG("select() errno=%d", errno);
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return -1;
        } else if (ret == 0) {
            DAL_PRINT("timeout!");
            return -1;
        }
        ret = read(gI2cPortContext.nHandle, pBuffer + numRead, nNbBytesToRead - numRead);
        if (ret > 0) {
            DAL_DEBUG("read %d bytes", ret);
            numRead += ret;
        } else if (ret == 0) {
            DAL_PRINT("_i2c_read() EOF");
            return -1;
        } else {
            DAL_DEBUG("_i2c_read() errno=%d", errno);
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return -1;
        }
    }
    return numRead;
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_write

PURPOSE:  Writes nNbBytesToWrite bytes from pBuffer to the link
          Returns the number of bytes that have been wrote to the interface or -1 in case of error.

-----------------------------------------------------------------------------*/

int phDal4Nfc_i2c_write(uint8_t * pBuffer, int nNbBytesToWrite)
{
    int ret;
    int numWrote = 0;

    DAL_ASSERT_STR(gI2cPortContext.nOpened == 1, "write called but not opened!");
    DAL_DEBUG("_i2c_write() called to write %d bytes\n", nNbBytesToWrite);

    while (numWrote < nNbBytesToWrite) {
        ret = write(gI2cPortContext.nHandle, pBuffer + numWrote, nNbBytesToWrite - numWrote);
        if (ret > 0) {
            DAL_DEBUG("wrote %d bytes", ret);
            numWrote += ret;
        } else if (ret == 0) {
            DAL_PRINT("_i2c_write() EOF");
            return -1;
        } else {
            DAL_DEBUG("_i2c_write() errno=%d", errno);
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return -1;
        }
    }

    return numWrote;
}

/*-----------------------------------------------------------------------------

FUNCTION: phDal4Nfc_i2c_reset

PURPOSE:  Reset the PN544, using the VEN pin

-----------------------------------------------------------------------------*/
int phDal4Nfc_i2c_reset(long level)
{
    DAL_DEBUG("phDal4Nfc_i2c_reset, VEN level = %ld", level);

    return ioctl(gI2cPortContext.nHandle, PN544_SET_PWR, level);
}

/*-----------------------------------------------------------------------------

MiniSW and EngineeringMode Interface

-----------------------------------------------------------------------------*/

#define READ_RERY  10

static void HW_RESET(int fd)
{
	unsigned int i;

	usleep(100000);
	i=0;
	ioctl(fd,PN544_SET_PWR,&i);
	usleep(50000);
	i=1;
	ioctl(fd,PN544_SET_PWR,&i);
	usleep(300000);
}


static void updatecrc(unsigned char    crcByte, unsigned short    *pCrc)
{
    crcByte = (crcByte ^ (unsigned char)((*pCrc) & 0x00FF));
    crcByte = (crcByte ^ (unsigned char)(crcByte << 4));
    *pCrc = (*pCrc >> 8) ^ ((unsigned short)crcByte << 8) ^ ((unsigned short)crcByte << 3) ^ ((unsigned short)crcByte >> 4);
}

static int CHECK_CRC(unsigned char    *pData)
{
    unsigned char    crc_byte = 0,
                           index = 0;
    unsigned short    crc = 0;

    unsigned char length,Crc1,Crc2;

    length = pData[0]-1;

#if 0

    crc = 0x6363; /* ITU-V.41 */
#else

    crc = 0xFFFF; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */
#endif /* #ifdef CRC_A */

    do
    {
        crc_byte = pData[index];
        updatecrc(crc_byte, &crc);
        index++;
    }
    while (index < length);

#if 1

    crc = ~crc; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */
#endif /* #ifndef INVERT_CRC */

    Crc1 = (unsigned char) (crc & 0xFF);
    Crc2 = (unsigned char) ((crc >> 8) & 0xFF);

    if((Crc1==pData[length])&&(Crc2==pData[1+length]))
    	return 0;

    ALOGE("\n CRC ERROR!");
    return -1;
}

static int UPDATE_CRC(unsigned char    *pData)
{
    unsigned char    crc_byte = 0,
                           index = 0;
    unsigned short    crc = 0;

    unsigned char length,Crc1,Crc2;

    length = pData[0]-1;

#if 0

    crc = 0x6363; /* ITU-V.41 */
#else

    crc = 0xFFFF; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */
#endif /* #ifdef CRC_A */

    do
    {
        crc_byte = pData[index];
        updatecrc(crc_byte, &crc);
        index++;
    }
    while (index < length);

#if 1

    crc = ~crc; /* ISO/IEC 13239 (formerly ISO/IEC 3309) */
#endif /* #ifndef INVERT_CRC */

    Crc1 = (unsigned char) (crc & 0xFF);
    Crc2 = (unsigned char) ((crc >> 8) & 0xFF);

    pData[length]=Crc1;
    pData[length+1]=Crc2;

    return 0;
}


static int BB2PNXXX(int fd, unsigned char * s)
{

	unsigned char i,k;
	if(CHECK_CRC(s)){
		ALOGE("\n CHECK_CRC s FAILED!");
		return -1;
	}

	k=write(fd,s,s[0]+1);
	if(k!=(s[0]+1))
	{
		ALOGE("\n WRITE FAILED!");
		return -1;
	}	

	ALOGE("\n BB->PNXXX: ");	
	for(i=0;i<s[0]+1;i++)
		ALOGE(" %02x",s[i]);

	return 0;
}

static int PNXXX2BB(int fd, unsigned char * s)
{
	unsigned char i,k,j[64]={0x00};

	if(CHECK_CRC(s))
		return -1;
		
	j[0]=0xFF;i=1;
	while((j[0]<0)||(j[0]>100))
	{
		if(i++ > READ_RERY)
		{
			ALOGE("\n MAYBE INT ERROR!");
			return -1;
		}

		k=read(fd,j,1);
	}
	k=read(fd,&(j[1]),j[0]);
	if(k!=j[0])
	{
		ALOGE("\n READ FAILED!");
		return -1;
	}
	ALOGE("\n BB<-PNXXX: ");	
	for(i=0;i<j[0]+1;i++)
	{
		if(s[i]==j[i])
			ALOGE(" %02x",j[i]);
		else
         {
                     ALOGE(" %02x",j[i]);
			ALOGE("\n MAYBE READ RESULT IS NOT WANTED!");
         }
	}
	return 0;
}


static int PNXXX2BB_CHECK(int fd, unsigned char * s)
{
	unsigned char i,k,j[64]={0x00};
       k=read(fd,j,1);
        if(k!=1)
         return -1;
	k=read(fd,&(j[1]),j[0]);
	if(k!=j[0])
	{
		ALOGE("\n READ FAILED!");
		return -1;
	}
	ALOGE("\n BB<-PNXXX: ");	
	for(i=0;i<j[0]+1;i++)
	{
		if(s[i]==j[i])
			ALOGE(" %02x",j[i]);
		else
         {
                     ALOGE(" %02x",j[i]);
			ALOGE("\n MAYBE READ RESULT IS NOT WANTED!");
                     return -1;
         }
	}
	return 0;
}

static int PNXXX2BB_DATA(int fd, unsigned char * s)
{
	unsigned char i,k,j[64]={0x00};

	j[0]=0xFF;i=1;
	while((j[0]<0)||(j[0]>100))
	{
		if(i++ > READ_RERY)
		{
			ALOGE("\n MAYBE INT ERROR!");
			return -1;
		}

		usleep(100000);
		k=read(fd,j,1);
	}
	k=read(fd,&(j[1]),j[0]);
	if(k!=j[0])
	{
		ALOGE("\n READ FAILED!");
		return -1;
	}
	ALOGE("\n BB<-PNXXX: ");	
	for(i=0;i<j[0]+1;i++)
	{
		s[i]=j[i];
		ALOGE(" %02x",j[i]);
	}

	if(CHECK_CRC(s))
		return -1;

	return 0;
}

/*
	PN544 Test - Switch ON 13,56MHz carrier.cmd
*/
int phDal4Nfc_i2c_rf_on(void)
{
	int fd = open("/dev/pn544", O_RDWR | O_NOCTTY);
	ALOGE("fd = %d",fd);
	//HW_RESET(fd);
	ioctl(fd, PN544_SET_PWR, 0);
	usleep(50000);
	ioctl(fd, PN544_SET_PWR, 1);
	usleep(100000);


	unsigned char bb1[]={0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5 };
	if(BB2PNXXX(fd,bb1))
		goto rf_on_failed;

	unsigned char pp1[]={0x03, 0xE6, 0x17, 0xA7 };
	if(PNXXX2BB(fd,pp1))
		goto rf_on_failed;

	//* OPEN STATIC PIPE TO ADMINISTRATION GATE
	unsigned char bb2[]={0x05, 0x80, 0x81, 0x03, 0xEA, 0x39 };
	if(BB2PNXXX(fd,bb2))
		goto rf_on_failed;

	unsigned char pp2[]={0x05, 0x81, 0x81, 0x80, 0xA5, 0xD5 };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp2))
		goto rf_on_failed;

	unsigned char bb3[]={0x03, 0xC1, 0xAA, 0xF2 };
	if(BB2PNXXX(fd,bb3))
		goto rf_on_failed;

	//* Clear all pipe
	unsigned char bb4[]={0x05, 0x89, 0x81, 0x14, 0xCA, 0xC1}; 
	if(BB2PNXXX(fd,bb4))
		goto rf_on_failed;

	unsigned char pp3[]={0x03, 0xC2, 0x31, 0xC0 };
	if(PNXXX2BB(fd,pp3))
		goto rf_on_failed;

	unsigned char pp4[]={0x05, 0x8A, 0x81, 0x80, 0x03, 0xFC };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp4))
		goto rf_on_failed;

	unsigned char bb5[]={0x03, 0xC2, 0x31, 0xC0 };
	if(BB2PNXXX(fd,bb5))
		goto rf_on_failed;

	//* Re-open pipe ADMIN Gate
	unsigned char bb6[]={0x05, 0x92, 0x81, 0x03, 0xC7, 0x09 };
	if(BB2PNXXX(fd,bb6))
		goto rf_on_failed;

	unsigned char pp5[]={0x05, 0x93, 0x81, 0x80, 0x88, 0xE5 };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp5))
		goto rf_on_failed;

	unsigned char bb7[]={0x03, 0xC3, 0xB8, 0xD1 };
	if(BB2PNXXX(fd,bb7))
		goto rf_on_failed;

	//* CREATE PIPE TO THE GATE System MNGMT

	unsigned char bb8[]={0x08, 0x9B, 0x81, 0x10, 0x20, 0x00, 0x90, 0xA9, 0xED };
	if(BB2PNXXX(fd,bb8))
		goto rf_on_failed;

	unsigned char pp6[]={0x0A, 0x9C, 0x81, 0x80, 0x01, 0x20, 0x00, 0x90, 0x02, 0x1D, 0x5C };
	//	Unpacked data = [81 80 01 20 00 90 02 ]
	if(PNXXX2BB(fd,pp6))
		goto rf_on_failed;

	unsigned char bb9[]={0x03, 0xC4, 0x07, 0xA5 };
	if(BB2PNXXX(fd,bb9))
		goto rf_on_failed;

	//* OPEN PIPE TO THE GATE System MNGMT

	unsigned char bb10[]={0x05, 0xA4, 0x82, 0x03, 0xD8, 0x73 };
	if(BB2PNXXX(fd,bb10))
		goto rf_on_failed;

	unsigned char pp7[]={0x03, 0xC5, 0x8E, 0xB4 };
	if(PNXXX2BB(fd,pp7))
		goto rf_on_failed;

	unsigned char pp8[]={0x05, 0xA5, 0x82, 0x80, 0x97, 0x9F };
	//	Unpacked data = [82 80 ]
	if(PNXXX2BB(fd,pp8))
		goto rf_on_failed;

	unsigned char bb11[]={0x03, 0xC5, 0x8E, 0xB4 };
	if(BB2PNXXX(fd,bb11))
		goto rf_on_failed;

	//* ---------------------------------------

	//* Test needs to be terminated by resetting the chip
	//* RF carrier 13,56MHz switched ON

	unsigned char bb12[]={0x07, 0xAD, 0x82, 0x25, 0x03, 0x00, 0x18, 0x79 };
	if(BB2PNXXX(fd,bb12))
		goto rf_on_failed;

	unsigned char pp9[]={0x05, 0xAE, 0x82, 0x80, 0x31, 0xB6 };
	//	Unpacked data = [82 80 ]
	if(PNXXX2BB(fd,pp9))
		goto rf_on_failed;

	unsigned char bb13[]={0x03, 0xC6, 0x15, 0x86 };
	if(BB2PNXXX(fd,bb13))
		goto rf_on_failed;

       close(fd);
	return 0;

rf_on_failed:
	HW_RESET(fd);
	close(fd);
	return -1;	
}

int phDal4Nfc_i2c_getDetectedTagCount_on(void){
	return mDetectedTagCount;
}

void phDal4Nfc_i2c_resetDetectedTagCount_on(int flag){
       int fd = open("/dev/pn544", O_RDWR | O_NOCTTY);
       if(flag == 1)
        {
	mDetectedTagCount = 0 ;
       ioctl(fd, PN544_SET_PWR, 0);
	usleep(500);
	ioctl(fd, PN544_SET_PWR, 1);
	usleep(500);
       close(fd);
        }
        else
        {
       ioctl(fd, PN544_SET_PWR, 1);
	usleep(500);
	ioctl(fd, PN544_SET_PWR, 0);
	usleep(500);
       close(fd);
        };

}


/*
	PN544 Test - Switch ON only Type A Reader
*/
void phDal4Nfc_i2c_typeAReader_on(void)
{
	int fd = open("/dev/pn544", O_RDWR | O_NOCTTY);
	ALOGE("fd = %d",fd);
	//HW_RESET(fd);


	unsigned char bb1[]={0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5 };
	if(BB2PNXXX(fd,bb1))
		goto typeA_on_failed;

	unsigned char pp1[]={0x03, 0xE6, 0x17, 0xA7 };
	if(PNXXX2BB(fd,pp1))
		goto typeA_on_failed;

	//* OPEN STATIC PIPE TO ADMINISTRATION GATE
	unsigned char bb2[]={0x05, 0x80, 0x81, 0x03, 0xEA, 0x39 };
	if(BB2PNXXX(fd,bb2))
		goto typeA_on_failed;

	unsigned char pp2[]={0x05, 0x81, 0x81, 0x80, 0xA5, 0xD5 };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp2))
		goto typeA_on_failed;

	unsigned char bb3[]={0x03, 0xC1, 0xAA, 0xF2 };
	if(BB2PNXXX(fd,bb3))
		goto typeA_on_failed;

	//* Clear all pipe
	unsigned char bb4[]={0x05, 0x89, 0x81, 0x14, 0xCA, 0xC1};
	if(BB2PNXXX(fd,bb4))
		goto typeA_on_failed;

	unsigned char pp3[]={0x03, 0xC2, 0x31, 0xC0 };
	if(PNXXX2BB(fd,pp3))
		goto typeA_on_failed;

	unsigned char pp4[]={0x05, 0x8A, 0x81, 0x80, 0x03, 0xFC };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp4))
		goto typeA_on_failed;

	unsigned char bb5[]={0x03, 0xC2, 0x31, 0xC0 };
	if(BB2PNXXX(fd,bb5))
		goto typeA_on_failed;

	//* Re-open pipe ADMIN Gate
	unsigned char bb6[]={0x05, 0x92, 0x81, 0x03, 0xC7, 0x09 };
	if(BB2PNXXX(fd,bb6))
		goto typeA_on_failed;

	unsigned char pp5[]={0x05, 0x93, 0x81, 0x80, 0x88, 0xE5 };
	//	Unpacked data = [81 80 ]
	if(PNXXX2BB(fd,pp5))
		goto typeA_on_failed;

	unsigned char bb7[]={0x03, 0xC3, 0xB8, 0xD1 };
	if(BB2PNXXX(fd,bb7))
		goto typeA_on_failed;

	//* CREATE PIPE TO THE GATE System MNGMT

	unsigned char bb8[]={0x08, 0x9B, 0x81, 0x10, 0x20, 0x00, 0x94, 0x8D, 0xAB };
	if(BB2PNXXX(fd,bb8))
		goto typeA_on_failed;

	unsigned char pp6[]={0x0A, 0x9C, 0x81, 0x80, 0x01, 0x20, 0x00, 0x94, 0x02, 0x7D, 0x3B };
	//	Unpacked data = [81 80 01 20 00 90 02 ]
	if(PNXXX2BB(fd,pp6))
		goto typeA_on_failed;

	unsigned char bb9[]={0x03, 0xC4, 0x07, 0xA5 };
	if(BB2PNXXX(fd,bb9))
		goto typeA_on_failed;

	//* OPEN PIPE TO THE GATE System MNGMT

	unsigned char bb10[]={0x05, 0xA4, 0x82, 0x03, 0xD8, 0x73 };
	if(BB2PNXXX(fd,bb10))
		goto typeA_on_failed;

	unsigned char pp7[]={0x03, 0xC5, 0x8E, 0xB4 };
	if(PNXXX2BB(fd,pp7))
		goto typeA_on_failed;

	unsigned char pp8[]={0x05, 0xA5, 0x82, 0x80, 0x97, 0x9F };
	//	Unpacked data = [82 80 ]
	if(PNXXX2BB(fd,pp8))
		goto typeA_on_failed;

	unsigned char bb11[]={0x03, 0xC5, 0x8E, 0xB4 };
	if(BB2PNXXX(fd,bb11))
		goto typeA_on_failed;

	//* ---------------------------------------

	//* Test needs to be terminated by resetting the chip
	//* RF carrier 13,56MHz switched ON

	unsigned char bb12[]={0x07, 0xAD, 0x82, 0x01, 0x06, 0x7F, 0x8A, 0xEC };
	if(BB2PNXXX(fd,bb12))
		goto typeA_on_failed;

	unsigned char pp9[]={0x05, 0xAE, 0x82, 0x80, 0x31, 0xB6 };
	//	Unpacked data = [82 80 ]
	if(PNXXX2BB(fd,pp9))
		goto typeA_on_failed;

	unsigned char bb13[]={0x03, 0xC6, 0x15, 0x86 };
	if(BB2PNXXX(fd,bb13))
		goto typeA_on_failed;

	unsigned char bb14[]={0x06, 0xB6, 0x82, 0x02, 0x06, 0x3B, 0x31 };
	if(BB2PNXXX(fd,bb14))
		goto typeA_on_failed;

	unsigned char pp10[]={0x06, 0xB7, 0x82, 0x80, 0x7F, 0xBA, 0x7C };
	//	Unpacked data = [82 80 7F]
	if(PNXXX2BB(fd,pp10))
		goto typeA_on_failed;

	unsigned char bb15[]={0x03, 0xC7, 0x9C, 0x97 };
	if(BB2PNXXX(fd,bb15))
		goto typeA_on_failed;

	unsigned char bb16[]={0x08, 0xBF, 0x81, 0x10, 0x13, 0x00, 0x13, 0x3C, 0XA7 };
	if(BB2PNXXX(fd,bb16))
		goto typeA_on_failed;

	unsigned char bb170[]={0x03, 0xC0, 0x23, 0xE3 };
	if(PNXXX2BB(fd,bb170))
		goto typeA_on_failed;

	unsigned char pp11[]={0x0A, 0xB8, 0x81, 0x80, 0x01, 0x13, 0x00, 0x13, 0x03, 0x21, 0x22 };
	if(PNXXX2BB(fd,pp11))
		goto typeA_on_failed;

	unsigned char bb17[]={0x03, 0xC0, 0x23, 0xE3 };
	if(BB2PNXXX(fd,bb17))
		goto typeA_on_failed;

	unsigned char bb18[]={0x05, 0x80, 0x83, 0x03, 0x5A, 0x0A };
	if(BB2PNXXX(fd,bb18))
		goto typeA_on_failed;

	unsigned char pp12[]={0x03, 0xC1, 0xAA, 0xF2 };
	if(PNXXX2BB(fd,pp12))
		goto typeA_on_failed;

	unsigned char pp13[]={0x05, 0x81, 0x83, 0x80, 0x15, 0xE6 };
	//	Unpacked data = [83 80]
	if(PNXXX2BB(fd,pp13))
		goto typeA_on_failed;

	unsigned char bb19[]={0x03, 0xC1, 0xAA, 0xF2 };
	if(BB2PNXXX(fd,bb19))
		goto typeA_on_failed;

	unsigned char bb20[]={0x07, 0x89, 0x83, 0x01, 0x10, 0x00, 0x81, 0xF7 };
	if(BB2PNXXX(fd,bb20))
		goto typeA_on_failed;

	unsigned char pp14[]={0x05, 0x8A, 0x83, 0x80, 0xB3, 0xCF };
	//	Unpacked data = [83 80]
	if(PNXXX2BB(fd,pp14))
		goto typeA_on_failed;

	unsigned char bb21[]={0x03, 0xC2, 0x31, 0xC0 };
	if(BB2PNXXX(fd,bb21))
		goto typeA_on_failed;

	unsigned char bb22[]={0x05, 0x92, 0x83, 0x50, 0x69, 0x5A };
	if(BB2PNXXX(fd,bb22))
		goto typeA_on_failed;

	unsigned char pp15[]={0x03, 0xC3, 0xB8, 0xD1 };
	ALOGE("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	//	Unpacked data = [83 80]
	if(PNXXX2BB(fd,pp15))
	{
		ALOGE("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
		goto typeA_on_failed;
		ALOGE("cccccccccccccccccccccccccccccccc");
	}

	ALOGE("dddddddddddddddddddddddddddd");
	unsigned char pp16[]={0x06, 0x93, 0x83, 0x50, 0x00, 0x92, 0x0F };

	//	Unpacked data = [83 80]
	if(PNXXX2BB_CHECK(fd,pp16) == -1)
       {
			ALOGE("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
			goto typeA_on_failed;
			ALOGE("fffffffffffffffffffffffffffffffffffffffffffffff");
	}
        else 
       {
             mDetectedTagCount++;
       }
    close(fd);
    return;

typeA_on_failed:
    ALOGE("hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
	HW_RESET(fd);
	close(fd);
}
