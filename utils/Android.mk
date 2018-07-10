LOCAL_PATH := $(call my-dir)

.PHONY: kms++-targets

define kms++-define-executable
include $$(CLEAR_VARS)
LOCAL_MODULE := $(1)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(1).cpp
LOCAL_RTTI_FLAG := -frtti
LOCAL_CPPFLAGS := \
    -std=c++11 \
    -fexceptions \
    -Wextra \
    -Wno-unused-parameter \
    $(empty)
LOCAL_C_INCLUDES := $(2)
LOCAL_SHARED_LIBRARIES := $(3)
include $$(BUILD_EXECUTABLE)

kms++-targets: $(1)

endef

# kmscapture depends on glob.h that may not be available in Bionic libc.
KSMPP_UTILS := kmstest kmsview kmsprint kmsblank wbcap wbm2m
ifneq ($(strip $(wildcard bionic/libc/include/glob.h)),)
KSMPP_UTILS += kmscapture
endif
$(foreach m,$(KSMPP_UTILS), \
  $(eval $(call kms++-define-executable,$(m), \
    external/libdrm/include/drm \
    $(addprefix $(dir $(LOCAL_PATH))/,kms++/inc kms++util/inc) \
    , \
    libdrm libkms++ libkms++util)))

KSMPP_UTILS += fbtest
$(eval $(call kms++-define-executable,fbtest, \
    $(addprefix $(dir $(LOCAL_PATH))/,kms++/inc kms++util/inc) \
    , \
    libkms++ libkms++util))
