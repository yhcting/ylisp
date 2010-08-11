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



/**
 * Stuffs about lisp interpreter system.
 * Naming Convention
 *   "_r" : recursive.
 *      ex. mark_r / mark
 *   leading 's' in word means : S-expression.
 *      ex. yltrue() (S-expression true)
 * Policy.
 *   NULL yle_t* means "ylinterpret_undefined result"
 *  
 */

#ifndef ___LISp_h___
#define ___LISp_h___

#include "def.h"
#include "ylisp.h"
#include "ylsfunc.h"

extern void
yleclean(yle_t* e);

yle_t*
ylapply(yle_t* f, yle_t* args, yle_t* a);

/*
 * get current evaluation id.
 */
unsigned int
ylget_eval_id();

#endif /* ___LISp_h___ */
