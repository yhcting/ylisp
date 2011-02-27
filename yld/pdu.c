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
 *    along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netinet/in.h>

#include "yld.h"

static const unsigned char* _CMD_DELIMIETER = (unsigned char*)":";

static int
_find_delimiter_pos(const unsigned char* stream, unsigned int sz) {
	int i = 0;
	while (i < sz) {
		if (*_CMD_DELIMIETER == stream[i])
			break;
		else
			i++;
	}
	if (i < sz)
		return i; /* found */
	else
		return -1;	       /* fail */
}

int
pdu_build(yldynb_t*		b,
	  const unsigned char* cmd,
	  const unsigned char* data,
	  unsigned int		sz) {
	assert(b && cmd && data);
	yldynb_reset(b);
	yldynb_append(b, cmd, strlen((char*)cmd));
	yldynb_append(b, _CMD_DELIMIETER, 1);
	yldynb_append(b, data, sz);
	return 0;
}

int
pdu_parser(yldynb_t*		   cmd,	  /* out */
	   yldynb_t*		   data,  /* out */
	   const unsigned char*   pdu,
	   unsigned int	   sz) {
	int i;
	assert(cmd && data && pdu);
	yldynb_reset(cmd);
	yldynb_reset(data);
	i = _find_delimiter_pos(pdu, sz);
	if (i < 0)
		return -1; /* fail to parse */
	/* parsing command */
	yldynb_append(cmd, pdu, i);
	yldynb_append(cmd,(unsigned char*)"", 1); /* add trailing 0 */

	/* getting data */
	yldynb_append(data, pdu + i + 1, sz - i - 1);
	return 0;
}



