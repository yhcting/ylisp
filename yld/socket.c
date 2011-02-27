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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#include "yld.h"

struct _stsock {
	int    ld; /* listen socket descriptor */
	int    cd; /* connected descriptor */
};

static void
_init_stsock(sock_t s) {
	s->ld = -1;
}

sock_t
sock_init(int port) {
	sock_t	s = malloc(sizeof(struct _stsock));
	if (!s) {
		printf("Out of memory\n");
		goto bail;
	}

	_init_stsock(s);

	if (0 > port || port > 65535) {
		printf("Invalid port number\n");
		goto bail;
	}

	/* create socket */
	if (0 > (s->ld = socket(AF_INET, SOCK_STREAM, 0))) {
		printf("Fail to create socket\n"
		       "    %s\n", strerror(errno));
		goto bail;
	}

	{ /* Just Scope */
		struct sockaddr_in     saddr;
		struct hostent*	       he;

		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(port);
		he = gethostbyname("localhost");
		memcpy(&saddr.sin_addr.s_addr, he->h_addr, he->h_length);
		if (0 > bind(s->ld, (struct sockaddr*)&saddr, sizeof(saddr))) {
			printf("Fail to binding socket to localhost\n"
			       "    %s\n", strerror(errno));
			goto bail;
		}
	}

	if (0 > listen(s->ld, 1)) {
		printf("Fail to listen socket\n"
		       "    %s\n", strerror(errno));
		goto bail;
	}

	if (0 > (s->cd = accept(s->ld, NULL, NULL))) {
		printf("Fail to accept\n"
		       "    %s\n", strerror(errno));
		goto bail;
	}

	return s;

 bail:
	if (s->ld >= 0)
		close(s->ld);
	if (s)
		free(s);
	return NULL;
}

int
sock_deinit(sock_t s) {
	if (s) {
		if (s->cd >= 0) {
			shutdown(s->cd, 2);
			close(s->cd);
		}
		if (s->ld >= 0)
			close(s->ld);
		free(s);
	}
	return 0;
}

int
sock_recv(sock_t s, void* user,
	  int (*rcv)(void*, unsigned char*, unsigned int)) {
	int		 ret = 0;
	unsigned char*	 rcvb = NULL; /* receive buffer */

	assert(s && rcv);

#define __check_recv()							\
	do {								\
		if (rb < 0) {						\
			printf("Fail to receive from accepted socket\n" \
			       "    %s\n", strerror(errno));		\
			ret = -1;					\
			goto done;					\
		} else if (!rb) {					\
			/* peer performs shudown */			\
			ret = 0;					\
			goto done;					\
		}							\
	} while (0)

	/*
	 * Protocal
	 * [size(4bytes)][data...]
	 * @size : sizeof pure data. (exclude self)
	 */
	while (1) {
		u4    sz;
		int   rb; /* received bytes */
		rcvb = NULL;
		rb = recv(s->cd, &sz, 4, 0);
		__check_recv();
		if (rb < 4) {
			printf("Unsupported protocoal!\n");
			ret = -1;
			goto done;
		} else {
			sz = ntohl(sz);
			rcvb = malloc(sz);
			if (!rcvb) {
				printf("Out of memory\n");
				ret = -1;
				goto done;
			}

			rb = recv(s->cd, rcvb, sz, 0);
			__check_recv();
			if (rb != sz) {
				printf(
"Size of receiving data is smaller than the one described at header\n"
				       );
				ret = -1;
				goto done;
			}
			if (0 >= (*rcv)(user, rcvb, sz)) {
				/*
				 * 'rcvb' is alread passed to 'rcv'.
				 * Prevent rcvb from duplicated 'free'
				 */
				rcvb = NULL;
				/* end of communication */
				ret = 0;
				goto done;
			}
		}
	}

#undef __check_recv

 done:
	if (rcvb)
		free(rcvb);
	return ret;
}

int
sock_send(sock_t s, unsigned char* b, unsigned int bsz) {
	{ /* Just Scope */
		u4 nsz; /* network value of bsz */
		nsz = htonl(bsz);
		if (0 > send(s->cd, &nsz, sizeof(u4), 0))
			return -1; /* fail to send */
	}
	{ /* Just Scope */
		unsigned int bs = 0;
		while (bs < bsz)
			bs += send(s->cd, b+bs, bsz-bs, 0);
	}
	return bsz;
}
