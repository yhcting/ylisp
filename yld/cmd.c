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

#include "yld.h"

static int
_cmd_autocomp (const unsigned char* data, unsigned int sz) {
        int            r;
        yldynb_t       b;     /* response data buffer */
        yldynb_t       pref;
        const char*    cmd;
        yldynbstr_init (&b, 4096);
        yldynb_init (&pref, 4096);
        yldynb_append (&pref, data, sz);
        yldynb_append (&pref, (unsigned char*)"", 1); /* add trailing 0 */

        r = ylsym_auto_complete ((char*)yldynb_buf (&pref), (char*)yldynb_buf (&b), yldynb_freesz (&b));
        yldynb_setsz (&b, strlen ((char*)yldynbstr_string (&b)) + 1); /* +1 for tailing 0 */

        switch (r) {
        case 0: {
                if(yldynbstr_len (&b) > 0) {
                        cmd = CMD_AUTOCOMP_MORE;
                } else {
                        /* candidatess */
                        /* we need to retrieve candidates.. */
                        int            num, i;
                        unsigned int   maxlen;
                        char**         pp;
                        num = ylsym_nr_candidates ((char*)yldynb_buf (&pref), &maxlen);
                        assert (num > 1);
                        pp = malloc (sizeof (char*) * num);
                        if (!pp) {
                                printf ("Fail to alloc memory : %d\n", num);
                                assert (0);
                        }
                        for (i=0; i<num; i++) {
                                pp[i] = malloc (maxlen+1);
                                if (!pp[i]) {
                                        printf ("Fail to alloc memory : %d\n", maxlen+1);
                                        assert (0);
                                }
                        }
                        i = ylsym_candidates((char*)yldynb_buf (&pref), pp, num, maxlen+1);
                        assert (i==num);

                        yldynbstr_reset (&b);
                        for(i=0; i<num; i++) {
                                yldynbstr_append (&b, "%s%s\n", yldynb_buf (&pref), pp[i]);
                                free (pp[i]);
                        }
                        free (pp);
                        cmd = CMD_AUTOCOMP_PRINT;
            }
        } break;

        case 1: {
                cmd = CMD_AUTOCOMP_COMP;
                yldynbstr_reset (&b);
        } break;

        case 2: {
                cmd = CMD_AUTOCOMP_PRINT;
                yldynbstr_reset (&b); /* nothing to print */
        } break;

        default:
                printf ("Internal error to try auto-completion. Out of memory?\n");
                assert (0);
        }

        { /* Just scope */
                yldynb_t* pdu = &pref; /* pref is used. so reuse it as pdu buffer */
                if (0 > pdu_build (pdu, (unsigned char*) cmd, yldynb_buf (&b), yldynbstr_len (&b)))
                        goto bail;

                if (0 > send_response (yldynb_buf (pdu), yldynb_sz (pdu)))
                        goto bail;
        }

        
        yldynb_clean (&b);
        yldynb_clean (&pref);
        return 0;

 bail:
        yldynb_clean (&b);
        yldynb_clean (&pref);
        
        return -1;
}

static int
_cmd_change_loglv (const unsigned char* data, unsigned int sz) {
        printf ("Command : Change log level\n");
        return 0;
}

static int
_cmd_interp (const unsigned char* data, unsigned int sz) {
        ylinterpret (data, sz);
        return 0;
}

static struct {
        char*          cmd;
        int         (* func) (const unsigned char*, unsigned int);
} _cmdtbl[] = {
        { CMD_AUTOCOMP,     &_cmd_autocomp },
        { CMD_CHGLOGLV,     &_cmd_change_loglv },
        { CMD_INTERP,       &_cmd_interp }
};

int
cmd_do (const unsigned char* cmd,
        const unsigned char* data,
        unsigned int         sz) {
        int i;
        for (i=0; i < arrsz (_cmdtbl); i++)
                if (0 == strcmp (_cmdtbl[i].cmd, (char*)cmd))
                        return (*_cmdtbl[i].func) (data, sz);
        printf ("Invalid command : %s\n", cmd);
        return -1;
}
