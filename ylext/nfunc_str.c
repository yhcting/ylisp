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

/* =========================================
 * We may support utf-8 later.
 * So, 'Binary' type cannot be put here even if
 *   it's almost same in terms of handling 'ascii' string.
 * =========================================*/

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "ylsfunc.h"

YLDEFNF(strlen, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	return ylacreate_dbl(strlen(ylasym(ylcar(e)).sym));
} YLENDNF(strlen)

YLDEFNF(split_to_line, 1, 1) {
	char     *p, *ps, *ln;
	yle_t*   lne; /* pair E, line E */
	yle_t    *rh, *rt;  /* return head, return tail */
	unsigned int len;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	/* get dummy pair head */
	rt = rh = ylcons(ylnil(), ylnil());

	p = ylasym(ylcar(e)).sym;;
	while (1) {
		len = 0;
		ps = p;

		/* count line length */
		while (0 != *p && '\n' != *p) {
			p++;
			len++;
		}

		/* remove last carrage return */
		if ('\r' == *(p-1))
			len--;

		ln = ylmalloc(len + 1); /* '+1' for trailing 0 */
		/*
		 * TODO: exception handling!
		 * But not critical.. this happends rarely
		 */
		ylassert(ln);
		memcpy(ln, ps, len);
		ln[len] = 0; /* add trailing 0 */

		/*
		 * now we got line.
		 * Let's make expression and append to the list
		 */
		lne = ylacreate_sym(ln);
		ylpsetcdr(rt, ylcons(lne, ylnil()));
		rt = ylcdr(rt);

		if (0 == *p || 0 == *(p+1))
			break; /* it's end */
		else
			p++; /* move to next character */
	}

	return ylcdr(rh);
} YLENDNF(split_to_line)

YLDEFNF(char_at, 2, 2) {
	int         idx;
	int         len;
	const char* p;
	char*       ch;

	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && ylais_type(ylcadr(e), ylaif_dbl()));

	/* check index range */
	idx = (int)yladbl(ylcadr(e));
	p = ylasym(ylcar(e)).sym;
	len = strlen(p);
	if (0 > idx || idx >= len)
		ylnfinterp_fail(YLErr_func_invalid_param,
				"invalid index value\n");

	p += idx;
	ch = ylmalloc(sizeof(char) * 2); /* 1 for tailing NULL */
	*ch = *p;
	ch[1] = 0; /* trailing NULL */
	return ylacreate_sym(ch);
} YLENDNF(char_at)


YLDEFNF(strcmp, 2, 2) {
	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	return ylacreate_dbl(strcmp(ylasym(ylcar(e)).sym,
				    ylasym(ylcadr(e)).sym));
} YLENDNF(strcmp)


YLDEFNF(end_with, 2, 2) {
	unsigned int  strsz, subsz;
	const char   *pstr, *psub;

	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	pstr = ylasym(ylcar(e)).sym;
	psub = ylasym(ylcadr(e)).sym;
	strsz = strlen(pstr);
	subsz = strlen(psub);

	/* filter trivial case */
	if (0 == subsz || 0 == strsz || subsz > strsz)
		return ylnil();

	if (0 == memcmp(pstr+(strsz-subsz), psub, subsz))
		return ylt();
	else
		return ylnil();
} YLENDNF(end_with)


YLDEFNF(start_with, 2, 3) {
	unsigned int  strsz, subsz;
	const char   *pstr, *psub;
	int           fromi;

	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && ylais_type(ylcadr(e), ylaif_sym()));

	if (2 == pcsz)
		fromi = 0;
	else {
		ylnfcheck_parameter(ylais_type(ylcaddr(e), ylaif_dbl()));
		fromi = (int)yladbl(ylcaddr(e));
	}

	pstr = ylasym(ylcar(e)).sym;
	psub = ylasym(ylcadr(e)).sym;
	strsz = strlen(pstr);
	subsz = strlen(psub);

	/* filter trivial case */
	if (0 == subsz || 0 == strsz || subsz > strsz)
		return ylnil();

	/* check index range */
	if (0 > fromi || fromi >= strsz) {
		ylnflogW ("invalid index value : %d\n", fromi);
		return ylnil();
	}

	if (0 == memcmp(pstr+fromi, psub, subsz))
		return ylt();
	else
		return ylnil();
} YLENDNF(start_with)

YLDEFNF(index_of, 2, 3) {
	unsigned int  strsz, subsz;
	const char   *pstr, *psub;
	int           fromi;

	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && ylais_type(ylcadr(e), ylaif_sym()));

	if (2 == pcsz)
		fromi = 0;
	else {
		ylnfcheck_parameter(ylais_type(ylcaddr(e), ylaif_dbl()));
		fromi = (int)yladbl(ylcaddr(e));
	}

	pstr = ylasym(ylcar(e)).sym;
	psub = ylasym(ylcadr(e)).sym;
	strsz = strlen(pstr);
	subsz = strlen(psub);

	/* filter trivial case */
	if (0 == subsz || 0 == strsz || subsz > strsz
	    || fromi > (strsz - subsz))
		return ylnil();
	if (fromi < 0)
		fromi = 0;

	{ /* just scope */
		int         remainsz = strsz - fromi;
		const char* p = pstr + fromi;
		while (remainsz >= subsz) {
			if (*p == *psub
			    && 0 == memcmp(p, psub, subsz))
				break;
			p++;
			remainsz--;
		}
		if (remainsz >= subsz)
			return ylacreate_dbl(strsz-remainsz);
		else
			return ylnil();
	}
} YLENDNF(index_of)


YLDEFNF(last_index_of, 2, 3) {
	unsigned int  strsz, subsz;
	const char   *pstr, *psub;
	int           fromi;

	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && ylais_type(ylcadr(e), ylaif_sym()));

	if (2 == pcsz)
		fromi = 0xfffffff; /* set to bit enough */
	else {
		ylnfcheck_parameter(ylais_type(ylcaddr(e), ylaif_dbl()));
		fromi = (int)yladbl(ylcaddr(e));
	}

	pstr = ylasym(ylcar(e)).sym;
	psub = ylasym(ylcadr(e)).sym;
	strsz = strlen(pstr);
	subsz = strlen(psub);

	/* filter trivial case */
	if (0 == subsz || 0 == strsz || subsz > strsz
	   || fromi < (subsz-1))
		return ylnil();
	if (fromi > strsz-1)
		fromi = strsz-1;

	{ /* just scope */
		/* BE CAREFUL : +1 is essential! */
		const char* p = pstr + fromi - subsz + 1;
		while (p >= pstr) {
			if (*p == *psub
			    && 0 == memcmp(p, psub, subsz))
				break;
			p--;
		}
		if (p >= pstr)
			return ylacreate_dbl((int)(p - pstr));
		else
			return ylnil();
	}
} YLENDNF(last_index_of)


YLDEFNF(replace, 3, 3) {

#define __check_buf(sz)                                                 \
	while ((pbend-pb) < (sz)) {					\
		/* +1 for tailing 0 */					\
		char* tmp = ylmalloc((pbend-pbuf)*2 + 1);		\
		memcpy(tmp, pbuf, pb-pbuf);				\
		ylfree(pbuf);						\
		/* exlude space for tailing 0 */			\
		pbend = tmp + (pbend-pbuf)*2;				\
		pb = tmp + (pb - pbuf);					\
		pbuf = tmp;						\
	}

	const char  *p, *pstr, *pold, *pnew, *pe;
	char        *pbuf, *pb, *pbend;
	unsigned int lenstr, lennew, lenold;
	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	pstr = p = ylasym(ylcar(e)).sym;
	lenstr = strlen(p);
	pold = ylasym(ylcadr(e)).sym;
	lenold = strlen(pold);
	pnew = ylasym(ylcaddr(e)).sym;
	lennew = strlen(pnew);

	/* filter trivial case */
	if (0 == lenold || lenstr < lenold) {
		/* nothing to replace! copy it and return! */
		pbuf = ylmalloc(lenstr+1); /* =1 for tailing NULL */
		strcpy(pbuf, p);
	} else {
		pe = pstr + lenstr - lenold + 1;
		/* initial size of buffer is same with string length */
		pb = pbuf = ylmalloc(lenstr + 1);
		pbend = pb + lenstr; /* exclude space for trailing 0 */
		while (p < pe) {
			if (0 == memcmp(p, pold, lenold)) {
				__check_buf(lennew);
				memcpy(pb, pnew, lennew);
				pb += lennew; p += lenold;
			} else {
				*pb = *p;
				pb++;
				p++;
			}
		}
		{ /* just scope */
			unsigned int remainsz = pstr - p + lenstr;
			__check_buf(remainsz);
			memcpy(pb, p, remainsz);
			pb += remainsz;
		}
		*pb = 0; /* add trailing NULL */
	}
	return ylacreate_sym(pbuf);

#undef __check_buf
} YLENDNF(replace)

YLDEFNF(substring, 2, 3) {
	long long    bi, ei; /* begin index end index */
	const char*  pstr;
	unsigned int lenstr;


	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && ylais_type(ylcadr(e), ylaif_dbl())
			    && (pcsz < 3
				|| ylais_type(ylcaddr(e), ylaif_dbl())) );

	pstr = ylasym(ylcar(e)).sym;
	lenstr = strlen(pstr);

	bi = (long long)yladbl(ylcadr(e));
	if (bi < 0)
		bi = 0;

	if (yleis_nil(ylcddr(e)))
		ei = lenstr;
	else {
		ei = (long long)yladbl(ylcaddr(e));
		if (ei > lenstr)
			ei = lenstr;
	}

	if (bi > ei)
		bi = ei;

	{ /* just scope */
		char*   tmp;

		/* make sub string */
		tmp = ylmalloc(ei-bi+1); /* +1 for tailing 0 */
		tmp[ei-bi] = 0; /* add trailing 0 */
		memcpy(tmp, pstr+bi, ei-bi);

		return ylacreate_sym(tmp);
	}
} YLENDNF(substring)

YLDEFNF(to_lower_case, 1, 1) {
	const char   *p, *pe;
	char         *pbuf, *pb;
	int           delta;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	p = ylasym(ylcar(e)).sym;
	pe = p + strlen(p);

	pb = pbuf = ylmalloc(pe - p + 1);
	pbuf[pe-p] = 0; /* add trailing 0 */

	delta = 'a' - 'A';
	while (p < pe) {
		if ('A' <= *p && 'Z' >= *p)
			*pb = *p + delta;
		else
			*pb = *p;
		pb++;
		p++;
	}

	return ylacreate_sym(pbuf);
} YLENDNF(to_lower_case)

YLDEFNF(to_upper_case, 1, 1) {
	const char   *p, *pe;
	char         *pbuf, *pb;
	int           delta;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	p = ylasym(ylcar(e)).sym;
	pe = p + strlen(p);

	pb = pbuf = ylmalloc(pe - p + 1);
	pbuf[pe-p] = 0; /* add trailing 0 */

	delta = 'A' - 'a';
	while (p < pe) {
		if ('a' <= *p && 'z' >= *p)
			*pb = *p + delta;
		else
			*pb = *p;
		pb++;
		p++;
	}

	return ylacreate_sym(pbuf);
} YLENDNF(to_upper_case)

YLDEFNF(trim, 1, 1) {
	const char    *ps, *pe, *p, *pend; /* p start / pend */
	char*          pbuf;

#define __is_ws(c) (' ' == (c) || '\n' == (c) || '\r' == (c) || '\t' == (c))

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	p = ylasym(ylcar(e)).sym;
	pend = p + strlen(p);

	/* check leading white space */
	ps = p;
	while (ps < pend) {
		if (!__is_ws(*ps))
			break;
		ps++;
	}

	/* check trailing white space */
	pe = pend;
	while (pe >= p) {
		pe--;
		if (!__is_ws(*pe))
			break;
	}

	if (ps > pe)
		pe = ps - 1; /* check : all are white space */

	/* pe is inclusive(+1) and trailing 0(+1) */
	pbuf = ylmalloc(pe - ps + 2);
	pbuf[pe - ps + 1] = 0; /* add trailing 0 */

	memcpy(pbuf, ps, pe-ps+1);

	return ylacreate_sym(pbuf);

#undef __is_ws
} YLENDNF(trim)
