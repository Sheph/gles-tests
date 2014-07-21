LOCAL_PATH := $(realpath $(call my-dir)/../.. )

PVRSDKDIR := $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE    := ogles2tools

### Add all source file names to be included in lib separated by a whitespace
LOCAL_SRC_FILES := 	Tools/PVRTPrint3DAPI.cpp \
					Tools/PVRTgles2Ext.cpp \
					Tools/PVRTTextureAPI.cpp \
					Tools/PVRTBackground.cpp \
					Tools/PVRTPFXParserAPI.cpp \
					Tools/PVRTShader.cpp

LOCAL_SRC_FILES += 	Tools/PVRTFixedPoint.cpp \
					Tools/PVRTMatrixF.cpp \
					Tools/PVRTMisc.cpp \
					Tools/PVRTTrans.cpp \
					Tools/PVRTVertex.cpp \
					Tools/PVRTModelPOD.cpp \
					Tools/PVRTDecompress.cpp \
					Tools/PVRTTriStrip.cpp \
					Tools/PVRTTexture.cpp \
					Tools/PVRTPrint3D.cpp \
					Tools/PVRTResourceFile.cpp \
					Tools/PVRTString.cpp \
					Tools/PVRTPFXParser.cpp \
					Tools/PVRTShadowVol.cpp \
					Tools/PVRTVector.cpp \
					Tools/PVRTError.cpp \
					Tools/PVRTUnicode.cpp \
					Tools/PVRTQuaternionF.cpp

LOCAL_C_INCLUDES := $(PVRSDKDIR)/include/OGLES2 $(PVRSDKDIR)/include

LOCAL_CFLAGS := -DBUILD_OGLES2

ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_CFLAGS += -fno-stack-protector 
endif

include $(BUILD_STATIC_LIBRARY)