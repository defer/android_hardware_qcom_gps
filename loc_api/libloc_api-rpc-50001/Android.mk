
ifneq ($(TARGET_NO_RPC),true)
ifeq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION),50001)
include $(call all-subdir-makefiles)
endif
endif # not TARGET_NO_RPC := true
