adb remount
adb shell mkdir /data/ylisp
adb shell mkdir /data/ylisp/bin
adb shell mkdir /data/ylisp/yls
adb shell mkdir /data/ylisp/test

adb push jni/yls/base.yl        /data/ylisp/yls/base.yl
adb push jni/yls/ext.yl         /data/ylisp/yls/ext.yl
adb push jni/yls/init.yl        /data/ylisp/yls/init.yl

# for test
adb push libs/armeabi/ylr       /data/ylisp/bin/ylr
adb push libs/armeabi/test      /data/ylisp/test/test
adb push jni/test/testrs000     /data/ylisp/test/testrs000
adb push jni/test/testrs001     /data/ylisp/test/testrs001
adb push jni/test/testrs002     /data/ylisp/test/testrs002
adb push jni/test/test_base.yl  /data/ylisp/test/test_base.yl
adb push jni/test/test_ext.yl   /data/ylisp/test/test_ext.yl


