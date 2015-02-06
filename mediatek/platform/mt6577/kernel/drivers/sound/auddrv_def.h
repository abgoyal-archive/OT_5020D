
#ifndef _AUDDRV_AUDIO_STREAM_DEF_H_
#define _AUDDRV_AUDIO_STREAM_DEF_H_


//#define SOUND_FAKE_READ

//#define DEBUG_AUDDRV
//#define DEBUG_AFE_REG
//#define DEBUG_ANA_REG


#ifdef DEBUG_AUDDRV
#define PRINTK_AUDDRV(format, args...) xlog_printk(ANDROID_LOG_VERBOSE, "Sound", format, ##args )
#else
#define PRINTK_AUDDRV(format, args...)
#endif

#ifdef DEBUG_AFE_REG
#define PRINTK_AFE_REG(format, args...) xlog_printk(ANDROID_LOG_VERBOSE, "Sound", format, ##args )
#else
#define PRINTK_AFE_REG(format, args...)
#endif

#ifdef DEBUG_ANA_REG
#define PRINTK_ANA_REG(format, args...) xlog_printk(ANDROID_LOG_VERBOSE, "Sound", format, ##args )
#else
#define PRINTK_ANA_REG(format, args...)
#endif







typedef struct
{
   kal_uint32 pucPhysBufAddr;
   kal_uint8 *pucVirtBufAddr;
   kal_int32 u4BufferSize;
   kal_int32 u4DataRemained;
   kal_uint32 u4SampleNumMask;    // sample number mask
   kal_uint32 u4SamplesPerInt;    // number of samples to play before interrupting
   kal_int32 u4WriteIdx;          // Previous Write Index.
   kal_int32 u4DMAReadIdx;        // Previous DMA Read Index.
   kal_uint32 u4fsyncflag;
   kal_uint32 uResetFlag;
   struct file *flip;
} AFE_BLOCK_T;


typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} AFE_DL_CONTROL_T;

typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} AFE_UL_CONTROL_T;

typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} I2S_INPUT_CONTROL_T;

typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} I2S_OUTPUT_CONTROL_T;

typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} DAI_INPUT_CONTROL_T;

typedef struct
{
   kal_uint32     status;
   kal_uint32     open_cnt;
} DAI_OUTPUT_CONTROL_T;

typedef struct
{
   AFE_BLOCK_T    rBlock;
   kal_uint32     u4BufferSize;
   kal_bool       bRunning;
   kal_uint32     open_cnt;
} AWB_INPUT_CONTROL_T;



#endif
