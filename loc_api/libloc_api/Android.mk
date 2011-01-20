#  Copyright (c) 2009,2011 Code Aurora Forum. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided
#        with the distribution.
#      * Neither the name of Code Aurora Forum, Inc. nor the names of its
#        contributors may be used to endorse or promote products derived
#        from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
#  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ifneq ($(BUILD_TINY_ANDROID),true)

AMSS_VERSION:=6356

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := gps.$(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE)

## Libs

LOCAL_SHARED_LIBRARIES := \
    librpc \
    libutils \
    libcutils

LOCAL_SRC_FILES += \
    loc_eng.cpp \
    loc_eng_ioctl.cpp \
    loc_eng_xtra.cpp \
    loc_eng_ni.cpp \
    loc_eng_log.cpp \
    loc_eng_cfg.cpp \
    gps.c

ifeq ($(FEATURE_GNSS_BIT_API), true)
LOCAL_SRC_FILES += \
    loc_eng_data_server.cpp \
    loc_eng_data_server_handler.cpp \
    ../../daemon/gpsone_thread_helper.c \
    ../../daemon/gpsone_glue_msg.c \
    ../../daemon/gpsone_glue_pipe.c
endif # FEATURE_GNSS_BIT_API

## Check if GPS is unsupported
ifeq (true, $(strip $(GPS_NOT_SUPPORTED)))
    LOCAL_SRC_FILES := loc_eng_null.cpp
else
    LOCAL_SHARED_LIBRARIES += libloc_api-rpc-qc
endif

LOCAL_CFLAGS += \
     -fno-short-enums -D_ANDROID_

## Includes

LOCAL_C_INCLUDES:= \
	$(TARGET_OUT_HEADERS)/libcommondefs-rpc/inc \
	$(TARGET_OUT_HEADERS)/librpc \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc-qc \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc-qc/rpc_inc

ifeq ($(FEATURE_GNSS_BIT_API), true)
GPSONE_BIT_API_DIR = ../../../modem-apis/$(QCOM_TARGET_PRODUCT)/api/libs/remote_apis/gpsone_bit_api
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../../../common/inc \
        $(LOCAL_PATH)/../../../oncrpc/inc \
        $(LOCAL_PATH)/../../../diag/include \
        $(LOCAL_PATH)/$(GPSONE_BIT_API_DIR)/rpc/inc \
        $(LOCAL_PATH)/$(GPSONE_BIT_API_DIR)/inc \
	$(LOCAL_PATH)/../../daemon
endif # FEATURE_GNSS_BIT_API

LOCAL_CFLAGS += -DUSE_QCOM_AUTO_RPC \
                -DAMSS_VERSION=$(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION)
LOCAL_C_INCLUDES+= \
	$(TARGET_OUT_HEADERS)/loc_api/rpcgen/inc \
	$(TARGET_OUT_HEADERS)/libcommondefs/rpcgen/inc \
	$(API_SRCDIR)/libs/remote_apis/loc_api/rpcgen/inc \
	$(API_SRCDIR)/libs/remote_apis/commondefs/rpcgen/inc \
	hardware/msm7k/librpc

LOCAL_CFLAGS+=$(GPS_FEATURES) \
	-D_ANDROID_

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
include $(BUILD_SHARED_LIBRARY)

endif # not BUILD_TINY_ANDROID
