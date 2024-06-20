/*
 * tclRegexp.h --
 *
 *	This file contains definitions used internally by Henry Spencer's
 *	regular expression code.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLREGEXP
#define _TCLREGEXP

#include "regex.h"

#ifdef HAVE_PCRE
#include <pcre.h>
#endif

/*
 * The TclRegexp structure encapsulates a compiled regex_t, the flags that
 * were used to compile it, and an array of pointers that are used to indicate
 * subexpressions after a call to Tcl_RegExpExec. Note that the string and
 * objPtr are mutually exclusive. These values are needed by Tcl_RegExpRange
 * in order to return pointers into the original string.
 */

typedef struct {
#ifdef HAVE_PCRE
    int	  *offsets;		/* Storage for array of offsets (indices to handle within PCRE) */
    int    offsCnt;
    size_t offsSize;
    int	  *wrkSpace;		/* Workspace storage vector (used by parsing via DFA). */
    int    wrkSpCnt;		/* Current length of shared workspace storage vector */
    size_t wrkSpSize;
#endif
    regmatch_t *matches;	/* Storage for array of indices into the Tcl_UniChar */
    size_t	matchSize;	/* representation of the last string matched
				 * with this regexp to indicate the location
				 * of subexpressions. */
} TclRegexpStorage;

typedef struct TclRegexp {
    int flags;			/* Regexp compile flags. */
    regex_t re;			/* Compiled re, includes number of
				 * subexpressions. */
#ifdef HAVE_PCRE
    pcre *pcre;			/* PCRE compile re */
    pcre_extra *study;		/* study of PCRE */
#endif
    CONST char *string;		/* Last string passed to Tcl_RegExpExec. */
    Tcl_Obj *objPtr;		/* Last object passed to Tcl_RegExpExecObj. */
    Tcl_Obj *globObjPtr;	/* Glob pattern rep of RE or NULL if none. */
    TclRegexpStorage *reStorage;/* Shared storage for array of indices, matches, workspace etc. */
    rm_detail_t details;	/* Detailed information on match (currently
				 * used only for REG_EXPECT). */
    int refCount;		/* Count of number of references to this
				 * compiled regexp. */
} TclRegexp;

int TclAdjustRegExpFlags(Tcl_Interp *, Tcl_Obj *, int flags);

#endif /* _TCLREGEXP */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
