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

/********************************
 *
 * Evaluation Thread Management!
 *
 ********************************/


#ifndef ___MTHREAd_h___
#define ___MTHREAd_h___

#include "lisp.h"

#define INVALID_TID -1

/* ETST : Evaluation Thread STate */
enum {
    ETST_SAFE = 0x00000001,
};

#define etst_set(cxt, m)    do { ((cxt)->state) |= (m); } while(0)
#define etst_clear(cxt, m)  do { ((cxt)->state) &= ~(m); } while(0)
#define etst_isset(cxt, m)  (!!((cxt)->state & (m)))


/* ETSIG : Evaluation Thread SIGnals */
enum {
    ETSIG_KILL = 0x00000001,
};

#define etsig_set(cxt, m)    do { ((cxt)->sig) |= (m); } while(0)
#define etsig_clear(cxt, m)  do { ((cxt)->sig) &= ~(m); } while(0)
#define etsig_isset(cxt, m)  (!!((cxt)->sig & (m)))



/*
 * All these interface are called inside lock of 'ylmt' module.
 * And most 'ylmt_xxx' functions lock mutex.
 * So, using those in below listener functions may cause deadlock!
 * DO NOT USE 'ylmt_xxx' public functions in it!
 * And definitely, EXITING from thread is also NOT ALLOWED!
 *
 * @cxt: (last) evaluation thread entered int safe state.
 * @mtx: mutex that keeps thread in safe mode.
 *       'pthread_cond_t' MUST be used with @mtx !!
 *       (HEY! BECAREFUL to use this MUTEX!)
 */
typedef struct {
    void(*    pre_add)      (const yletcxt_t*);
    void(*    post_add)     (const yletcxt_t*);
    void(*    pre_rm)       (const yletcxt_t*);
    void(*    post_rm)      (const yletcxt_t*);
    void(*    thd_safe)     (const yletcxt_t* cxt, pthread_mutex_t* mtx);
    void(*    all_safe)     (pthread_mutex_t* mtx);
} ylmtlsn_t; /* ylmt LiSteN InterFace */


extern ylerr_t
ylmt_init();

extern void
ylmt_deinit();

extern void
ylmt_add(yletcxt_t* cxt);

extern void
ylmt_rm(yletcxt_t* cxt);

extern unsigned int
ylmt_nr_pres(yletcxt_t* cxt);

extern void
ylmt_close_all_pres(yletcxt_t* cxt);

/*
 * This only can be called at "initialization" stage!
 */
extern void
ylmt_register_listener(const ylmtlsn_t*);

/*
 * Are all evaluation thread in safe state?
 */
extern int
ylmt_is_safe(yletcxt_t* cxt);

/*
 * Kill evaluation thread!
 * Mechanism.
 *    kill child process
 *    Set 'KILL' signal.
 */
extern int
ylmt_kill(yletcxt_t* cxt, pthread_t tid);

/*
 * @cb is called in 'lock'
 * So, 'ylmt_xxx' SHOULD NOT be used inside callback due to risk of deadlock!
 * See comments of 'ylmtlsn_t' for more details.
 *
 * @cxt : calling thread context. This can be NULL.
 */
extern void
ylmt_walk(yletcxt_t* cxt, void* user,
           /* return 1 to keep going, 0 to stop */
          int(*cb)(void*, yletcxt_t* cxt));

/*
 * locked version.
 */
extern void
ylmt_walk_locked(yletcxt_t* cxt, void* user,
                 /* return 1 to keep going, 0 to stop */
                 int(*cb)(void*, yletcxt_t* cxt));

#endif /* ___ETHREAd_h___ */
