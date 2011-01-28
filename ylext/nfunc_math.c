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

#ifdef HAVE_LIBM

#include <math.h>
#include "ylsfunc.h"

#define _MATHFUNC1(nAME)                                        \
    YLDEFNF(nAME, 1, 1) {                                       \
        double r;                                               \
        ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));  \
        r = nAME(yladbl(ylcar(e)));                             \
        return ylacreate_dbl(r);                                \
    } YLENDNF(nAME)

#define _MATHFUNC2(nAME)                                        \
    YLDEFNF(nAME, 2, 2) {                                       \
        double r;                                               \
        ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));  \
        r = nAME(yladbl(ylcar(e)), yladbl(ylcadr(e)));          \
        return ylacreate_dbl(r);                                \
    } YLENDNF(nAME)


_MATHFUNC1(cos)
_MATHFUNC1(sin)
_MATHFUNC1(tan)
_MATHFUNC1(acos)
_MATHFUNC1(asin)
_MATHFUNC1(atan)
_MATHFUNC1(cosh)
_MATHFUNC1(sinh)
_MATHFUNC1(tanh)
_MATHFUNC1(exp)
_MATHFUNC1(log)
_MATHFUNC1(log10)
_MATHFUNC1(sqrt)
_MATHFUNC1(ceil)
_MATHFUNC1(floor)
_MATHFUNC1(fabs)
_MATHFUNC2(pow)

#endif /* HAVE_LIBM */
