LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libkms++util
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    $(patsubst $(LOCAL_PATH)/%,%, \
      $(wildcard $(LOCAL_PATH)/src/*.cpp)) \
    $(empty)

LOCAL_RTTI_FLAG := -frtti
LOCAL_CPPFLAGS := \
    -std=c++11 \
    -fexceptions \
    -Wextra \
    -Wno-unused-parameter \
    $(empty)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/inc \
    $(dir $(LOCAL_PATH))/kms++/inc \
    $(empty)

LOCAL_SHARED_LIBRARIES := \
    libkms++ \
    $(empty)

include $(BUILD_SHARED_LIBRARY)
