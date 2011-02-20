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

#ifndef __YLd_h__
#define __YLd_h__

#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <malloc.h>

#include "ylisp.h"
#include "yldynb.h"

/* Pre-requsite */
#if USHRT_MAX != 65535 || UINT_MAX != 4294967295
#       error Unsupported platform.
#endif

/**********************
 * Configuation Flag!
 **********************/
/* Debugging general */
/* #define _DBG */

#ifdef _DBG
#       define dbg(x) do { x } while(0)
#else /* _DBG */
#       define dbg(x) do { } while(0)
#endif /* _DBG */


/*
 * This should be used when size is significant
 */
typedef unsigned char  u1;
typedef unsigned short u2;
typedef unsigned int   u4;

typedef char           s1;
typedef short          s2;
typedef int            s4;

/* delimiter should be char (NOT string) */

#define not_used(e) do { (e)=(e); } while(0)

/* Macros to handle bit mask! */
#define bset(x, mask)    do { (x) |= (mask);  } while(0)
#define bclear(x, mask)  do { (x) &= ~(mask); } while(0)
#define bisset(x, mask)  (!!((x) & (mask)))

/* Some useful macros */
#define arrsz(x)         (sizeof (x) / sizeof ((x)[0]))


/****************************
 * Commands
 ****************************/

/* Request commands */
#define    CMD_AUTOCOMP    "AUTOCOMP"
#define    CMD_CHGLOGLV    "CHGLOGLV"
#define    CMD_INTERP      "INTERP"

/* Event commands */
#define    CMD_PRINT       "PRINT"
#define    CMD_LOG         "LOG"

/* Response commands */
/* there is more symbol candidates */
#define    CMD_AUTOCOMP_MORE     "AUTOCOMP_MORE"
/* symbol is auto-completed */
#define    CMD_AUTOCOMP_COMP     "AUTOCOMP_COMP"
/* print candidates */
#define    CMD_AUTOCOMP_PRINT    "AUTOCOMP_PRINT"


/****************************
 * Socket interface wrapper
 ****************************/
int
send_response (unsigned char* b, unsigned int bsz);

/****************************
 * Command processing
 ****************************/
/*
 * @return : < 0 (ex. invalid command)
 */
int
cmd_do (const unsigned char* cmd,
        const unsigned char* data,
        unsigned int         sz);

/****************************
 * PDU part interface
 ****************************/

/*
 * @return : -1 (fail)
 */
int
pdu_build (yldynb_t*            b,      /* out */
           const unsigned char* cmd,
           const unsigned char* data,
           unsigned int         sz);

/*
 * @cmd : buffer data is NULL trailing string.
 * @return : -1 (fail: Not a valid pdu)
 */
int
pdu_parser (yldynb_t*            cmd,   /* out */
            yldynb_t*            data,  /* out */
            const unsigned char* pdu,
            unsigned int         sz);

/****************************
 * Socket part interface
 ****************************/

/*
 * returns socket instance, NULL if fails.
 * @port  : port to listen
 * @rcv   : callback listening socket.
 */
struct _stsock;
typedef struct _stsock* sock_t;

/*
 * create, bind, listen, accept
 */
extern sock_t
sock_init (int port);

extern int
sock_deinit (sock_t);

/*
 * Start receive loop
 * @rcv :
 *    If @rcv returns value that is <=0, than connection will be shutdown.
 *    To continue, return values which is >=0 
 *    Buffer passed as parameter is heap-allocated one.
 *    So, freeing this is callback's responsibility.
 * This never returns if success.
 * <0 : if fails
 */
extern int
sock_recv (sock_t s, void* user,
           int (*rcv) (void*, unsigned char*, unsigned int));

extern int
sock_send (sock_t s, unsigned char* b, unsigned int bsz);

#endif /* __YLd_h__ */
