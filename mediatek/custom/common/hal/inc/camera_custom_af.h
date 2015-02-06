
#ifndef _AF_COEF_H
#define _AF_COEF_H

#include "MediaTypes.h"

#ifdef MTK_ZSD_AF_ENHANCE

#define ISO_MAX_NUM 8
#define GMEAN_MAX_NUM 6

    typedef struct
    {
        MINT32 i4ISONum;
        MINT32 i4ISO[ISO_MAX_NUM];

        MINT32 i4GMeanNum;
        MINT32 i4GMean[GMEAN_MAX_NUM];
        
        MINT32 i4FV_DC[GMEAN_MAX_NUM][ISO_MAX_NUM];
        MINT32 i4MIN_TH[GMEAN_MAX_NUM][ISO_MAX_NUM];        
        MINT32 i4HW_TH[GMEAN_MAX_NUM][ISO_MAX_NUM];       

    }AF_THRES;

    typedef struct
    {
        MINT32 i4AFS_STEP_MIN_ENABLE;
        MINT32 i4AFS_STEP_MIN_NORMAL;
        MINT32 i4AFS_STEP_MIN_MACRO;

        MINT32 i4FRAME_TIME;
        MINT32 i4ZSD_FRAME_TIME;

        MINT32 i4AFS_MAX_STEP;
        MINT32 i4ZSD_AFS_MAX_STEP;

        MINT32 i4AFS_FAIL_POS;
        MINT32 i4VAFC_FAIL_POS;
        MINT32 i4VAFC_FAIL_CNT;

        MINT32 i4AFC_THRES_MAIN;
        MINT32 i4AFC_THRES_SUB;
        MINT32 i4VAFC_THRES_MAIN;
        MINT32 i4VAFC_THRES_SUB;        
        AF_THRES sNormalTH;
        
        MINT32 i4ZSD_AFS_THRES_MAIN;
        MINT32 i4ZSD_AFS_THRES_SUB;
        MINT32 i4ZSD_AFS_MONO_THRES;    
        MINT32 i4ZSD_AFS_MONO_OFFSET;
        MINT32 i4ZSD_AFC_THRES_MAIN;
        MINT32 i4ZSD_AFC_THRES_SUB;    
        AF_THRES sZSDTH;

        MINT32 i4AFS_FRAME_WAIT;
        MINT32 i4ZSD_AFS_FRAME_WAIT;
        MINT32 i4VAFC_FRAME_WAIT;
        
        MINT32 i4AFS_INIT_WAIT;
        MINT32 i4ZSD_AFS_INIT_WAIT;
        
        MINT32 i4AFC_DONE_WAIT;
        MINT32 i4ZSD_AFC_DONE_WAIT;
        MINT32 i4VAFC_DONE_WAIT;

        MINT32 i4CHANGE_CNT_DELTA;

        // -------------------------------
        MINT32 i4FIRST_FV_WAIT;
        MINT32 i4ZSD_FIRST_FV_WAIT;
        MINT32 i4VAFC_FIRST_FV_WAIT;

        // -------------------------------
        MINT32 i4VAFC_GS_CHANGE_THRES;    
        MINT32 i4VAFC_GS_CHANGE_OFFSET;    
        MINT32 i4VAFC_GS_CHANGE_CNT;
        
        MINT32 i4VAFC_FV_CHANGE_THRES;    
        MINT32 i4VAFC_FV_CHANGE_OFFSET;    
        MINT32 i4VAFC_FV_CHANGE_CNT;
        
        MINT32 i4VAFC_FV_STABLE1_THRES;     // percentage -> 0 more stable  
        MINT32 i4VAFC_FV_STABLE1_OFFSET;    // value -> 0 more stable
        MINT32 i4VAFC_FV_STABLE1_NUM;      // max = 9 (more stable), reset = 0
        MINT32 i4VAFC_FV_STABLE1_CNT;      // max = 9                      

        // -------------------------------
        MINT32 i4GS_CHANGE_THRES;    
        MINT32 i4GS_CHANGE_OFFSET;    
        MINT32 i4GS_CHANGE_CNT;    
        
        MINT32 i4FV_CHANGE_OFFSET;        
        
        MINT32 i4FV_STABLE1_THRES;        
        MINT32 i4FV_STABLE1_OFFSET;
        MINT32 i4FV_STABLE1_NUM;                        
        MINT32 i4FV_STABLE1_CNT;                        

        MINT32 i4FV_STABLE2_THRES;        
        MINT32 i4FV_STABLE2_OFFSET;
        MINT32 i4FV_STABLE2_NUM;                        
        MINT32 i4FV_STABLE2_CNT;                        

        // -------------------------------
        MINT32 i4ZSD_GS_CHANGE_THRES;    
        MINT32 i4ZSD_GS_CHANGE_OFFSET;    
        MINT32 i4ZSD_GS_CHANGE_CNT;    

        MINT32 i4ZSD_FV_CHANGE_THRES;    
        MINT32 i4ZSD_FV_CHANGE_OFFSET;    
        MINT32 i4ZSD_FV_CHANGE_CNT;    
        
        MINT32 i4ZSD_FV_STABLE1_THRES;        
        MINT32 i4ZSD_FV_STABLE1_OFFSET;
        MINT32 i4ZSD_FV_STABLE1_NUM;                        
        MINT32 i4ZSD_FV_STABLE1_CNT;                        

        MINT32 i4ZSD_FV_STABLE2_THRES;        
        MINT32 i4ZSD_FV_STABLE2_OFFSET;
        MINT32 i4ZSD_FV_STABLE2_NUM;                        
        MINT32 i4ZSD_FV_STABLE2_CNT;                        
        // -------------------------------

        MINT32 i4VIDEO_AFC_SPEED_ENABLE;
        MINT32 i4VIDEO_AFC_NormalNum;
        MINT32 i4VIDEO_AFC_TABLE[30];
        
    } AF_COEF_T;

#else

    typedef struct
    {
        MINT32 i4AFS_STEP_MIN_ENABLE;
        MINT32 i4AFS_STEP_MIN_NORMAL;
        MINT32 i4AFS_STEP_MIN_MACRO;

        MINT32 i4AFC_THRES_MAIN;
        MINT32 i4AFC_THRES_SUB;
        
        MINT32 i4ZSD_HW_TH;
        MINT32 i4ZSD_AFS_THRES_MAIN;
        MINT32 i4ZSD_AFS_THRES_SUB;
        MINT32 i4ZSD_AFS_THRES_OFFSET;
        MINT32 i4ZSD_AFC_THRES_MAIN;
        MINT32 i4ZSD_AFC_THRES_SUB;    
        MINT32 i4ZSD_AFC_THRES_OFFSET;
        
        MINT32 i4ZSD_AFS_MONO_THRES;    
        MINT32 i4ZSD_AFS_MONO_OFFSET;

        MINT32 i4AFS_WAIT;
        MINT32 i4AFC_WAIT;
        
        MINT32 i4FV_CHANGE_OFFSET;        
        MINT32 i4FV_STABLE_THRES;        
        MINT32 i4FV_STABLE_OFFSET;
        MINT32 i4FV_STABLE_CNT;                        

        MINT32 i4VIDEO_AFC_SPEED_ENABLE;
        MINT32 i4VIDEO_AFC_NormalNum;
        MINT32 i4VIDEO_AFC_TABLE[30];
        
    } AF_COEF_T;
#endif

	AF_COEF_T get_AF_Coef();
	
#endif /* _AF_COEF_H */

