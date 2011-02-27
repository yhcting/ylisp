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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "yldev.h"

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#       include "nfunc.in"
#undef NFUNC

extern int ylbase_nfunc_init();

#ifdef CONFIG_STATIC_CNF
void
ylcnf_load_ylbase() {
#else /* CONFIG_STATIC_CNF */
void
ylcnf_onload(yletcxt_t* cxt) {
#endif /* CONFIG_STATIC_CNF */

	ylbase_nfunc_init();
	/* return if fail to register */
#define NFUNC(n, s, type, desc)						\
	if (YLOk != ylregister_nfunc(YLDEV_VERSION ,			\
				     s,					\
				     YLNFN(n),				\
				     type,				\
				     ">> lib: ylbase <<\n" desc))	\
		return;
#       include "nfunc.in"
#undef NFUNC

}

#ifdef CONFIG_STATIC_CNF
void
ylcnf_unload_ylbase() {
#else /* CONFIG_STATIC_CNF */
void
ylcnf_onunload(yletcxt_t* cxt) {
#endif /* CONFIG_STATIC_CNF */

#define NFUNC(n, s, type, desc) ylunregister_nfunc(s);
#       include "nfunc.in"
#undef NFUNC

}
