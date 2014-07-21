LOCAL_PATH := $(realpath $(call my-dir)/../../../..)
PVRSDKDIR := $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE    := OGLES2Water

### Add all source file names to be included in lib separated by a whitespace
LOCAL_SRC_FILES := \
Water/FragShader.cpp \
Water/ModelFShader.cpp \
Water/ModelVShader.cpp \
Water/Mountain.cpp \
Water/MountainFloor.cpp \
Water/NewNormalMap.cpp \
Water/OGLES2Water.cpp \
Water/PlaneTexFShader.cpp \
Water/PlaneTexVShader.cpp \
Water/sail.cpp \
Water/Scene.cpp \
Water/SkyboxFShader.cpp \
Water/SkyboxVShader.cpp \
Water/Tex2DFShader.cpp \
Water/Tex2DVShader.cpp \
Water/VertShader.cpp \
Water/wood.cpp \
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
