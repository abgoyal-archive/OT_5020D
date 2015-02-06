LOCAL_PATH:=$(call my-dir)
include $(CLEAR_VARS)
#LOCAL_ARM_MODE:=arm
#Added [BEGIN] by jrd.lipeng for audio parameters customization(PR277804)
ifeq ($(JRD_AUDIO_PARAMS_CUST),yes)
  LOCAL_CFLAGS += -DJRD_AUDIO_PARAMS_CUST
endif
#Added [END] by jrd.lipeng for audio parameters customization(PR277804)
LOCAL_SHARED_LIBRARIES:= libc libcutils
LOCAL_SRC_FILES:= \
	CFG_file_info.c
LOCAL_C_INCLUDES:= \
      $(MTK_PATH_CUSTOM)/cgen/cfgfileinc \
      $(MTK_PATH_CUSTOM)/cgen/cfgdefault \
      $(MTK_PATH_SOURCE)/external/nvram/libnvram
 
ifeq ($(JRD_CAMRY_AUDIO_PARAMETERS),yes)
      LOCAL_C_INCLUDES+= \
      $(MTK_PATH_CUSTOM)/cgen/inc/camry_global_JB
else
      LOCAL_C_INCLUDES+= \
      $(MTK_PATH_CUSTOM)/cgen/inc 
endif
LOCAL_MODULE:=libcustom_nvram
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE:= false
include $(BUILD_SHARED_LIBRARY)


