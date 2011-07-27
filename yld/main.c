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

#include <pthread.h>
#include <stdarg.h>

#include "yld.h"

#define _DEFAULT_PORT 9923

static sock_t _s = NULL;



/* ===========================
 * Socket interface wrapper
 * ===========================*/
int
send_response(unsigned char* b, unsigned int bsz) {
	return sock_send(_s, b, bsz);
}



/* ===========================
 * Packet Handling
 * ===========================*/

struct _packet {
	unsigned char* data;
	unsigned int   sz;
};

void*
_packet_handler(void* arg) {
	struct _packet*	  p = (struct _packet*) arg;
	yldynb_t	  cmd;
	yldynb_t	  data;
	assert(p);
	yldynb_init(&cmd,  256);
	yldynb_init(&data, 4096);

	if (0 > pdu_parser(&cmd, &data, p->data, p->sz))
		goto done;

	if (0 > cmd_do(yldynb_buf(&cmd), yldynb_buf(&data), yldynb_sz(&data)))
		goto done;

 done:
	yldynb_clean(&cmd);
	yldynb_clean(&data);
	free(p->data);
	free(p);
	return NULL;
}


static int
_on_recv(void* user, unsigned char* d, unsigned int sz) {
	int		r;
	pthread_t	thd;
	struct _packet* p = malloc(sizeof(*p));
	assert(p);
	p->data = d;
	p->sz	= sz;

	r = pthread_create(&thd, NULL, &_packet_handler, p);
	if (r) {
		printf("Fail to create packet handler thread\n"
			"    =>%s\n", strerror(errno));
		exit(0);
	}
	return 1;
}


/* ===========================
 * YLISP
 * ===========================*/

static int _loglv = YLLogW;

static int
__print(const char* cmd, const char* format, va_list ap) {
	yldynb_t  d;
	yldynb_t  pdu;
	int	  r = -1;

	yldynb_init(&d, 4096);
	yldynb_init(&pdu, 4096);

	while (1) {
		r = vsnprintf((char*)yldynb_buf(&d),
			      yldynb_freesz(&d),
			      format,
			      ap);
		if (r < 0)
			goto bail;
		if (r < yldynb_limit(&d))
			break; /* success */
		yldynb_expand(&d);
	}
	d.sz = r;

	if (0 > pdu_build(&pdu,
			  (unsigned char*)cmd,
			  yldynb_buf(&d),
			  yldynb_sz(&d)))
		goto bail;

	if (0 > sock_send(_s, yldynb_buf(&pdu), yldynb_sz(&pdu)))
		goto bail;

	yldynb_clean(&pdu);
	yldynb_clean(&d);
	return r;

 bail:
	yldynb_clean(&pdu);
	yldynb_clean(&d);
	return -1;
}

static int
_print(const char* format, ...) {
	int	r;
	va_list ap;
	va_start(ap, format);
	r = __print(CMD_PRINT, format, ap);
	if (0 > r)
		printf("PRINT fails\n");
	va_end(ap);
	return r;
}

static void
_log(int lv, const char* format, ...) {
	int	r;
	va_list ap;

	if (lv < _loglv)
		return;
	va_start(ap, format);
	r = __print(CMD_LOG, format, ap);
	if (0 > r)
		printf("LOG fails\n");
	va_end(ap);
	return;
}

static void
_assert_(int a) {
#define __MSG "ASSERT!!!"
	if (!a) {
		sock_send(_s,(unsigned char*)__MSG, sizeof(__MSG));
		assert(0);
	}
#undef __MSG
}

#ifdef CONFIG_STATIC_CNF
extern void ylcnf_load_ylbase(void);
extern void ylcnf_load_ylext(void);
#endif /* CONFIG_STATIC_CNF */

int
main(int argc, const char* argv[]) {
	int	port = _DEFAULT_PORT;
	if (argc > 1) {
		char* pe;
		port = strtol(argv[0], &pe, 0);
		if (!port && *pe) {
			printf("Invalid port number\n");
			exit(0);
		}
	}
	if (NULL ==(_s = sock_init(port))) {
		printf("Fail to initialize socket\n");
		exit(0);
	}

	{ /* Just scope */
		ylsys_t	  sys;

		/* set system parameter */
		sys.print   = &_print;
		sys.log	    = &_log;
		sys.assert_ = &_assert_;
		sys.malloc  = &malloc;
		sys.free    = &free;
		sys.mode    = YLMode_batch;
		sys.mpsz    = 4*1024;
		sys.gctp    = 80;

		if (YLOk != ylinit(&sys)) {
			printf("Fail to initialize ylisp\n");
			exit(1);
		}

#ifdef CONFIG_STATIC_CNF
		ylcnf_load_ylbase(NULL);
		ylcnf_load_ylext(NULL);
#endif /* CONFIG_STATIC_CNF */

	} /* Just scope */

	if (0 > sock_recv(_s, NULL, &_on_recv)) {
		printf("Error to receive data from socket.\n");
		exit(0);
	}
	return 0;
}
