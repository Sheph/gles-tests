LOCAL_PATH := $(realpath $(call my-dir)/../../../..)
PVRSDKDIR := $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE    := OGLES2ChameleonMan

### Add all source file names to be included in lib separated by a whitespace
LOCAL_SRC_FILES := \
ChameleonMan/ChameleonBelt.cpp \
ChameleonMan/ChameleonScene.cpp \
ChameleonMan/DefaultFragShader.cpp \
ChameleonMan/DefaultVertShader.cpp \
ChameleonMan/FinalChameleonManHeadBody.cpp \
ChameleonMan/FinalChameleonManLegs.cpp \
ChameleonMan/lamp.cpp \
ChameleonMan/OGLES2ChameleonMan.cpp \
ChameleonMan/SkinnedFragShader.cpp \
ChameleonMan/SkinnedVertShader.cpp \
ChameleonMan/skyline.cpp \
ChameleonMan/Tang_space_BeltMap.cpp \
ChameleonMan/Tang_space_BodyMap.cpp \
ChameleonMan/Tang_space_LegsMap.cpp \
ChameleonMan/Wall_diffuse_baked.cpp \
				   Shell/PVRShell.cpp \
				   Shell/PVRShellAPI.cpp \
				   Shell/PVRShellOS_Android.cpp

LOCAL_C_INCLUDES :=	\
				    $(PVRSDKDIR)/include/OGLES2 \
				    $(PVRSDKDIR)/include \
				    $(PVRSDKDIR)/Shell/Android

LOCAL_CFLAGS := -DBUILD_OGLES2 -DPVRSHELL_FPS_OUTPUT

ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_CFLAGS += -fno-stack-protector 
endif

LOCAL_LDLIBS :=  \
				-llog \
				-landroid \
				-lEGL \
				-lGLESv2

LOCAL_STATIC_LIBRARIES := \
				          android_native_app_glue \
				          ogles2tools 

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
