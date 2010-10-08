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
 * Public header for interpreter users
 */

#ifndef ___YLISp_h___
#define ___YLISp_h___


typedef enum {
    YLOk,
    YLErr_unknown,

    /* fail to initialization */
    YLErr_init,

    YLErr_out_of_memory,

    YLErr_internal,

    YLErr_io, /* io error */
    
    YLErr_force_stopped,

    YLErr_under_interpreting,

    YLErr_cnf_register,

    YLErr_syntax_unknown,

    YLErr_syntax_escape,

    YLErr_syntax_parenthesis,

    YLErr_syntax_single_quote,

    /*
     * single quoted symbol/yllist SHOULD NOT be evaluated.
     */
    YLErr_eval_squoted,

    /* 
     * LErr_eval_xxx : 
     *    from starting evaluation to before doing functional operation 
     */
    /* evaluated number is out of range */
    YLErr_eval_range,

    /* ylinterpret_undefined symbol */
    YLErr_eval_undefined,

    /* first element of yllist should be function */
    YLErr_eval_function_expected,
    
    /* assert false! */
    YLErr_eval_assert,

    /* 
     * LErr_func_xxx : 
     *    error inside function. - function specific.
     */
    YLErr_func_invalid_param,

    /* fail to execute native function */
    YLErr_func_fail,

} ylerr_t;

typedef enum {
    YLLogV = 0,  /* verbose */
    YLLogD,      /* devleop */
    YLLogI,      /* information */
    YLLogW,      /* warning */
    YLLogE,      /* error */
    YLLogLV_NUM
} ylloglv_t;


/**
 * All members are mendatory!
 */
typedef struct {
    /* function for logging */
    void       (*log)(int loglv, const char* format, ...);

    /* function for print output - printf like */
    int        (*print)(const char* format, ...);

    /* assert. "assert(0)" means, "module meent unrecoverable error!" */
    void       (*assert)(int);

    /* memory allocation - to get centralized control about memory statistic */
    void*      (*malloc)(unsigned int);

    /* memory free */
    void       (*free)(void*);

    /* interpreter memory pool size*/
    unsigned int  mpsz;
} ylsys_t; /* system parameter  */

/* 
 * this SHOULD BE called firstly before using module.
 */
extern ylerr_t
ylinit(ylsys_t* sysv);

/*
 * clean ylisp structure
 */
extern void
yldeinit();

/**
 * !! IMPORTANT !!
 *    'ylinterpret' & 'ylforce_stop' can be at different thread context.
 *    Calling 'ylforce_stop' can stop 'interpret'
 *    BUT, 'ylinterpret' or 'ylforce_stop' itself CANNOT BE concurrent!
 *    (Behavior is NOT DEFINED!)
 */

extern ylerr_t
ylinterpret(const char* stream, unsigned int streamsz);

/*
 * interrupt current interpreting.
 */
extern void
ylforce_stop();

/**
 * get more symbols to make longest prefix.
 * @return: 
 *    0 : success and we meet the branch. 
 *    1 : success and meet the leaf. 
 *    2 : cannot find node which matchs prefix.
 *    <0 : error (ex. not enough buffer size)
 */
extern int
ylsym_auto_complete(const char* start_with, char* buf, unsigned int bufsz);

/**
 * @max_symlen: [out] max symbol length of candidates(based on 'sizeof(char)' - excluding prefix.
 * @return: <0 : internal error.
 */
extern int
ylsym_nr_candidates(const char* start_with, unsigned int* max_symlen);

/**
 * @return: <0: error. Otherwise number of candidates found.
 */
extern int
ylsym_candidates(const char* start_with, 
                 char** ppbuf,       /* in/out */
                 unsigned int ppbsz, /* size of ppbuf - regarding 'ppbuf[i]' */
                 unsigned int pbsz); /* size of pbuf - regarding 'ppbuf[0][x]' */

#endif /* ___YLISp_h___ */
