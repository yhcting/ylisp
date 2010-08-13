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




#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <memory.h>
#include "jni.h"
#include "ylisp.h"


#define _MAX_SYM_LEN          1024

/* See Main.java */
enum {
    _AC_WRONG_PREFIX  = 0,
    _AC_CANDIDATES    = 1,
    _AC_MORE_PREFIX   = 2,
    _AC_COMPLETE      = 3,
};


static int _loglv = YLLogW;

static struct {
    char*          b;
    unsigned int   sz;    /* not including tailing NULL */
    unsigned int   limit;
} _outmsg;


static int
_vprint(const char* format, va_list args) {
#define __BUFSZ 4*1024 /* usually, page size is 4KB */

    static char   __buf[__BUFSZ];

    char*         tmp;
    int           cw; /* charactera written */

    /* -1 for null-terminator */
    cw = vsnprintf (__buf, __BUFSZ -1, format, args);
    
    if(cw >= 0) {
        __buf[cw] = 0; /* trailing NULL */
        /* secure enough memory space */
        while(_outmsg.sz + cw > _outmsg.limit) {
            tmp = malloc(_outmsg.limit*2);
            memcpy(tmp, _outmsg.b, _outmsg.limit);
            free(_outmsg.b);
            _outmsg.b = tmp;
            _outmsg.limit *= 2;
        }
        memcpy(_outmsg.b + _outmsg.sz, __buf, cw);
        _outmsg.sz += cw;
    }
    
#undef __BUFSZ    
    return cw;
}

static int
_print(const char* format, ...) {
    va_list args;
    int     ret;
    va_start (args, format);
    ret = _vprint(format, args);
    va_end (args);
    return ret;
}

static void
_log(int lv, const char* format, ...) {
    if(lv >= _loglv) {
        va_list ap;
        va_start(ap, format);
        _vprint(format, ap);
        va_end(ap);
    }
}


static void
_assert(int a) { 
    assert(a); 
}

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

    _outmsg.sz = 0; /* clean out message */
    if( YLOk != ylinterpret(stream, strlen(stream)) ) {
        jret = JNI_FALSE;
    }

    (*jenv)->ReleaseStringUTFChars(jenv, jstr, stream);

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
    _outmsg.b[_outmsg.sz] = 0; /* trailing 0 */
    return ((*jenv)->NewStringUTF(jenv, _outmsg.b));
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

    _outmsg.sz = 0; /* clean out message */
    ret = yltrie_get_more_possible_prefix(prefix, _outmsg.b, _outmsg.limit);
    _outmsg.sz = strlen(_outmsg.b);

    switch(ret) {
        case 0: {
            if(0 < _outmsg.sz) {
                ret = _AC_MORE_PREFIX;
            } else {
                /* we need to retrieve candidates.. */
                int            num, i;
                unsigned int   maxlen;
                char**         pp;
                num = ylget_candidates_num(prefix, &maxlen);
                assert(num > 1);
                pp = malloc(sizeof(char*)*num);
                for(i=0; i<num; i++) {
                    pp[i] = malloc(maxlen+1);
                }
                i = ylget_candidates(prefix, pp, num, maxlen+1);
                assert(i==num);
                for(i=0; i<num; i++) {
                    _print("%s%s\n", prefix, pp[i]);
                    free(pp[i]);
                }
                free(pp);
                ret = _AC_CANDIDATES;
            }
        } break;

        case 1: {
            ret = _AC_COMPLETE;
            /* nothing to do */
        } break;

        case 2: {
            ret = _AC_WRONG_PREFIX;
            /* nothing to do */
        } break;

        default:
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
    jvmopt.optionString = "-Djava.class.path=/home/hbg683/Develop/cedetws/ylisp/yljfe/jsrc/bin";
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

    /* initialize */
    _outmsg.limit = 16*1024; /* 16KB output buffer - initial value */
    _outmsg.b = (char*)malloc(_outmsg.limit);
    _outmsg.sz = 0;

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

    /* run initial scripts if required */
    if(argc > 1) {
        int     i;
        char    cmd[2*1024]; /* temporary buffer */
        char   *p, *base;
        const char* prefix = "(interpret-file '";
        base = cmd + strlen(prefix);
        memcpy(cmd, prefix, base-cmd);
        for(i=1; i<argc; i++) {
            p = base + strlen(argv[i]);
            memcpy(base, argv[i], p-base);
            *p = ')'; p++; *p = 0; /* trailing NULL */
            if( YLOk != ylinterpret(cmd, p - cmd) ) {
                printf("Error: Fail to interpret script [%s]\n", argv[i]);
                return  0;
            }
        }
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
