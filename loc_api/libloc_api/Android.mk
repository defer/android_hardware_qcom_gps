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
