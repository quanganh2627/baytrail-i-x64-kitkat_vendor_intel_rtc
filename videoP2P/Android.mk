#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This makefile shows how to build a shared library and an activity that
# bundles the shared library and calls it using JNI.

# Modified from development/samples/SimpleJNI/Android.mk

TOP_LOCAL_PATH:= $(call my-dir)

# Build activity

LOCAL_PATH:= $(TOP_LOCAL_PATH)
include $(CLEAR_VARS)

WEBRTC_JAVA_SRC_PATH:=../../../../vendor/intel/PRIVATE/rtc/webrtc/third_party/webrtc

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    $(call all-subdir-java-files) \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_capture/android/java/org/webrtc/videoengine/VideoCaptureAndroid.java \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_capture/android/java/org/webrtc/videoengine/CaptureCapabilityAndroid.java \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_capture/android/java/org/webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_render/android/java/org/webrtc/videoengine/ViESurfaceRenderer.java \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_render/android/java/org/webrtc/videoengine/ViEAndroidGLES20.java \
    $(WEBRTC_JAVA_SRC_PATH)/modules/video_render/android/java/org/webrtc/videoengine/ViERenderer.java

LOCAL_PACKAGE_NAME := videoP2P

#LOCAL_JNI_SHARED_LIBRARIES := libwebrtc-video-p2p-jni

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_SDK_VERSION := current

include $(BUILD_PACKAGE)

# ============================================================

# Also build all of the sub-targets under this one: the shared library.
#include $(call all-makefiles-under,$(LOCAL_PATH))
include $(TOP_LOCAL_PATH)/jni/ITBAndroid.mk
