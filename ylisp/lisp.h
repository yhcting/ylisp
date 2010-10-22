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
#ifdef CONFIG_DBG_EVAL
#   define dbg_eval(x)     do{ x } while(0)
#else /* _YLDBG_EVAL */
#   define dbg_eval(x)
#endif /* _YLDBG_EVAL */

#ifdef CONFIG_DBG_MEM
#   define dbg_mem(x)      do{ x } while(0)
#else /* CONFIG_DBG_MEM */
#   define dbg_mem(x)
#endif /* CONFIG_DBG_MEM */

#ifdef CONFIG_DBG_GEN
#   define dbg_gen(x)      do{ x } while(0)
#else /* CONFIG_DBG_MEM */
#   define dbg_gen(x)
#endif /* CONFIG_DBG_MEM */


#ifdef CONFIG_DBG_INTEVAL
#   define dbg_inteval(x)  do{ x } while(0)
#else /* CONFIG_DBG_INTEVAL */
#   define dbg_inteval(x)
#endif /* CONFIG_DBG_INTEVAL */

#include "ylsfunc.h"
#include "ylut.h"



extern ylerr_t ylnfunc_init();
extern ylerr_t ylsfunc_init();
extern ylerr_t ylinterp_init();
extern void    ylinterp_deinit();

extern void
yleclean(yle_t* e);

/*
 * GC Protection required to caller
 */
extern yle_t*
ylapply(yle_t* f, yle_t* args, yle_t* a);

/*
 * get current evaluation id.
 */
extern unsigned int
yleval_id();

/*
 * To show 'eval' stack when interpreting fails
 */
extern void
ylpush_eval_info(const yle_t* e);

extern void
ylpop_eval_info();

/*
 * only for internel use - for 'interpret-file' command!
 * (To know whether interpreting started by user request or by batch script)
 */
extern ylerr_t
ylinterpret_internal(const unsigned char* stream, unsigned int streamsz);

extern void
ylinteval_lock();

void
ylinteval_unlock();

/*
 * to avoid symbol name (function name) conflicts with plug-ins
 * ylisp uses special naming rules!
 */
#undef  YLNFN
#define YLNFN(n)    __Yl__LNF__##n##__


#endif /* ___LISp_h___ */
