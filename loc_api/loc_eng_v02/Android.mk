ifneq ($(BUILD_TINY_ANDROID),true)
#Compile this library only for builds with the latest modem image
ifeq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION),50001)
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
    loc_api_sync_req.cpp \
    ../libloc_api_50001/gps.c

ifeq ($(FEATURE_GNSS_BIT_API), true)
LOCAL_SRC_FILES += \
    loc_eng_data_server.cpp \
    loc_eng_data_server_handler.cpp \
    ../../daemon/gpsone_thread_helper.c \
    ../../daemon/gpsone_glue_msg.c \
    ../../daemon/gpsone_glue_pipe.c
endif # FEATURE_GNSS_BIT_API

## Check if GPS is unsupported
ifeq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION),50001)
    LOCAL_SHARED_LIBRARIES += libloc_api-rpc-qc
else
    LOCAL_STATIC_LIBRARIES:= libloc_api-rpc    
endif

LOCAL_CFLAGS += \
     -fno-short-enums \
     -D_ANDROID_ \
     -DUSE_QCOM_AUTO_RPC \
     -DAMSS_VERSION=$(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION)

## Includes
LOCAL_C_INCLUDES:= \
	$(TARGET_OUT_HEADERS)/libcommondefs-rpc/inc \
	$(TARGET_OUT_HEADERS)/librpc \

ifeq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION),50001)
LOCAL_C_INCLUDES += \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc-qc \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc-qc/rpc_inc \
        $(TARGET_OUT_HEADERS)/loc_api/rpcgen/inc \
        $(TARGET_OUT_HEADERS)/libcommondefs/rpcgen/inc \
        hardware/msm7k/librpc
else
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libloc_api-rpc \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc/inc
endif

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

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
include $(BUILD_SHARED_LIBRARY)
endif # BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION = 50001
endif # not BUILD_TINY_ANDROID
