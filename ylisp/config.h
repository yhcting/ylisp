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



#ifndef ___CONFIg_h___
#define ___CONFIg_h___


/* ---------------------------------
 * Interpreter Internal Configuration
 * ---------------------------------*/

/* size of memory pool. the more the better! */
#define MPSIZE  (128*1024)


/* ---------------------------------
 * Switches for debugging
 * ---------------------------------*/
#define __ENABLE_LOG
#define __ENABLE_ASSERT

/* #define __DBG_GEN */  /* general debugging - usually required */
/* #define __DBG_EVAL*/  /* to debug evaluation */
/* #define __DBG_MEM */  /* to debug memory pool and GC */



#endif /* ___CONFIg_h___ */
