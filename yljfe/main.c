/*****************************************************************************
 *    Copyright (C) 2010 Younghyung Cho. <yhcting77@gmail.com>
 *    
 *    This file is part of YLISP.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as
 *    published by the Free Software Foundation either version 3 of the 
 *    License, or (at your option) any later version.
 *    
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License 
 *    (<http://www.gnu.org/licenses/lgpl.html>) for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

/*
 * This is simple front end...
 * So, exception handling for lots of cases are missing...
 * (Buggy... but worth to use... front-end)
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <memory.h>
#include "jni.h"
#include "ylisp.h"

/*
 * "ylut.h" is optional.
 * This is included to use utility functions.
 * (Using 'ylut.h' is not essential!)
 */
#include "ylut.h"

#define _INIT_OUTBUFSZ        4*1024 /* normal page size */

/* See Main.java */
enum {
    _AC_HANDLED       = 0,
    _AC_MORE_PREFIX   = 1,
    _AC_COMPLETE      = 2,
};

static int _loglv = YLLogW;


static int
_print(const char* format, ...) {
    /* print to standard out directly */
    va_list args;
    int     ret;
    va_start (args, format);
    ret = vprintf(format, args);
    va_end (args);
    /* we want to print its string immediately */
    /* fflush(stdout); */
    return ret;
}

static void
_log(int lv, const char* format, ...) {
    if(lv >= _loglv) {
        va_list ap;
        /* print to standard out directly */
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

static void
_assert(int a) { assert(a); }


ylutdynb_t dynb = {0, 0, NULL};

/* ===========================================
 * JNI Functions - START
 * ===========================================*/


/*
 * Class:     Main
 * Method:    nativeInterpret
 * Signature: (Ljava/lang/String;)Z
 */
static jboolean JNICALL 
_jni_Main_nativeInterpret
(JNIEnv* jenv, jobject jobj, jstring jstr) {
    jboolean       jret = JNI_TRUE;
    const char*    stream;

    stream = (*jenv)->GetStringUTFChars(jenv, jstr, NULL);

    ylutstr_reset(&dynb);
    if( YLOk != ylinterpret(stream, strlen(stream)) ) {
        jret = JNI_FALSE;
    }

    (*jenv)->ReleaseStringUTFChars(jenv, jstr, stream);

    return jret;
}

/*
 * Class:     Main
 * Method:    nativeInterrupt
 * Signature: ()Z
 */
static jboolean JNICALL 
_jni_Main_nativeForceStop
(JNIEnv* jenv, jobject jobj) {
    jboolean       jret = JNI_TRUE;
    ylforce_stop();
    return jret;
}

/*
 * Class:     Main
 * Method:    nativeGetLastInterpretMessage
 * Signature: ()Ljava/lang/String;
 */
static jstring JNICALL 
_jni_Main_nativeGetLastNativeMessage
(JNIEnv* jenv, jobject jobj) {
    jstring jret;
    jret = ((*jenv)->NewStringUTF(jenv, ylutstr_string(&dynb)));
    if(!jret) {
        _print("Not enough Java memory to get native message!\n"
               "Native message size : %d\n", ylutstr_len(&dynb));
    }

    /* shrink */
    if(ylutstr_len(&dynb) > _INIT_OUTBUFSZ) {
        ylutdynb_clean(&dynb);
        if( 0 > ylutstr_init(&dynb, _INIT_OUTBUFSZ) ) {
            /* fail to alloc page... may be due to external fragmentation?? */
            assert(0);
        }
    }

    return jret;
}

/*
 * Class:     Main
 * Method:    nativeSetLogLevel
 * Signature: (I)V
 */
static void JNICALL 
_jni_Main_nativeSetLogLevel
(JNIEnv* jenv, jobject jobj, jint lv) {
    if(lv < YLLogV) { lv = YLLogV; }
    if(lv > YLLogE) { lv = YLLogE; }
    _loglv = lv;
}

/*
 * Class:     Main
 * Method:    nativeAutoComplete
 * Signature: (Ljava/lang/String;)I
 */
static jint JNICALL 
_jni_Main_nativeAutoComplete
(JNIEnv* jenv, jobject jobj, jstring jprefix) {
    int         ret;
    const char* prefix;

    prefix = (*jenv)->GetStringUTFChars(jenv, jprefix, NULL);

    ylutstr_reset(&dynb);
    ret = yltrie_get_more_possible_prefix(prefix, ylutstr_ptr(&dynb), ylutdynb_freesz(&dynb));
    /* we need to assess directly..here... due to limitation of API */
    dynb.sz += strlen(ylutstr_string(&dynb));

    switch(ret) {
        case 0: {
            if(ylutstr_len(&dynb) > 0) {
                ret = _AC_MORE_PREFIX;
            } else {
                /* we need to retrieve candidates.. */
                int            num, i;
                unsigned int   maxlen;
                char**         pp;
                num = ylget_candidates_num(prefix, &maxlen);
                assert(num > 1);
                pp = malloc(sizeof(char*)*num);
                if(!pp) { 
                    _print("Fail to alloc memory : %d\n", num);
                    assert(0);
                }
                for(i=0; i<num; i++) {
                    pp[i] = malloc(maxlen+1);
                    if(!pp[i]) {
                        _print("Fail to alloc memory : %d\n", maxlen+1);
                        assert(0);
                    }
                }
                i = ylget_candidates(prefix, pp, num, maxlen+1);
                assert(i==num);
                for(i=0; i<num; i++) {
                    _print("%s%s\n", prefix, pp[i]);
                    free(pp[i]);
                }
                free(pp);
                _print("\n");
                ret = _AC_HANDLED;
            }
        } break;

        case 1: {
            ret = _AC_COMPLETE;
            /* nothing to do */
        } break;

        case 2: {
            ret = _AC_HANDLED;
            /* nothing to do */
        } break;

        default:
            _print("Internal error to try auto-completion. Out of memory?\n");
            ret = -1; /* error case */
    }

    (*jenv)->ReleaseStringUTFChars(jenv, jprefix, prefix);
    
    return ret;
}

/* ===========================================
 * JNI Functions - END
 * ===========================================*/

static int
_register_natives(JNIEnv* jenv) {

    /* Natives */
    const JNINativeMethod jninms[] = {
        { "nativeInterpret", 
          "(Ljava/lang/String;)Z",
          (void*)_jni_Main_nativeInterpret },
        { "nativeForceStop", 
          "()Z",
          (void*)_jni_Main_nativeForceStop },
        { "nativeGetLastNativeMessage",
          "()Ljava/lang/String;",
          (void*)_jni_Main_nativeGetLastNativeMessage },
        { "nativeSetLogLevel",
          "(I)V",
          (void*)_jni_Main_nativeSetLogLevel },
        { "nativeAutoComplete",
          "(Ljava/lang/String;)I",
          (void*)_jni_Main_nativeAutoComplete },
    };

    jclass cls = (*jenv)->FindClass(jenv, "Main");
    if(!cls) { return -1; }
    
    if( 0 > (*jenv)->RegisterNatives(jenv, cls, jninms, 
                                 sizeof(jninms)/sizeof(jninms[0])) ) {
        return -1;
    }
    return 0;
}


static JNIEnv* 
_create_jvm(JavaVM** jvm) {
    JNIEnv*             jenv;
    JavaVMInitArgs      jvmargs;
    JavaVMOption        jvmopt;

    /* TODO : env variable should be used for this! */
    jvmopt.optionString = "-Djava.class.path=./jsrc/bin";
    jvmargs.version = JNI_VERSION_1_6;
    jvmargs.nOptions = 1;
    jvmargs.options = &jvmopt;
    jvmargs.ignoreUnrecognized = 0;

    if(0 > JNI_CreateJavaVM(jvm, (void**)&jenv, &jvmargs)) {
        /* error case */
        return NULL;
    }
    return jenv;
}

static int
_start_java(JavaVM* jvm, JNIEnv* jenv) {
    jclass         jcls_main = NULL;
    jmethodID      jmid_main = NULL;
    jcls_main = (*jenv)->FindClass(jenv, "Main");
    if(!jcls_main) { return -1; }

    jmid_main = (*jenv)->GetStaticMethodID(jenv, jcls_main, "main", "([Ljava/lang/String;)V");
    if(!jmid_main) { return -1; }
    (*jenv)->CallStaticVoidMethod(jenv, jcls_main, jmid_main, NULL);
}


int
main(int argc, char* argv[]) {
    /* initialize ylisp */
    { /* just scope */
        ylsys_t     sys;
    
        sys.print     = _print;
        sys.log       = _log;
        sys.assert    = _assert;
        sys.malloc    = malloc;
        sys.free      = free;

        if(YLOk != ylinit(&sys)) {
            printf("Error: Fail to initialize ylisp\n");
            return 0;
        }
    }

    /* This should be called after ylinit() */
    ylutstr_init(&dynb, _INIT_OUTBUFSZ);

    /* run initial scripts if required */
    if(argc > 1) {
        unsigned char*   strm;
        unsigned int     strmsz;
        strm = ylutfile_read(&strmsz, "../yls/yljfe_initrc.yl", 0);
        if(!strm) {
            printf("Fail to read initrc file..\n");
            return 0;
        }
        if(YLOk != ylinterpret(strm, strmsz)) {
            printf("Fail to interpret initrc script.\n");
            return 0;
        }
        free(strm);
    }

    /* start java */
    { /* just scope */
        JNIEnv*    jenv;
        JavaVM*    jvm;

        jenv = _create_jvm(&jvm);
        if(!jenv) {
            printf("Error: Fail to create JavaVM\n");
            return 0;
        }

        if(0 > _register_natives(jenv)) {
            printf("Error: Fail to register natives\n");
            return 0;
        }

        if(0 > _start_java(jvm, jenv)) {
            printf("Error : Start java\n");
            return 0;
        }

        while(1) {
            sleep(60*60*1000);
        }
        (*jvm)->DestroyJavaVM(jvm);
    }
}
