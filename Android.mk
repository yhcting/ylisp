LOCAL_PATH := $(call my-dir)

MY_SO_CFLAGS:= -DHAVE_CONFIG_H -fPIC -DPIC -O2

#######################################################
#
# Android ndk 8 seems not to support '-rdynamics'
# (So, plan for using on Android, is pending...)
#
#######################################################


# for ylisp
include $(CLEAR_VARS)
LOCAL_MODULE := libylisp
LOCAL_SRC_FILES := \
	ylisp/gsym.c   ylisp/interpret.c  ylisp/lisp.c     ylisp/mempool.c   ylisp/mthread.c \
	ylisp/nfunc.c  ylisp/nfunc_mt.c   ylisp/parser.c   ylisp/sfunc.c     ylisp/symlookup.c \
	ylisp/trie.c   ylisp/ut.c
LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(NDK_PROJECT_PATH)
include $(BUILD_STATIC_LIBRARY)


## for ylbase
include $(CLEAR_VARS)
LOCAL_MODULE := libylbase
LOCAL_SRC_FILES := ylbase/libmain.c  ylbase/nfunc.c
LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ylisp $(NDK_PROJECT_PATH)
include $(BUILD_STATIC_LIBRARY)


## for ylext
include $(CLEAR_VARS)
LOCAL_MODULE := libylext
LOCAL_SRC_FILES := \
	ylext/crc.c        ylext/hash.c       ylext/libmain.c \
	ylext/nfunc_arr.c  ylext/nfunc_bin.c  ylext/nfunc_map.c  ylext/nfunc_math.c \
	ylext/nfunc_re.c   ylext/nfunc_str.c  ylext/nfunc_sys.c
LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ylisp $(NDK_PROJECT_PATH)
include $(BUILD_STATIC_LIBRARY)


## for ylr
include $(CLEAR_VARS)
LOCAL_MODULE := ylr
LOCAL_SRC_FILES := ylr/main.c
LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_LDFLAGS :=
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ylisp $(NDK_PROJECT_PATH)
LOCAL_STATIC_LIBRARIES := libylisp libylbase libylext
include $(BUILD_EXECUTABLE)

## for test
include $(CLEAR_VARS)
LOCAL_MODULE := test
LOCAL_SRC_FILES := test/main.c
LOCAL_CFLAGS := -DHAVE_CONFIG_H
LOCAL_LDFLAGS :=
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ylisp $(NDK_PROJECT_PATH)
LOCAL_STATIC_LIBRARIES := libylisp libylbase libylext
include $(BUILD_EXECUTABLE)
