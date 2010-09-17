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



#ifndef ___LISp_h___
#define ___LISp_h___

#include "config.h"
/*
 * For debugging
 */
#ifdef __DBG_EVAL
#   define dbg_eval(x)     do{ x } while(0)
#else /* _YLDBG_EVAL */
#   define dbg_eval(x)
#endif /* _YLDBG_EVAL */

#ifdef __DBG_MEM
#   define dbg_mem(x)      do{ x } while(0)
#else /* __DBG_MEM */
#   define dbg_mem(x)
#endif /* __DBG_MEM */

#ifdef __DBG_GEN
#   define dbg_gen(x)      do{ x } while(0)
#else /* __DBG_MEM */
#   define dbg_gen(x)
#endif /* __DBG_MEM */


#ifdef __DBG_INTEVAL
#   define dbg_inteval(x)  do{ x } while(0)
#else /* __DBG_INTEVAL */
#   define dbg_inteval(x)
#endif /* __DBG_INTEVAL */

#include "ylsfunc.h"
#include "ylut.h"

extern void
yleclean(yle_t* e);

yle_t*
ylapply(yle_t* f, yle_t* args, yle_t* a);

/*
 * get current evaluation id.
 */
unsigned int
ylget_eval_id();

/*
 * To show 'eval' stack when interpreting fails
 */
void
ylpush_eval_info(const yle_t* e);

void
ylpop_eval_info();

/*
 * only for internel use - for 'interpret-file' command!
 * (To know whether interpreting started by user request or by batch script)
 */
ylerr_t
ylinterpret_internal(const char* stream, unsigned int streamsz);

void
ylinteval_lock();

void
ylinteval_unlock();

/*
 * Actually, 'pid_t' should be used.
 * But we don't want to include "sys/types.h" here..
 * So, long is used instead of 'pid_t'
 */
int
ylchild_proc_set(long pid);

void
ylchild_proc_unset();

/*
 * to avoid symbol name (function name) conflicts with plug-ins
 * ylisp uses special naming rules!
 */
#undef  YLNFN
#define YLNFN(n)    __Yl__LNF__##n##__


#endif /* ___LISp_h___ */
