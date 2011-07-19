ifneq ($(BUILD_TINY_ANDROID),true)
#Compile this library only for builds with the latest modem image

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

## Libs
LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils

LOCAL_SRC_FILES += \
    loc_log.cpp


LOCAL_CFLAGS += \
     -fno-short-enums \
     -D_ANDROID_

## Includes
LOCAL_C_INCLUDES:= \
   $(TARGET_OUT_HEADERS)/librpc \
   $(TARGET_OUT_HEADERS)/loc_api/rpcgen/inc \
   $(TARGET_OUT_HEADERS)/libloc-rpc/rpc_inc \
   $(TARGET_OUT_HEADERS)/libloc_api-rpc-qc/rpc_inc \
   $(TARGET_OUT_HEADERS)/libcommondefs-rpc \
   $(TARGET_OUT_HEADERS)/libcommondefs/rpcgen/inc \
   hardware/msm7k/librpc

LOCAL_COPY_HEADERS_TO:= gps.utils/
LOCAL_COPY_HEADERS:= \
   loc_log.h \
   loc_dbg.h

LOCAL_MODULE := libgps.utils

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
include $(BUILD_SHARED_LIBRARY)
endif # not BUILD_TINY_ANDROID
