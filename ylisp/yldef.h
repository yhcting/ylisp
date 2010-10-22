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
 * Define FUNDAMENTAL and YLISP-INDEPENDENT constants/macros
 * These macros are de-facto standard!
 * So, we don't need to worry about symbol conflicts.
 */

#ifndef ___DEf_h___
#define ___DEf_h___

#ifndef NULL
#   define NULL ((void*)0)
#endif

#ifndef TRUE
#   define TRUE 1
#endif

#ifndef FALSE
#   define FALSE 0
#endif

#ifndef offset_of
#   define offset_of(type, member) ((unsigned int) &((type*)0)->member)
#endif

#ifndef container_of
#   define container_of(ptr, type, member) ((type*)(((char*)(ptr)) - offset_of(type, member)))
#endif

#endif /* ___DEf_h___ */
