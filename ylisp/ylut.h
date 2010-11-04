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
 * Understanding these code is far away from understand 'ylisp'.
 * This is OPTIONAL.
 * Utility functions
 */

#ifndef ___YLUt_h___
#define ___YLUt_h___

/*================================
 *
 * Misc. utilities
 *
 *================================*/

/**
 * CALLER SHOULD FREE returned MEMORY by calling 'free()'
 * @btext:
 *    TRUE : return value is string (includes trailing 0)
 *    FALSE: return value is pure binary data.
 * @outsz:
 *    if success, this includes size of file.
 *    if fails, this includes error cause
 *        YLOk: success.
 *        YLErr_io: file I/O error.
 *        YLErr_out_of_memory: not enough memory
 *
 * @return:
 *    NULL : if fails or size of file is 0.
 *    So, reading empty file returns NULL and *outsz == YLUTREADF_OK
 */
extern void*
ylutfile_read(unsigned int* outsz, const char* fpath, int btext);



#endif /* ___YLUt_h___ */
