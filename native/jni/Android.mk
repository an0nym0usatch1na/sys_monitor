LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mon_reader
LOCAL_SRC_FILES := main.cpp filter.cpp interface.cpp ./../../share/debug.c

include $(BUILD_EXECUTABLE)
