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

/*
 * Main parser...
 * Simple FSA(Finite State Automata) is used to parse syntax.
 */

#include <string.h>
#include "lisp.h"



/* =====================================
 *
 * State Automata
 *
 * =====================================*/
/*
 * Common values of all state
 */
struct _fsa {
	yle_t		     sentinel;

	/* current expr (usually pair) */
	yle_t*		     pe;

	/* element buffer - start and (end+1) position.*/
	unsigned char	    *b, *bend;

	/* current position in buffer */
	unsigned char*	     pb;

	/* stream / (end+1) of stream(pe) */
	const unsigned char *s, *send;

	/* variable - passed from caller */
	int*		     line;

	/* state stack */
	ylstk_t*	     ststk;

	/* prev pair expression stack */
	ylstk_t*	     pestk;
}; /* Finite State Automata - This is Automata context. */

struct _fsas {
	const char*  name;  /* state name. usually used for debugging */
	void	     (*enter)	     (yletcxt_t*,
				      const struct _fsas*, struct _fsa*);
	void	     (*come_back)    (yletcxt_t*,
				      const struct _fsas*, struct _fsa*);
	/*
	 * @struct _fsas**:	next state. NULL menas 'exit current state'
	 * @return: TRUE if character is handled. Otherwise FALSE.
	 */
	int	     (*next_char)    (yletcxt_t*,
				      const struct _fsas*, struct _fsa*,
				      char, const struct _fsas**);
	void	     (*exit)	     (yletcxt_t*,
				      const struct _fsas*, struct _fsa*);
}; /* FSA State */


#define _DECL_FSAS_FUNC(nAME)						\
	static void _fsas_##nAME##_enter(yletcxt_t*,			\
					 const struct _fsas*, struct _fsa*); \
	static void _fsas_##nAME##_come_back(yletcxt_t*,		\
					     const struct _fsas*,	\
					     struct _fsa*);		\
	static int  _fsas_##nAME##_next_char(yletcxt_t*,		\
					     const struct _fsas*,	\
					     struct _fsa*,		\
					     char,			\
					     const struct _fsas**);	\
	static void _fsas_##nAME##_exit(yletcxt_t*,			\
					const struct _fsas*, struct _fsa*);

_DECL_FSAS_FUNC(init)
_DECL_FSAS_FUNC(list)
_DECL_FSAS_FUNC(squote)
_DECL_FSAS_FUNC(symbol)
_DECL_FSAS_FUNC(dquote)
_DECL_FSAS_FUNC(comment)
_DECL_FSAS_FUNC(escape)

#undef _DECL_FSAS_FUNC


/* vvvvvvvvvvvvvvvvvvvvvvvvvv Transition table vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
Notation :
    <> : chracter is not handled.
    X : error
    - : not changed
    E : exit state

===============================================================================
state	 "	other	 '	\      (      )	    ;	    \s
-------------------------------------------------------------------------------
init	 dquo	<symb>	 squo	*esca  list   X	     comm   -
list	 dquo	<symb>	 squo	*esca  list   E	     comm   -
squote	 dquo	<symb>	 squo	*esca  list   <E>    <E>    <E>
symbol	 <E>	-	 <E>	esca   <E>    <E>    <E>    <E>
dquote	 E	-	 -	esca   -      -	     -	    -
comment	 -	-	 -	-      -      -	     -	    E(\n)
escape	 E	E	 X	E      X      X	     X	    X
===============================================================================

Exceptional case
    *esca : enter symbol state and then enter to escape state. (2 steps)
	   (escape state is only for symbol.
	    And, exiting from escape state should return valid symbol
	      or part of symbol)

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/


static const  struct _fsas
/*
 * Dummy state to represent "state is not changed.".
 */
_fsas_nochg = { "nochg", NULL, NULL, NULL, NULL },

/*
 * Dummy state to represent "exit from current state".
 */
_fsas_exit = { "exit", NULL, NULL, NULL, NULL },

/*
 * Initial state.
 */
_fsas_init = {
	"init",
	&_fsas_init_enter,
	&_fsas_init_come_back,
	&_fsas_init_next_char,
	&_fsas_init_exit
},

/*
 * In symbol
 */
_fsas_symbol = {
	"symbol",
	&_fsas_symbol_enter,
	&_fsas_symbol_come_back,
	&_fsas_symbol_next_char,
	&_fsas_symbol_exit
},

/*
 * In double quota
 */
_fsas_dquote = {
	"dquote",
	&_fsas_dquote_enter,
	&_fsas_dquote_come_back,
	&_fsas_dquote_next_char,
	&_fsas_dquote_exit
},

/*
 *  In comment
 */
_fsas_comment = {
	"comment",
	&_fsas_comment_enter,
	&_fsas_comment_come_back,
	&_fsas_comment_next_char,
	&_fsas_comment_exit
},

/*
 *  In escape
 */
_fsas_escape = {
	"escape",
	&_fsas_escape_enter,
	&_fsas_escape_come_back,
	&_fsas_escape_next_char,
	&_fsas_escape_exit
},

/*
 *  In list
 */
_fsas_list = {
	"list",
	&_fsas_list_enter,
	&_fsas_list_come_back,
	&_fsas_list_next_char,
	&_fsas_list_exit
},

/*
 *  In single quote
 */
_fsas_squote = {
	"squote",
	&_fsas_squote_enter,
	&_fsas_squote_come_back,
	&_fsas_squote_next_char,
	&_fsas_squote_exit
};



/* =====================================
 * Tool Functions
 * =====================================*/
static inline void
_fsa_add_symchar(struct _fsa* fsa, char c) {
	if (fsa->pb < (fsa->bend-1)) {
		*fsa->pb = c;
		fsa->pb++;
	} else
		ylinterp_fail(YLErr_internal,
			      "Syntax Error : too long symbol!!!!!");
}

static inline yle_t*
_create_atom_sym(unsigned char* sym, unsigned int len) {
	char* str = ylmalloc(sizeof(char)*(len+1));
	if (!str)
		ylinterp_fail(YLErr_out_of_memory,
			      "Out Of Memory : [%d]!\n",
			      len);
	memcpy(str, sym, len);
	str[len] = 0; /* ylnull-terminator */
	return ylacreate_sym(str);
}

static inline void
_eval_exp(yletcxt_t* cxt, yle_t* e) {
	yle_t* r;
	dbg_gen(yllogD(">>>>> Eval exp:\n"
		       "    %s\n", ylechain_print(ylethread_buf(cxt), e)););
	r = yleval (cxt, e, ylnil ());
	if (YLMode_repl == ylmode ())
		ylprint("%s\n", ylechain_print(ylethread_buf (cxt), r));
}

/* =====================================
 * State Functions
 * =====================================*/


static void
_fsas_init_enter(yletcxt_t* cxt,
		 const struct _fsas* fsas, struct _fsa* fsa) {
	/*
	 * Initialise sentinel.
	 * At first, set invalid(NULL) car/cdr
	 * If not, ylpassign try to unref car/cdr which is invalid address.
	 */
	yleset_type(&fsa->sentinel, YLEPair); /* type of sentinel is 'pair' */
	fsa->sentinel.u.p.car = fsa->sentinel.u.p.cdr = NULL;

	fsa->pe = &fsa->sentinel;
}

static void
_fsas_init_come_back(yletcxt_t* cxt,
		     const struct _fsas* fsas, struct _fsa* fsa) {
	/* check that there is expression to evaluate*/
	if (&fsa->sentinel != fsa->pe) {
		dbg_gen(
			yllogD("\n\n\n------ Line : %d -------\n",
			       *fsa->line);
			);
		_eval_exp(cxt, ylcadr(&fsa->sentinel));
	}
	/* unrefer to free dangling block */
	ylpassign(&fsa->sentinel, NULL, NULL);

	/* prepare for new interpretation */
	fsa->pe = &fsa->sentinel;
}

static int
_fsas_init_next_char(yletcxt_t* cxt,
		     const struct _fsas* fsas, struct _fsa* fsa,
		     char c, const struct _fsas** nexts) {
	int  bhandled = TRUE;
	switch(c) {
	case '"':  *nexts = &_fsas_dquote;			 break;
	case '\'': *nexts = &_fsas_squote;			 break;
	case '\\': *nexts = &_fsas_escape;			 break;
	case '(':  *nexts = &_fsas_list;			 break;
	case ')':
		ylinterp_fail(YLErr_syntax_parenthesis,
			      "Syntax Error : parenthesis mismatching\n");
	case ';':  *nexts = &_fsas_comment;			 break;
	case '\r':
	case '\n':
	case '\t':
	case ' ':  *nexts = &_fsas_nochg;			 break;
	default:   *nexts = &_fsas_symbol;    bhandled = FALSE;	 break;
	}
	return bhandled;
}

static void
_fsas_init_exit(yletcxt_t* cxt,
		const struct _fsas* fsas, struct _fsa* fsa) {
	/*
	 * we need to unref memory blocks that is indicated by sentinel
	 * (Usually, both are nil.
	 */
	ylpassign(&fsa->sentinel, NULL, NULL);
}

/* ------------------------------------------ */

static void
_fsas_list_enter(yletcxt_t* cxt,
		 const struct _fsas* fsas, struct _fsa* fsa) {
	yle_t*	pe = NULL;
	/* add new pair node for list */
	yle_t*	pair = ylcons(ylnil(), ylnil());

	ylpsetcdr(fsa->pe, pair);
	/* create sentinel */
	pe = ylcons(ylnil(), ylnil());
	ylpsetcar(pair, pe);

	/* pair is new tail of this list */
	ylstk_push(fsa->pestk, pair);
	fsa->pe = pe;
}

static void
_fsas_list_come_back(yletcxt_t* cxt,
		     const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_list_next_char(yletcxt_t* cxt,
		     const struct _fsas* fsas, struct _fsa* fsa,
		     char c, const struct _fsas** nexts) {
	int  bhandled = TRUE;
	switch(c) {
	case '"':  *nexts = &_fsas_dquote;			  break;
	case '\'': *nexts = &_fsas_squote;			  break;
	case '\\': *nexts = &_fsas_escape;			  break;
	case '(':  *nexts = &_fsas_list;			  break;
	case ')':  *nexts = &_fsas_exit;			  break;
	case ';':  *nexts = &_fsas_comment;			  break;
	case '\r':
	case '\n':
	case '\t':
	case ' ':  *nexts = &_fsas_nochg;			  break;
	default:   *nexts = &_fsas_symbol;    bhandled = FALSE;	  break;
	}
	return bhandled;
}

static void
_fsas_list_exit(yletcxt_t* cxt,
		const struct _fsas* fsas, struct _fsa* fsa) {
	yle_t* pe = ylstk_pop(fsa->pestk);
	/* connect to real expression chain - exclude sentinel */
	ylpsetcar(pe, ylcdar(pe));
	fsa->pe = pe;
}

/* ------------------------------------------ */

static void
_fsas_squote_enter(yletcxt_t* cxt,
		   const struct _fsas* fsas, struct _fsa* fsa) {
	/* add quote and connect with it*/
	yle_t*	 pair = ylcons(ylnil(), ylnil());
	yle_t*	 pe;

	ylpsetcdr(fsa->pe, pair);
	pe = ylcons(ylq(), ylnil());
	ylpsetcar(pair, pe);
	ylstk_push(fsa->pestk, pair);
	fsa->pe = pe;
}

static void
_fsas_squote_come_back(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_squote_next_char(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa,
		       char c, const struct _fsas** nexts) {
	int  bhandled = TRUE;
	switch(c) {
	case '"':  *nexts = &_fsas_dquote;			  break;
	case '\'': *nexts = &_fsas_squote;			  break;
	case '\\': *nexts = &_fsas_escape;			  break;
	case '(':  *nexts = &_fsas_list;			  break;

	case ')':
	case ';':
	case '\r':
	case '\n':
	case '\t':
	case ' ':  *nexts = &_fsas_exit;     bhandled = FALSE;	  break;
	default:   *nexts = &_fsas_symbol;   bhandled = FALSE;	  break;
	}
	return bhandled;
}

static void
_fsas_squote_exit(yletcxt_t* cxt,
		  const struct _fsas* fsas, struct _fsa* fsa) {
	/* in single quote case, sentinel isn't used. */
	fsa->pe = ylstk_pop(fsa->pestk);
}

/* ------------------------------------------ */

static void
_fsas_symbol_enter(yletcxt_t* cxt,
		   const struct _fsas* fsas, struct _fsa* fsa) {
	fsa->pb = fsa->b; /* initialize buffer pointer */
}

static void
_fsas_symbol_come_back(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_symbol_next_char(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa,
		       char c, const struct _fsas** nexts) {
	int	 bhandled = TRUE;
	switch(c) {
	case '"':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	case '\'': *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	case '\\': *nexts = &_fsas_escape;		       break;
	case '(':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	case ')':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	case ';':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	case '\r':
	case '\n':
	case '\t':
	case ' ':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
	default:   *nexts = &_fsas_nochg;		       break;
	}
	if (&_fsas_nochg == *nexts)
		_fsa_add_symchar(fsa, c);
	return bhandled;
}

static void
_fsas_symbol_exit(yletcxt_t* cxt,
		  const struct _fsas* fsas, struct _fsa* fsa) {
	yle_t* pair = ylcons(ylnil(), ylnil());
	yle_t* se = _create_atom_sym(fsa->b, (unsigned int)(fsa->pb - fsa->b));
	ylpsetcar(pair, se);
	ylpsetcdr(fsa->pe, pair);
	/* update previous element pointer */
	fsa->pe = pair;
	fsa->pb = fsa->b;
}

/* ------------------------------------------ */

static void
_fsas_dquote_enter(yletcxt_t* cxt,
		   const struct _fsas* fsas, struct _fsa* fsa) {
	_fsas_symbol_enter(cxt, fsas, fsa);
}

static void
_fsas_dquote_come_back(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_dquote_next_char(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa,
		       char c, const struct _fsas** nexts) {
	int	 bhandled = TRUE;
	switch(c) {
	case '"':  *nexts = &_fsas_exit;	 break;
	case '\\': *nexts = &_fsas_escape;	 break;
	default:   *nexts = &_fsas_nochg;	 break;
	}
	if (&_fsas_nochg == *nexts)
		_fsa_add_symchar(fsa, c);
	return bhandled;
}

static void
_fsas_dquote_exit(yletcxt_t* cxt,
		  const struct _fsas* fsas, struct _fsa* fsa) {
	_fsas_symbol_exit(cxt, fsas, fsa);
}

/* ------------------------------------------ */

static void
_fsas_comment_enter(yletcxt_t* cxt,
		    const struct _fsas* fsas, struct _fsa* fsa) {
}

static void
_fsas_comment_come_back(yletcxt_t* cxt,
			const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_comment_next_char(yletcxt_t* cxt,
			const struct _fsas* fsas, struct _fsa* fsa, char c,
			const struct _fsas** nexts) {
	int	 bhandled = TRUE;
	switch(c) {
	case '\n': *nexts = &_fsas_exit;	 break;
	default:   *nexts = &_fsas_nochg;	 break;
	}
	return bhandled;
}

static void
_fsas_comment_exit(yletcxt_t* cxt,
		   const struct _fsas* fsas, struct _fsa* fsa) {
}

/* ------------------------------------------ */

static void
_fsas_escape_enter(yletcxt_t* cxt,
		   const struct _fsas* fsas, struct _fsa* fsa) {
}

static void
_fsas_escape_come_back(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa) {
}

static int
_fsas_escape_next_char(yletcxt_t* cxt,
		       const struct _fsas* fsas, struct _fsa* fsa,
		       char c, const struct _fsas** nexts) {
	int	     bhandled = TRUE;
	char     ch = c;

	*nexts = &_fsas_exit;
	switch(c) {
	case '"':
	case '\\': break;

		/* "\n" is supported to represent line-feed */
	case 'n':  ch = '\n';	     break;
	default:
		ylinterp_fail(YLErr_syntax_escape,
"Syntax Error : Unsupported character for escape!!\n"
			      );
	}
	_fsa_add_symchar(fsa, ch);
	return bhandled;
}

static void
_fsas_escape_exit(yletcxt_t* cxt,
		  const struct _fsas* fsas, struct _fsa* fsa) {
}

/* ------------------------------------------ */
void*
ylinterp_automata(void* arg) {
	/* Declar space to use */
	ylerr_t		      ret = YLOk;
	const unsigned char*  p;
	const struct _fsas   *st, *nst; /* current st, next st */
	struct _fsa           fsa;
	yletcxt_t            *cxt; /* This Evaluation Thread Context */

	{ /* Just scope */
		struct __interpthd_arg* parg = (struct __interpthd_arg*)arg;
		cxt = parg->cxt;

		/*
		 * Why lock?
		 * See comments at interpret.c : ylinterpret_internal.x
		 */
		_mlock(&cxt->m);
		_munlock(&cxt->m);

		fsa.pb = fsa.b = parg->b;
		fsa.bend = fsa.b + parg->bsz;
		fsa.s = parg->s;
		fsa.send = fsa.s + parg->sz;
		fsa.line = parg->line;
		fsa.ststk = parg->ststk;
		fsa.pestk = parg->pestk;
	}

	p = fsa.s;

	st = &_fsas_init;
	ylstk_push(fsa.ststk, (void*)st);
	(*_fsas_init.enter)(cxt, &_fsas_init, &fsa);

	while (*p) {
		/* yllogV("+++ [%s] : '%c' => ",
		          st->name, ('\n' == *p)? '@': *p); */
		if ((*st->next_char)(cxt, st, &fsa, *p, &nst)) {
			if ('\n' == *p)
				(*fsa.line)++;
			p++;
		}
		/* yllogV1("[%s]", nst->name); */

		if (&_fsas_nochg == nst)
			; /* nothing to do */
		else if (&_fsas_exit == nst) {
			(*st->exit)(cxt, st, &fsa);
			/* pop current state from stack */
			ylstk_pop(fsa.ststk);
			/* move to previous state */
			st = ylstk_peek(fsa.ststk);
			(*st->come_back)(cxt, st, &fsa);
			/* yllogV1(" => [%s]", st->name); */
		} else if (&_fsas_escape == nst) {
			/* special case */
			if ( !(&_fsas_dquote == ylstk_peek(fsa.ststk)
			       || &_fsas_symbol == ylstk_peek(fsa.ststk)) ) {
				/*
				 * enter symbol state firstly.
				 * - 'dquote' state is PRACTICALLY same with
				 *     'symbol' state.
				 *   So, we don't need to handle
				 *     not only 'symbol'
				 *     but also 'dquote' state.
				 */
				ylstk_push(fsa.ststk, (void*)&_fsas_symbol);
				(_fsas_symbol.enter)(cxt, &_fsas_symbol, &fsa);
			}
			ylstk_push(fsa.ststk, (void*)nst);
			(*nst->enter)(cxt, nst, &fsa);
			st = nst;
		} else {
			ylstk_push(fsa.ststk, (void*)nst);
			(*nst->enter)(cxt, nst, &fsa);
			st = nst;
		}
		/* yllogV0("\n"); */

		if (fsa.send == p)
			/*
			 * for easy parsing, automata always adds '\n'
			 *   at the end of stream!!!
			 */
			p = (unsigned char*)"\n";
	}

	st = ylstk_pop(fsa.ststk);
	if (&_fsas_init == st) {
		(*_fsas_init.exit)(cxt, &_fsas_init, &fsa);
		if (0 == ylstk_size(fsa.pestk))
			goto done;
		else {
			yllogE ("Syntax Error!!!!!!\n");
			ret = YLErr_syntax_unknown;
			goto done;
		}
	} else {
		yllogE ("Syntax Error!!!!!!\n");
		ret = YLErr_syntax_unknown;
		goto done;
	}

 done:
	return (void*)ret;
}

