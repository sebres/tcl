/*
 * tclRegexp.c --
 *
 *	This file contains the public interfaces to the Tcl regular expression
 *	mechanism.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclRegexp.h"

/*
 *----------------------------------------------------------------------
 * The routines in this file use Henry Spencer's regular expression package
 * contained in the following additional source files:
 *
 *	regc_color.c	regc_cvec.c	regc_lex.c
 *	regc_nfa.c	regcomp.c	regcustom.h
 *	rege_dfa.c	regerror.c	regerrs.h
 *	regex.h		regexec.c	regfree.c
 *	regfronts.c	regguts.h
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 *
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., Sun Microsystems Inc., and Scriptics
 * Corporation, none of whom are responsible for the results. The author
 * thanks all of them.
 *
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications.
 *
 * I'd appreciate being given credit for this package in the documentation of
 * software which uses it, but that is not a requirement.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * HENRY SPENCER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * *** NOTE: this code has been altered slightly for use in Tcl: ***
 * *** 1. Names have been changed, e.g. from re_comp to		 ***
 * ***    TclRegComp, to avoid clashes with other 		 ***
 * ***    regexp implementations used by applications. 		 ***
 */

/*
 * Thread local storage used to maintain a per-thread cache of compiled
 * regular expressions.
 */

#define NUM_REGEXPS 30

typedef struct ThreadSpecificData {
    int initialized;		/* Set to 1 when the module is initialized. */
    char *patterns[NUM_REGEXPS];/* Strings corresponding to compiled regular
				 * expression patterns. NULL means that this
				 * slot isn't used. Malloc-ed. */
    int patLengths[NUM_REGEXPS];/* Number of non-null characters in
				 * corresponding entry in patterns. -1 means
				 * entry isn't used. */
    struct TclRegexp *regexps[NUM_REGEXPS];
				/* Compiled forms of above strings. Also
				 * malloc-ed, or NULL if not in use yet. */
    TclRegexpStorage reStorage;	/* TSD global (shared) storage for offsets/matches/workspace */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * Declarations for functions used only in this file.
 */

static void		AllocCaptStorage(TclRegexp *regexpPtr);
static TclRegexp *	CompileRegexp(Tcl_Interp *interp, const char *pattern,
			    int length, int flags);
static void		DupRegexpInternalRep(Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr);
static void		FinalizeRegexp(ClientData clientData);
static void		FreeRegexp(TclRegexp *regexpPtr);
static void		FreeRegexpInternalRep(Tcl_Obj *objPtr);
static int		RegExpExecUniChar(Tcl_Interp *interp, Tcl_RegExp re,
			    const Tcl_UniChar *uniString, int numChars,
			    int nmatches, int flags);
static int		SetRegexpFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

#ifdef HAVE_PCRE
typedef struct {
    int rm_so;		/* start of substring */
    int rm_eo;		/* end of substring */
} regoffs_t;

static void		EnlargeWrkSpaceStorage(TclRegexp *regexpPtr);

#define	VectorCoountPCRE(regexpPtr) \
		((int)(regexpPtr->re.re_nsub+1)*3)

#endif
/*
 * The regular expression Tcl object type. This serves as a cache of the
 * compiled form of the regular expression.
 */

Tcl_ObjType tclRegexpType = {
    "regexp",				/* name */
    FreeRegexpInternalRep,		/* freeIntRepProc */
    DupRegexpInternalRep,		/* dupIntRepProc */
    NULL,				/* updateStringProc */
    SetRegexpFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpCompile --
 *
 *	Compile a regular expression into a form suitable for fast matching.
 *	This function is DEPRECATED in favor of the object version of the
 *	command.
 *
 * Results:
 *	The return value is a pointer to the compiled form of string, suitable
 *	for passing to Tcl_RegExpExec. This compiled form is only valid up
 *	until the next call to this function, so don't keep these around for a
 *	long time! If an error occurred while compiling the pattern, then NULL
 *	is returned and an error message is left in the interp's result.
 *
 * Side effects:
 *	Updates the cache of compiled regexps.
 *
 *----------------------------------------------------------------------
 */

Tcl_RegExp
Tcl_RegExpCompile(
    Tcl_Interp *interp,		/* For use in error reporting and to access
				 * the interp regexp cache. */
    const char *pattern)	/* String for which to produce compiled
				 * regular expression. */
{
    return (Tcl_RegExp) CompileRegexp(interp, pattern, (int) strlen(pattern),
	    REG_ADVANCED);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpExec --
 *
 *	Execute the regular expression matcher using a compiled form of a
 *	regular expression and save information about any match that is found.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is returned
 *	and the interp's result contains an error message. Otherwise the
 *	return value is 1 if a matching range is found and 0 if there is no
 *	matching range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpExec(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tcl_RegExp re,		/* Compiled regular expression; must have been
				 * returned by previous call to
				 * Tcl_GetRegExpFromObj. */
    const char *text,		/* Text against which to match re. */
    const char *start)		/* If text is part of a larger string, this
				 * identifies beginning of larger string, so
				 * that "^" won't match. */
{
    int flags, result, numChars;
    TclRegexp *regexp = (TclRegexp *)re;
    Tcl_DString ds;
    const Tcl_UniChar *ustr;

    /*
     * If the starting point is offset from the beginning of the buffer, then
     * we need to tell the regexp engine not to match "^".
     */

    if (text > start) {
	flags = REG_NOTBOL;
    } else {
	flags = 0;
    }

    /*
     * Remember the string for use by Tcl_RegExpRange().
     */

    regexp->string = text;
    regexp->objPtr = NULL;

    /*
     * Convert the string to Unicode and perform the match.
     */

    Tcl_DStringInit(&ds);
    ustr = Tcl_UtfToUniCharDString(text, -1, &ds);
    numChars = Tcl_DStringLength(&ds) / sizeof(Tcl_UniChar);
    result = RegExpExecUniChar(interp, re, ustr, numChars, -1 /* nmatches */,
	    flags);
    Tcl_DStringFree(&ds);

    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_RegExpRange --
 *
 *	Returns pointers describing the range of a regular expression match,
 *	or one of the subranges within the match.
 *
 * Results:
 *	The variables at *startPtr and *endPtr are modified to hold the
 *	addresses of the endpoints of the range given by index. If the
 *	specified range doesn't exist then NULLs are returned.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
Tcl_RegExpRange(
    Tcl_RegExp re,		/* Compiled regular expression that has been
				 * passed to Tcl_RegExpExec. */
    int index,			/* 0 means give the range of the entire match,
				 * > 0 means give the range of a matching
				 * subrange. */
    const char **startPtr,	/* Store address of first character in
				 * (sub-)range here. */
    const char **endPtr)	/* Store address of character just after last
				 * in (sub-)range here. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;
    regmatch_t *matches = regexpPtr->reStorage->matches;
    const char *string;

    if ((size_t) index > regexpPtr->re.re_nsub) {
	*startPtr = *endPtr = NULL;
    } else if (matches[index].rm_so < 0) {
	*startPtr = *endPtr = NULL;
    } else {
	if (regexpPtr->objPtr) {
	    string = TclGetString(regexpPtr->objPtr);
	} else {
	    string = regexpPtr->string;
	}
	*startPtr = Tcl_UtfAtIndex(string, matches[index].rm_so);
	*endPtr = Tcl_UtfAtIndex(string, matches[index].rm_eo);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * RegExpExecUniChar --
 *
 *	Execute the regular expression matcher using a compiled form of a
 *	regular expression and save information about any match that is found.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is returned
 *	and an error message is left in interp's result. Otherwise the return
 *	value is 1 if a matching range was found or 0 if there was no matching
 *	range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
RegExpExecUniChar(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tcl_RegExp re,		/* Compiled regular expression; returned by a
				 * previous call to Tcl_GetRegExpFromObj */
    const Tcl_UniChar *wString,	/* String against which to match re. */
    int numChars,		/* Length of Tcl_UniChar string (must be
				 * >=0). */
    int nmatches,		/* How many subexpression matches (counting
				 * the whole match as subexpression 0) are of
				 * interest. -1 means "don't know". */
    int flags)			/* Regular expression flags. */
{
    int status;
    TclRegexp *regexpPtr = (TclRegexp *) re;
    TclRegexpStorage *reStorage = regexpPtr->reStorage;
    size_t last = regexpPtr->re.re_nsub + 1;
    size_t nm = last;

    if (nmatches >= 0 && (size_t) nmatches < nm) {
	nm = (size_t) nmatches;
    }

    status = TclReExec(&regexpPtr->re, wString, (size_t) numChars,
	    &regexpPtr->details, nm, reStorage->matches, flags);

    /*
     * Check for errors.
     */

    if (status != REG_OKAY) {
	if (status == REG_NOMATCH) {
	    return 0;
	}
	if (interp != NULL) {
	    TclRegError(interp, "error while matching regular expression: ",
		    status);
	}
	return -1;
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclRegExpRangeUniChar --
 *
 *	Returns pointers describing the range of a regular expression match,
 *	or one of the subranges within the match, or the hypothetical range
 *	represented by the rm_extend field of the rm_detail_t.
 *
 * Results:
 *	The variables at *startPtr and *endPtr are modified to hold the
 *	offsets of the endpoints of the range given by index. If the specified
 *	range doesn't exist then -1s are supplied.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclRegExpRangeUniChar(
    Tcl_RegExp re,		/* Compiled regular expression that has been
				 * passed to Tcl_RegExpExec. */
    int index,			/* 0 means give the range of the entire match,
				 * > 0 means give the range of a matching
				 * subrange, -1 means the range of the
				 * rm_extend field. */
    int *startPtr,		/* Store address of first character in
				 * (sub-)range here. */
    int *endPtr)		/* Store address of character just after last
				 * in (sub-)range here. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;
    regmatch_t *matches = regexpPtr->reStorage->matches;

    if ((regexpPtr->flags&REG_EXPECT) && index == -1) {
	*startPtr = regexpPtr->details.rm_extend.rm_so;
	*endPtr = regexpPtr->details.rm_extend.rm_eo;
    } else if ((size_t) index > regexpPtr->re.re_nsub) {
	*startPtr = -1;
	*endPtr = -1;
    } else {
	*startPtr = matches[index].rm_so;
	*endPtr = matches[index].rm_eo;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpMatch --
 *
 *	See if a string matches a regular expression.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is returned
 *	and the interp's result contains an error message. Otherwise the
 *	return value is 1 if "text" matches "pattern" and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpMatch(
    Tcl_Interp *interp,		/* Used for error reporting. May be NULL. */
    const char *text,		/* Text to search for pattern matches. */
    const char *pattern)	/* Regular expression to match against text. */
{
    Tcl_RegExp re;

    re = Tcl_RegExpCompile(interp, pattern);
    if (re == NULL) {
	return -1;
    }
    return Tcl_RegExpExec(interp, re, text, text);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpExecObj --
 *
 *	Execute a precompiled regexp against the given object.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is returned
 *	and the interp's result contains an error message. Otherwise the
 *	return value is 1 if "string" matches "pattern" and 0 otherwise.
 *
 * Side effects:
 *	Converts the object to a Unicode object.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpExecObj(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tcl_RegExp re,		/* Compiled regular expression; must have been
				 * returned by previous call to
				 * Tcl_GetRegExpFromObj. */
    Tcl_Obj *textObj,		/* Text against which to match re. */
    int offset,			/* Character index that marks where matching
				 * should begin. */
    int nmatches,		/* How many subexpression matches (counting
				 * the whole match as subexpression 0) are of
				 * interest. -1 means all of them. */
    int flags)			/* Regular expression execution flags. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;
#ifdef HAVE_PCRE
    TclRegexpStorage *reStorage = regexpPtr->reStorage;
#endif
    int length;
    int reflags = regexpPtr->flags;
#define TCL_REG_GLOBOK_FLAGS (TCL_REG_ADVANCED | TCL_REG_NOSUB | TCL_REG_NOCASE)

    /*
     * Take advantage of the equivalent glob pattern, if one exists.
     * This is possible based only on the right mix of incoming flags (0)
     * and regexp compile flags.
     */
    if ((offset == 0) && (nmatches == 0) && (flags == 0)
	    && !(reflags & ~TCL_REG_GLOBOK_FLAGS)
	    && (regexpPtr->globObjPtr != NULL)) {
	int nocase = (reflags & TCL_REG_NOCASE) ? TCL_MATCH_NOCASE : 0;

	/*
	 * Pass to TclStringMatchObj for obj-specific handling.
	 * XXX: Currently doesn't take advantage of exact-ness that
	 * XXX: TclReToGlob tells us about
	 */

	return TclStringMatchObj(textObj, regexpPtr->globObjPtr, nocase);
    }

    /*
     * Save the target object so we can extract strings from it later.
     */

    regexpPtr->string = NULL;
    regexpPtr->objPtr = textObj;

    if (reflags & TCL_REG_PCRE) {
#ifdef HAVE_PCRE
	const char *matchstr;
	int match, eflags, *offsets, nm;

	nm = reStorage->offsCnt;
	offsets = reStorage->offsets;

#define utfstr 1
#if 0
	/* not implemented for byte-array */
	utfstr = (textObj->typePtr != &tclByteArrayType);
#endif
	if (utfstr) {
	    matchstr = Tcl_GetStringFromObj(textObj, &length);
	    /* OFFS_CHAR2BYTE: convert offset in chars to offset in bytes */
	    if (!(flags & TCL_REG_BYTEOFFS) && offset > 0) {
		Tcl_UniChar ch;
		const char *src = matchstr, *srcend = matchstr + length;

		/* Tcl_UtfAtIndex considering string length */
		while (offset-- > 0 && src < srcend) {
		    src += TclUtfToUniChar(src, &ch);
		}
		if (offset <= 0) {
		    offset = src - matchstr;
		} else {
		    offset = length+1; /* outside of string (and > 0 for empty string) */
		}
	    }
	} else {
	    matchstr = (const char*)Tcl_GetByteArrayFromObj(textObj, &length);
	}

	if (offset > length) {
	    offset = length;
	}
#undef utfstr

	regexpPtr->details.rm_extend.rm_so = offset;

	eflags = PCRE_NO_UTF8_CHECK;
	if (flags & TCL_REG_NOTBOL) {
	    eflags |= PCRE_NOTBOL;
	} if (offset >= length) {
	    /* 
	     * PCRE has currently a bug by multiline with offset after "\n":
	     * ^ - meant assert start of string (or line, in multiline mode),
	     * but it will be not found by offset after "\n" regardless multiline.
	     * Thus just let do a small adustment (shift begin of string to offset).
	     * Not we'll do it always in order to regard enable multiline by exec using `(?m)`.
	     */
	    matchstr = "";
	    offset = 0;
	    length = 0;
	}

	if (!(regexpPtr->flags & TCL_REG_PCDFA)) {
	    match = pcre_exec(regexpPtr->pcre, regexpPtr->study,
		matchstr, length, offset, eflags, offsets, nm);
	} else {
	    //TODO:
	    do {
		match = pcre_dfa_exec(regexpPtr->pcre, regexpPtr->study,
		    matchstr, length, offset, eflags, offsets, nm, 
		    reStorage->wrkSpace, reStorage->wrkSpCnt);
		if (match == PCRE_ERROR_DFA_WSSIZE) {
		    EnlargeWrkSpaceStorage(regexpPtr);
		    continue;
	        }
		if (match) break;
		/* insufficient capture space - enlarge vectors buffer */
		regexpPtr->re.re_nsub = (regexpPtr->re.re_nsub+1)*2;
		AllocCaptStorage(regexpPtr);
		nm = reStorage->offsCnt;
		offsets = reStorage->offsets;
	    } while(1);
	}

	/*
	 * Check for errors.
	 */

	if (match == PCRE_ERROR_NOMATCH) {
	    return 0;
	} else if (match == 0) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp,
			"pcre_exec had insufficient capture space", NULL);
	    }
	    return -1;
	} else if (match < -1) {
	    if (interp != NULL) {
		char buf[32 + TCL_INTEGER_SPACE];
		sprintf(buf, "pcre_exec returned error code %d", match);
		Tcl_AppendResult(interp, buf, NULL);
	    }
	    return -1;
	}

	/*
	 * Adjust match indices relative offset where matching began.
	 */
	if (offset) {
	    int i;
	    for (i = 0; i <= match*2; i++) {
		offsets[i] -= offset;
	    }
	}
	/* TODO: OFFS_BYTE2CHAR not yet implemented for Tcl_RegExpExecObj* /
	/ * OFFS_BYTE2CHAR: convert offset in bytes to offset in chars * /
	if (!(flags & TCL_REG_BYTEOFFS)) {
	    ...
	    see for example OFFS_BYTE2CHAR block in TclRegexpPCRE
	    ...
	}
	*/

	return 1;
#else
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "PCRE not available", NULL);
	}
	return -1;
#endif
    } else {
	Tcl_UniChar *udata;

	udata = Tcl_GetUnicodeFromObj(textObj, &length);

	if (offset > length) {
	    offset = length;
	}
	udata += offset;
	length -= offset;

	return RegExpExecUniChar(interp, re, udata, length, nmatches, flags);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpMatchObj --
 *
 *	See if an object matches a regular expression.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is returned
 *	and the interp's result contains an error message. Otherwise the
 *	return value is 1 if "text" matches "pattern" and 0 otherwise.
 *
 * Side effects:
 *	Changes the internal rep of the pattern and string objects.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpMatchObj(
    Tcl_Interp *interp,		/* Used for error reporting. May be NULL. */
    Tcl_Obj *textObj,		/* Object containing the String to search. */
    Tcl_Obj *patternObj)	/* Regular expression to match against
				 * string. */
{
    Tcl_RegExp re;

    re = Tcl_GetRegExpFromObj(interp, patternObj,
	    TCL_REG_ADVANCED | TCL_REG_NOSUB);
    if (re == NULL) {
	return -1;
    }
    return Tcl_RegExpExecObj(interp, re, textObj, 0 /* offset */,
	    0 /* nmatches */, 0 /* flags */);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpGetInfo --
 *
 *	Retrieve information about the current match.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_RegExpGetInfo(
    Tcl_RegExp regexp,		/* Pattern from which to get subexpressions. */
    Tcl_RegExpInfo *infoPtr)	/* Match information is stored here. */
{
    TclRegexp *regexpPtr = (TclRegexp *) regexp;
    regmatch_t *matches = regexpPtr->reStorage->matches;

    infoPtr->nsubs = regexpPtr->re.re_nsub;
    infoPtr->matches = (Tcl_RegExpIndices *)matches;
    if (regexpPtr->flags & TCL_REG_PCRE) {
#ifdef HAVE_PCRE
    	int *offsets = regexpPtr->reStorage->offsets;
	if ((int *)matches != offsets) {
	    int i;
	    for (i = 0; i <= infoPtr->nsubs; i++) {
		matches[i].rm_so = offsets[i*2];
		matches[i].rm_eo = offsets[i*2+1];
	    }
	}
	infoPtr->extendStart = -1; /* XXX support? */
#else
	Tcl_Panic("Cannot get info for PCRE match");
#endif
    } else {
	infoPtr->extendStart = regexpPtr->details.rm_extend.rm_so;
    }
}


int
TclAdjustRegExpFlags(
    Tcl_Interp *interp,		/* To access the interp regexp default. */
    Tcl_Obj *objPtr,		/* Object whose string rep contains regular
				 * expression pattern. */
    int flags			/* Regular expression compilation flags. */
) {
    /* if type is not explicit specified */
    if (!(flags & TCL_REG_EXPLTYPE)) {
    	int msk;
	/* own re-type from interp */
	if ( (interp != NULL)
	  && (msk = (((Interp *)interp)->flags & (INTERP_PCRE|INTERP_DFA)))
	) {
	    flags |= TCL_REG_PCRE;
	    if (msk & INTERP_DFA) {
		flags |= TCL_REG_PCDFA;
	    } else {
	    	flags &= ~TCL_REG_PCDFA;
	    }
	}
	/* if does not work in PCRE - switch to classic (backwards compatibility) */
	if ((flags & TCL_REG_PCRE)) {
	    const char *regStr = TclGetString(objPtr);
	    if (*regStr == '*' && (objPtr->length >= 4) 
	    	&& (memcmp("***=", regStr, 4) == 0)
	    ) {
		flags = (flags & ~TCL_REG_PCRE) | TCL_REG_EXPLTYPE;
	    }
	}
    }
    return flags;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetRegExpFromObj --
 *
 *	Compile a regular expression into a form suitable for fast matching.
 *	This function caches the result in a Tcl_Obj.
 *
 * Results:
 *	The return value is a pointer to the compiled form of string, suitable
 *	for passing to Tcl_RegExpExec. If an error occurred while compiling
 *	the pattern, then NULL is returned and an error message is left in the
 *	interp's result.
 *
 * Side effects:
 *	Updates the native rep of the Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

Tcl_RegExp
Tcl_GetRegExpFromObj(
    Tcl_Interp *interp,		/* For use in error reporting, and to access
				 * the interp regexp cache. */
    Tcl_Obj *objPtr,		/* Object whose string rep contains regular
				 * expression pattern. Internal rep will be
				 * changed to compiled form of this regular
				 * expression. */
    int flags)			/* Regular expression compilation flags. */
{
    int length;
    TclRegexp *regexpPtr;
    char *pattern;

    /*
     * This is OK because we only actually interpret this value properly as a
     * TclRegexp* when the type is tclRegexpType.
     */

    regexpPtr = (TclRegexp *) objPtr->internalRep.twoPtrValue.ptr1;

    /* if type is not explicit specified */
    if (!(flags & TCL_REG_EXPLTYPE)) {
	/* own re-type from interp, disable PCRE if needed */
	flags = TclAdjustRegExpFlags(interp, objPtr, flags);
    }

    /* 
     * Explicit flag has no meaning further - remove it in order to compare.
     */
    flags &= ~TCL_REG_EXPLTYPE;

    if ((objPtr->typePtr != &tclRegexpType) || (regexpPtr->flags != flags)) {
	pattern = TclGetStringFromObj(objPtr, &length);

	regexpPtr = CompileRegexp(interp, pattern, length, flags);
	if (regexpPtr == NULL) {
	    return NULL;
	}

	/*
	 * Add a reference to the regexp so it will persist even if it is
	 * pushed out of the current thread's regexp cache. This reference
	 * will be removed when the object's internal rep is freed.
	 */

	regexpPtr->refCount++;

	/*
	 * Free the old representation and set our type.
	 */

	TclFreeIntRep(objPtr);
	objPtr->internalRep.twoPtrValue.ptr1 = (void *) regexpPtr;
	objPtr->typePtr = &tclRegexpType;
    }
    return (Tcl_RegExp) regexpPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegAbout --
 *
 *	Return information about a compiled regular expression.
 *
 * Results:
 *	The return value is -1 for failure, 0 for success, although at the
 *	moment there's nothing that could fail. On success, a list is left in
 *	the interp's result: first element is the subexpression count, second
 *	is a list of re_info bit names.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclRegAbout(
    Tcl_Interp *interp,		/* For use in variable assignment. */
    Tcl_RegExp re)		/* The compiled regular expression. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;
    struct infoname {
	int bit;
	const char *text;
    };
    static const struct infoname infonames[] = {
	{REG_UBACKREF,		"REG_UBACKREF"},
	{REG_ULOOKAHEAD,	"REG_ULOOKAHEAD"},
	{REG_UBOUNDS,		"REG_UBOUNDS"},
	{REG_UBRACES,		"REG_UBRACES"},
	{REG_UBSALNUM,		"REG_UBSALNUM"},
	{REG_UPBOTCH,		"REG_UPBOTCH"},
	{REG_UBBS,		"REG_UBBS"},
	{REG_UNONPOSIX,		"REG_UNONPOSIX"},
	{REG_UUNSPEC,		"REG_UUNSPEC"},
	{REG_UUNPORT,		"REG_UUNPORT"},
	{REG_ULOCALE,		"REG_ULOCALE"},
	{REG_UEMPTYMATCH,	"REG_UEMPTYMATCH"},
	{REG_UIMPOSSIBLE,	"REG_UIMPOSSIBLE"},
	{REG_USHORTEST,		"REG_USHORTEST"},
	{0,			NULL}
    };
    const struct infoname *inf;
    Tcl_Obj *infoObj;

    /*
     * The reset here guarantees that the interpreter result is empty and
     * unshared. This means that we can use Tcl_ListObjAppendElement on the
     * result object quite safely.
     */

    Tcl_ResetResult(interp);

    /*
     * Assume that there will never be more than INT_MAX subexpressions. This
     * is a pretty reasonable assumption; the RE engine doesn't scale _that_
     * well and Tcl has other limits that constrain things as well...
     */

    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewIntObj((int) regexpPtr->re.re_nsub));

    /*
     * Now append a list of all the bit-flags set for the RE.
     */

    TclNewObj(infoObj);
    for (inf=infonames ; inf->bit != 0 ; inf++) {
	if (regexpPtr->re.re_info & inf->bit) {
	    Tcl_ListObjAppendElement(NULL, infoObj,
		    Tcl_NewStringObj(inf->text, -1));
	}
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), infoObj);

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegError --
 *
 *	Generate an error message based on the regexp status code.
 *
 * Results:
 *	Places an error in the interpreter.
 *
 * Side effects:
 *	Sets errorCode as well.
 *
 *----------------------------------------------------------------------
 */

void
TclRegError(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    const char *msg,		/* Message to prepend to error. */
    int status)			/* Status code to report. */
{
    char buf[100];		/* ample in practice */
    char cbuf[100];		/* lots in practice */
    size_t n;
    const char *p;

    Tcl_ResetResult(interp);
    n = TclReError(status, NULL, buf, sizeof(buf));
    p = (n > sizeof(buf)) ? "..." : "";
    Tcl_AppendResult(interp, msg, buf, p, NULL);

    sprintf(cbuf, "%d", status);
    (void) TclReError(REG_ITOA, NULL, cbuf, sizeof(cbuf));
    Tcl_SetErrorCode(interp, "REGEXP", cbuf, buf, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeRegexpInternalRep --
 *
 *	Deallocate the storage associated with a regexp object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the compiled regular expression.
 *
 *----------------------------------------------------------------------
 */

static void
FreeRegexpInternalRep(
    Tcl_Obj *objPtr)		/* Regexp object with internal rep to free. */
{
    TclRegexp *regexpRepPtr = (TclRegexp *) objPtr->internalRep.twoPtrValue.ptr1;

    /*
     * If this is the last reference to the regexp, free it.
     */

    if (--(regexpRepPtr->refCount) <= 0) {
	FreeRegexp(regexpRepPtr);
    }
    objPtr->typePtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DupRegexpInternalRep --
 *
 *	We copy the reference to the compiled regexp and bump its reference
 *	count.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Increments the reference count of the regexp.
 *
 *----------------------------------------------------------------------
 */

static void
DupRegexpInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    TclRegexp *regexpPtr = (TclRegexp *) srcPtr->internalRep.twoPtrValue.ptr1;

    regexpPtr->refCount++;
    copyPtr->internalRep.twoPtrValue.ptr1 = srcPtr->internalRep.twoPtrValue.ptr1;
    copyPtr->typePtr = &tclRegexpType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetRegexpFromAny --
 *
 *	Attempt to generate a compiled regular expression for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a regular expression is stored as "objPtr"s
 *	internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetRegexpFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr)		/* The object to convert. */
{
    if (Tcl_GetRegExpFromObj(interp, objPtr, REG_ADVANCED) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * AllocCaptStorage --
 */
static void
AllocCaptStorage(TclRegexp *regexpPtr)
{
    int nsubs = regexpPtr->re.re_nsub;
    TclRegexpStorage *reStorage = regexpPtr->reStorage;

#ifdef HAVE_PCRE
    int veccnt;
    /* 
     * We use special handling to allocate storages for PCRE offsets/matches,
     * because on some systems size we can use the same storage for both,
     * if sizes of regoffs_t and regmatch_t are equal.
     */
    veccnt = VectorCoountPCRE(regexpPtr);
    if (reStorage->offsCnt < veccnt) {
	reStorage->offsCnt = veccnt;
	reStorage->offsSize = sizeof(int) * veccnt;
	/* if initial call (first call) */
	if (!reStorage->offsets) {
	    reStorage->offsets = (int*)ckalloc(reStorage->offsSize);
	    /* we can use matches = offsets if size of two int's is equal regmatch_t structure */
	    if (sizeof(regoffs_t) != sizeof(regmatch_t)) {
		reStorage->matchSize = sizeof(regmatch_t) * (nsubs+1);
		reStorage->matches = (regmatch_t*)ckalloc(reStorage->matchSize);
	    } else {
		reStorage->matchSize = reStorage->offsSize;
		reStorage->matches = (regmatch_t*)reStorage->offsets;
	    }
	} else {
	    /* enlarge storages */
	    reStorage->offsets = (int*)ckrealloc(
		(char*)reStorage->offsets, sizeof(int) * veccnt);
	    /* we can use matches = offsets if size of two int's is equal regmatch_t structure */
	    if (sizeof(regoffs_t) != sizeof(regmatch_t)) {
		reStorage->matchSize = sizeof(regmatch_t) * (nsubs+1);
		reStorage->matches = (regmatch_t*)ckrealloc(
		    (char*)reStorage->matches, reStorage->matchSize);
	    } else {
		reStorage->matchSize = reStorage->offsSize;
		reStorage->matches = (regmatch_t*)reStorage->offsets;
	    }
	}
    }

    /* if DFA and still no workspace allocated - initial call */
    if ((regexpPtr->flags & TCL_REG_PCDFA) && !reStorage->wrkSpace) {
	reStorage->wrkSpCnt = 60;
	reStorage->wrkSpSize = sizeof(int) * reStorage->wrkSpCnt;
	reStorage->wrkSpace = (int *)ckalloc(reStorage->wrkSpSize);
    }

#else

    if (!reStorage->matches || reStorage->matchSize < sizeof(regmatch_t) * (nsubs+1)) {
	reStorage->matchSize = sizeof(regmatch_t) * (nsubs+1);
	/* if initial call (first call) */
	if (!reStorage->matches) {
	    reStorage->matches = (regmatch_t*)ckalloc(reStorage->matchSize);
	} else {
	    /* enlarge storage */
	    reStorage->matches = (regmatch_t*)ckrealloc(
		(char*)reStorage->matches, reStorage->matchSize);
	}
    }

#endif
}

#ifdef HAVE_PCRE
static void
EnlargeWrkSpaceStorage(TclRegexp *regexpPtr) {
    TclRegexpStorage *reStorage = regexpPtr->reStorage;
    size_t newSize;

    /* double size, just to avoid too many reallocations */
    reStorage->wrkSpCnt *= 2;
    newSize = sizeof(int) * reStorage->wrkSpCnt;

    if (reStorage->wrkSpSize < newSize) {
	reStorage->wrkSpSize = newSize;
	/* enlarge storage */
	reStorage->wrkSpace = (int*)ckrealloc(
	    (char*)reStorage->wrkSpace, newSize);
    }
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * CompileRegexp --
 *
 *	Attempt to compile the given regexp pattern. If the compiled regular
 *	expression can be found in the per-thread cache, it will be used
 *	instead of compiling a new copy.
 *
 * Results:
 *	The return value is a pointer to a newly allocated TclRegexp that
 *	represents the compiled pattern, or NULL if the pattern could not be
 *	compiled. If NULL is returned, an error message is left in the
 *	interp's result.
 *
 * Side effects:
 *	The thread-local regexp cache is updated and a new TclRegexp may be
 *	allocated.
 *
 *----------------------------------------------------------------------
 */

static TclRegexp *
CompileRegexp(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    const char *string,		/* The regexp to compile (UTF-8). */
    int length,			/* The length of the string in bytes. */
    int flags)			/* Compilation flags. */
{
    TclRegexp *regexpPtr;
    const Tcl_UniChar *uniString;
    int numChars, status, i, exact;
    Tcl_DString stringBuf;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;
	Tcl_CreateThreadExitHandler(FinalizeRegexp, NULL);
    }

    /*
     * This routine maintains a second-level regular expression cache in
     * addition to the per-object regexp cache. The per-thread cache is needed
     * to handle the case where for various reasons the object is lost between
     * invocations of the regexp command, but the literal pattern is the same.
     */

    /*
     * Check the per-thread compiled regexp cache. We can only reuse a regexp
     * if it has the same pattern and the same flags.
     */

    for (i = 0; (i < NUM_REGEXPS) && (tsdPtr->patterns[i] != NULL); i++) {
	if ((length == tsdPtr->patLengths[i])
		&& (tsdPtr->regexps[i]->flags == flags)
		&& (strcmp(string, tsdPtr->patterns[i]) == 0)) {
	    /*
	     * Move the matched pattern to the first slot in the cache and
	     * shift the other patterns down one position.
	     */

	    if (i != 0) {
		int j;
		char *cachedString;

		cachedString = tsdPtr->patterns[i];
		regexpPtr = tsdPtr->regexps[i];
		for (j = i-1; j >= 0; j--) {
		    tsdPtr->patterns[j+1] = tsdPtr->patterns[j];
		    tsdPtr->patLengths[j+1] = tsdPtr->patLengths[j];
		    tsdPtr->regexps[j+1] = tsdPtr->regexps[j];
		}
		tsdPtr->patterns[0] = cachedString;
		tsdPtr->patLengths[0] = length;
		tsdPtr->regexps[0] = regexpPtr;
	    }
	    return tsdPtr->regexps[0];
	}
    }

    /*
     * This is a new expression, so compile it and add it to the cache.
     */

    regexpPtr = (TclRegexp *) ckalloc(sizeof(TclRegexp));
    memset(regexpPtr, 0, sizeof(TclRegexp));

    regexpPtr->flags = flags;
    regexpPtr->details.rm_extend.rm_so = -1;
    regexpPtr->details.rm_extend.rm_eo = -1;

    if (flags & TCL_REG_PCRE) {
#ifdef HAVE_PCRE
	pcre *pcre;
	char *p, *cstring = (char *) string;
	const char *errstr;
	int erroffset, rc, nsubs, pcrecflags;

	/*
	 * Convert from Tcl classic to PCRE cflags
	 */

	/* XXX Should enable PCRE_UTF8 selectively on non-ByteArray Tcl_Obj 
	 * TODO: parse of bytearray doesn't expect PCRE_UTF8 here 
	 * PCRE_JAVASCRIPT_COMPAT - for \uXXXX char sequence support */
	pcrecflags = PCRE_UTF8 | PCRE_UCP | PCRE_NO_UTF8_CHECK |
		PCRE_DOLLAR_ENDONLY | PCRE_JAVASCRIPT_COMPAT;
	/*
	for (i = 0, p = cstring; i < length; i++) {
	    if (UCHAR(*p++) > 0x80) {
		pcrecflags |= PCRE_UTF8 | PCRE_UCP;
		break;
	    }
	}
	*/
	if (flags & TCL_REG_NOCASE) {
	    pcrecflags |= PCRE_CASELESS;
	}
	if (flags & TCL_REG_EXPANDED) {
	    pcrecflags |= PCRE_EXTENDED;
	}
	if (flags & TCL_REG_NLANCH) {
	    pcrecflags |= PCRE_MULTILINE;
	}
	if (!(flags & TCL_REG_NLSTOP)) {
	    pcrecflags |= PCRE_DOTALL;
	}
	/*
	 * Note that DFA currently does not support captured groups (substrings) at all,
	 * but it returns all matched alternatives instead of. Disable capturing anyway.
	 */
	if ((flags & TCL_REG_PCDFA)) {
	    pcrecflags |= PCRE_NO_AUTO_CAPTURE;
	}

	if (cstring[length] != 0) {
	    cstring = (char *) ckalloc(length + 1);
	    memcpy(cstring, string, length);
	    cstring[length] = 0;
	}
	pcre = pcre_compile(cstring, pcrecflags, &errstr, &erroffset, NULL);
	regexpPtr->pcre = pcre;
	if (cstring != (char *) string) {
	    ckfree(cstring);
	}

	if (pcre == NULL) {
	    ckfree((char *)regexpPtr);
	    Tcl_AppendResult(interp,
		    "couldn't compile pcre pattern: ", errstr, NULL);
	    return NULL;
	}

	regexpPtr->study = pcre_study(pcre, PCRE_STUDY_JIT_COMPILE, &errstr);
	if (errstr != NULL) {
	    pcre_free(pcre);
	    ckfree((char *)regexpPtr);
	    Tcl_AppendResult(interp,
		    "error studying pcre pattern: ", errstr, NULL);
	    return NULL;
	}

	nsubs = 0;
	if (!(flags & TCL_REG_PCDFA)) {
	  rc = pcre_fullinfo(pcre, NULL, PCRE_INFO_CAPTURECOUNT, &nsubs);
	  if (rc != 0) {
	    /* todo - error handling */
	    nsubs = 0;
	  }
	}
	regexpPtr->re.re_nsub = nsubs;

#else
	Tcl_AppendResult(interp,
		"couldn't compile pcre pattern: pcre unavailabe", NULL);
	return NULL;
#endif
    } else {
	/*
	 * Get the up-to-date string representation and map to unicode.
	 */

	Tcl_DStringInit(&stringBuf);
	uniString = Tcl_UtfToUniCharDString(string, length, &stringBuf);
	numChars = Tcl_DStringLength(&stringBuf) / sizeof(Tcl_UniChar);

	/*
	 * Compile the string and check for errors.
	 */

	status = TclReComp(&regexpPtr->re, uniString, (size_t) numChars, flags);
	Tcl_DStringFree(&stringBuf);

	if (status != REG_OKAY) {
	    /*
	     * Clean up and report errors in the interpreter, if possible.
	     */

	    ckfree((char *)regexpPtr);
	    if (interp) {
		TclRegError(interp,
			"couldn't compile regular expression pattern: ",
			status);
	    }
	    return NULL;
	}
    }

    /*
     * Don't allocate capture storages, it occurs on demand by the first usage,
     * just set current reference in regexp for fast storage access without TSD lookup.
     */
    regexpPtr->reStorage = &tsdPtr->reStorage;

    /*
     * Convert RE to a glob pattern equivalent, if any, and cache it.  If this
     * is not possible, then globObjPtr will be NULL.  This is used by
     * Tcl_RegExpExecObj to optionally do a fast match (avoids RE engine).
     */

    if (TclReToGlob(NULL, string, length, &stringBuf, &exact) == TCL_OK) {
	regexpPtr->globObjPtr = Tcl_NewStringObj(Tcl_DStringValue(&stringBuf),
		Tcl_DStringLength(&stringBuf));
	Tcl_IncrRefCount(regexpPtr->globObjPtr);
	Tcl_DStringFree(&stringBuf);
    } else {
	regexpPtr->globObjPtr = NULL;
    }

    /*
     * Initialize the refcount to one initially, since it is in the cache.
     */

    regexpPtr->refCount = 1;

    /*
     * Free the last regexp, if necessary, and make room at the head of the
     * list for the new regexp.
     */

    if (tsdPtr->patterns[NUM_REGEXPS-1] != NULL) {
	TclRegexp *oldRegexpPtr = tsdPtr->regexps[NUM_REGEXPS-1];
	if (--(oldRegexpPtr->refCount) <= 0) {
	    FreeRegexp(oldRegexpPtr);
	}
	ckfree(tsdPtr->patterns[NUM_REGEXPS-1]);
    }
    for (i = NUM_REGEXPS - 2; i >= 0; i--) {
	tsdPtr->patterns[i+1] = tsdPtr->patterns[i];
	tsdPtr->patLengths[i+1] = tsdPtr->patLengths[i];
	tsdPtr->regexps[i+1] = tsdPtr->regexps[i];
    }
    tsdPtr->patterns[0] = (char *) ckalloc((unsigned) (length+1));
    strcpy(tsdPtr->patterns[0], string);
    tsdPtr->patLengths[0] = length;
    tsdPtr->regexps[0] = regexpPtr;

    /* Ensure we have offsets/matches of expected size once at compile time */
    AllocCaptStorage(regexpPtr);

    return regexpPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeRegexp --
 *
 *	Release the storage associated with a TclRegexp.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeRegexp(
    TclRegexp *regexpPtr)	/* Compiled regular expression to free. */
{
#ifdef HAVE_PCRE
    if (regexpPtr->flags & TCL_REG_PCRE) {
	pcre_free(regexpPtr->pcre);
	if (regexpPtr->study) {
	    pcre_free_study(regexpPtr->study);
	}
    } else
#endif
    TclReFree(&regexpPtr->re);
    if (regexpPtr->globObjPtr) {
	TclDecrRefCount(regexpPtr->globObjPtr);
    }
    ckfree((char *) regexpPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FinalizeRegexp --
 *
 *	Release the storage associated with the per-thread regexp cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FinalizeRegexp(
    ClientData clientData)	/* Not used. */
{
    int i;
    TclRegexp *regexpPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    TclRegexpStorage *reStorage = &tsdPtr->reStorage;

    for (i = 0; (i < NUM_REGEXPS) && (tsdPtr->patterns[i] != NULL); i++) {
	regexpPtr = tsdPtr->regexps[i];
	if (--(regexpPtr->refCount) <= 0) {
	    FreeRegexp(regexpPtr);
	}
	ckfree(tsdPtr->patterns[i]);
	tsdPtr->patterns[i] = NULL;
    }
#ifdef HAVE_PCRE
    if (reStorage->offsets != NULL) {
    	/* preserve dual release of same block */
	if ((int*)reStorage->matches == reStorage->offsets) {
	    reStorage->matches = NULL;
	}
	ckfree((char *) reStorage->offsets);
	reStorage->offsSize = 0;
    }
    if (reStorage->wrkSpace != NULL) {
	ckfree((char *) reStorage->wrkSpace);
	reStorage->wrkSpSize = 0;
	reStorage->wrkSpCnt = 0;
    }
#endif
    if (reStorage->matches != NULL) {
	ckfree((char *) reStorage->matches);
	reStorage->matchSize = 0;
    }
    /*
     * We may find ourselves reinitialized if another finalization routine
     * invokes regexps.
     */
    tsdPtr->initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegexpClassic --
 *
 *	This procedure processes a classic "regexp".
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclRegexpClassic(
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument objects. */
    Tcl_RegExp regExpr,
    int flags,
    int offset)
{
    int i, match, numMatches = 0, numMatchesSaved;
    int eflags, stringLength, matchLength;
    Tcl_Obj *objPtr, *resultPtr = NULL;
    Tcl_RegExpInfo info;

    objPtr = objv[1];
    stringLength = Tcl_GetCharLength(objPtr);

    eflags = 0;
    objc -= 2;
    objv += 2;

    if (flags & TCL_REG_DOINLINE) {
	/*
	 * Save all the subexpressions, as we will return them as a list
	 */

	numMatchesSaved = -1;
    } else {
	/*
	 * Save only enough subexpressions for matches we want to keep, expect
	 * in the case of -all, where we need to keep at least one to know
	 * where to move the offset.
	 */

	numMatchesSaved = (objc == 0) ? (flags & TCL_REG_RETALL) : objc;
    }

    /*
     * The following loop is to handle multiple matches within the same source
     * string; each iteration handles one match. If "-all" hasn't been
     * specified then the loop body only gets executed once. We terminate the
     * loop when the starting offset is past the end of the string.
     */

    while (1) {
	/*
	 * Pass either 0 or TCL_REG_NOTBOL in the eflags. Passing
	 * TCL_REG_NOTBOL indicates that the character at offset should not be
	 * considered the start of the line. If for example the pattern {^} is
	 * passed and -start is positive, then the pattern will not match the
	 * start of the string unless the previous character is a newline.
	 */

	if (offset == 0) {
	    eflags = 0;
	} else if (offset > stringLength) {
	    eflags = TCL_REG_NOTBOL;
	} else if (Tcl_GetUniChar(objPtr, offset-1) == (Tcl_UniChar)'\n') {
	    eflags = 0;
	} else {
	    eflags = TCL_REG_NOTBOL;
	}

	match = Tcl_RegExpExecObj(interp, regExpr, objPtr, offset,
		numMatchesSaved, eflags);
	if (match < 0) {
	    return TCL_ERROR;
	}

	if (match == 0) {
	    break;
	}

	/*
	 * If additional variable names have been specified, return index
	 * information in those variables.
	 */

	Tcl_RegExpGetInfo(regExpr, &info);
	if (flags & TCL_REG_DOINLINE) {
	    /*
	     * It's the number of substitutions, plus one for the matchVar at
	     * index 0
	     */

	    objc = info.nsubs + 1;
	    if (!resultPtr) {
		resultPtr = Tcl_NewObj();
	    }
	}
	for (i = 0; i < objc; i++) {
	    Tcl_Obj *newPtr;

	    if (flags & TCL_REG_RETIDX) {
		int start, end;
		Tcl_Obj *objs[2];

		/*
		 * Only adjust the match area if there was a match for that
		 * area. (Scriptics Bug 4391/SF Bug #219232)
		 */

		if (i <= info.nsubs && info.matches[i].start >= 0) {
		    start = offset + info.matches[i].start;
		    end = offset + info.matches[i].end;

		    /*
		     * Adjust index so it refers to the last character in the
		     * match instead of the first character after the match.
		     */

		    if (end >= offset) {
			end--;
		    }
		} else {
		    start = -1;
		    end = -1;
		}

		objs[0] = Tcl_NewLongObj(start);
		objs[1] = Tcl_NewLongObj(end);

		newPtr = Tcl_NewListObj(2, objs);
	    } else {
		if (i <= info.nsubs) {
		    newPtr = Tcl_GetRange(objPtr,
			    offset + info.matches[i].start,
			    offset + info.matches[i].end - 1);
		} else {
		    newPtr = Tcl_NewObj();
		}
	    }
	    if (flags & TCL_REG_DOINLINE) {
		if (Tcl_ListObjAppendElement(interp, resultPtr, newPtr)
			!= TCL_OK) {
		    Tcl_DecrRefCount(newPtr);
		    Tcl_DecrRefCount(resultPtr);
		    return TCL_ERROR;
		}
	    } else {
		Tcl_Obj *valuePtr;
		valuePtr = Tcl_ObjSetVar2(interp, objv[i], NULL, newPtr, 0);
		if (valuePtr == NULL) {
		    Tcl_AppendResult(interp, "couldn't set variable \"",
			    TclGetString(objv[i]), "\"", NULL);
		    return TCL_ERROR;
		}
	    }
	}

	numMatches++;
	if (!(flags & TCL_REG_RETALL)) {
	    break;
	}

	/*
	 * Adjust the offset to the character just after the last one in the
	 * matchVar and increment numMatches to count how many times we have
	 * match. We always increment the offset by at least one to prevent
	 * endless looping (as in the case: regexp -all {a*} a). Otherwise,
	 * when we match the NULL string at the end of the input string, we
	 * will loop indefinately (because the length of the match is 0, so
	 * offset never changes).
	 */

	matchLength = info.matches[0].end - info.matches[0].start;
	offset += info.matches[0].end;

	/*
	 * A match of length zero could happen for {^} {$} or {.*} and in
	 * these cases we always want to bump the index up one.
	 */

	if (matchLength == 0) {
	    offset++;
	}
	if (offset >= stringLength) {
	    break;
	}
    }

    /*
     * Set the interpreter's object result to an integer object with numMatches
     * (the number of times through the while - 1) if -inline wasn't specified,
     * otherwise it's a list with matches.
     */

    if (flags & TCL_REG_DOINLINE) {
	Tcl_SetObjResult(interp, resultPtr ? resultPtr : Tcl_NewObj());
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(numMatches));
    }
    return TCL_OK;
}

/*
 * Helper to map byte offset to char offset, using array of known offsets
 * mb2c, which gets stored typically:
 * first start [0], last known middle start [1] & end [2], first end [3]
 */
int
BOffs2COffs(
    int *mb2c,		/* Mapping array of offsets ([0..3] - byte offsets,
			 * [4..7] - char offsets). */
    const char *src,	/* String source in utf-8. */
    int offs)		/* Byte offset to be mapped to char offset; caller must
			 * check it is positive. */
{
    int i, o = offs, coffs = 0;

    /* Scan for known offsets starting from largest one */
    for (i = 3; i >= 0 && mb2c[i]; i--) {
	if (o >= mb2c[i]) {
	    o -= mb2c[i];
	    src += mb2c[i];
	    coffs = mb2c[i+4];
	    break;
	}
    }
    /* Scan for chars in source starting from found known offset or 0 */
    if (o) {
	const char *srcend;
	Tcl_UniChar ch = 0;
	
	srcend = src + o;
	do {
	    coffs++;
	    src += TclUtfToUniChar(src, &ch);
	} while (src < srcend);
	/*
	 * New offset known that is larger that previously known one, store it
	 * and shift indices (1st and 2nd only), because 0th and 3rd are reserved
	 * for first start and end indices (helps by search of nested indices).
	 */
	if (i > 1) {
	    if (i > 2 && mb2c[i-1]) {
		mb2c[i-2] = mb2c[i-1];
		mb2c[i+4-2] = mb2c[i+4-1];
	    }
	    if (mb2c[i]) {
		mb2c[i-1] = mb2c[i];
		mb2c[i+4-1] = mb2c[i+4];
	    }
	    mb2c[i] = offs;
	    mb2c[i+4] = coffs;
	}
    }
    return coffs;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegexpPCRE --
 *
 *	This procedure processes a PCRE "regexp".
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclRegexpPCRE(
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument objects. */
    Tcl_RegExp regExpr,
    int flags,
    int offset)
{
#ifdef HAVE_PCRE
    int i, match, eflags, pcrecflags = 0, stringLength, *offsets, matchelems,
	offsetDiff, offsetC = offset, numMatches = 0 /*, utfstr*/;
    Tcl_Obj *objPtr, *resultPtr = NULL;
    const char *matchstr;
    pcre *re;
    pcre_extra *study;
    TclRegexp *regexpPtr = (TclRegexp *) regExpr;
    TclRegexpStorage *reStorage = regexpPtr->reStorage;
    int mb2c[4*2] = {/* byte offs */ 0, 0, 0, 0,  /* char offs */ 0, 0, 0, 0};

    objPtr = objv[1];
    /* 
     * Match byte-array as string (safe against shimmer, probably possible if RE
     * gets recompiled on demand without PCRE_UTF8, but there are '\w' or `[[:alpha:]]`
     * which could then confuse some byte sequences with chars, so to avoid regression
     * let parse it as utf-8).
     * TODO: implement -binary option to scan byte-array as byte-array
     */
#define utfstr 1
#if 0
    /* not implemented for byte-array */
    utfstr = (objPtr->typePtr != &tclByteArrayType);
#endif
    /*
     * Get match string and translate offset into correct placement for utf-8 chars.
     */
    if (utfstr) {
	matchstr = Tcl_GetStringFromObj(objPtr, &stringLength);
	/* OFFS_CHAR2BYTE: convert offset in chars to offset in bytes */
	if (!(flags & TCL_REG_BYTEOFFS) && offset > 0) {
	    Tcl_UniChar ch;
	    const char *src = matchstr, *srcend = matchstr + stringLength;

	    
	    mb2c[0+4] = offset;
	    /* Tcl_UtfAtIndex considering string length */
	    while (offset-- > 0 && src < srcend) {
		src += TclUtfToUniChar(src, &ch);
	    }
	    mb2c[3+4] = (mb2c[0+4] -= offset+1); /* known char offset */
	    mb2c[3] = mb2c[0] = src - matchstr;  /* for this byte offset */
	    if (offset <= 0) {
		offset = mb2c[0];
	    } else {
		offset = stringLength+1; /* outside of string (and > 0 for empty string) */
	    }
	}
    } else {
	matchstr = (const char *)Tcl_GetByteArrayFromObj(objPtr, &stringLength);
	if (offset > 0) {
	    mb2c[3+4] = mb2c[0+4] =
	    mb2c[3] = mb2c[0] = offset;
	}
    }

    objc -= 2;
    objv += 2;

    offsets = reStorage->offsets;
    matchelems = 0;
    /* need captured groups or -inline */
    if (objc || (flags & TCL_REG_DOINLINE)) {
	matchelems = reStorage->offsCnt;
    } else {
    	/* -all expects at least the first index in order to scroll over string */
	if (flags & TCL_REG_RETALL) {
	    matchelems = 3;
	}
    	eflags |= PCRE_NO_AUTO_CAPTURE;
    }

    re = regexpPtr->pcre;
    study = regexpPtr->study;
    eflags = PCRE_NO_UTF8_CHECK;
    if ((flags & TCL_REG_RETALL)) {
	pcre_fullinfo(re, NULL, PCRE_INFO_OPTIONS, &pcrecflags);
    }

    /*
     * The following loop is to handle multiple matches within the same source
     * string; each iteration handles one match. If "-all" hasn't been
     * specified then the loop body only gets executed once. We terminate the
     * loop when the starting offset is past the end of the string.
     */

    while (1) {

	offsetDiff = 0;
	if (offset > 0) {

	   /* 
	    * PCRE has currently a "bug" by multiline with offset after "\n":
	    * ^ - meant assert start of string (or line, in multiline mode),
	    * but it will be not found by offset after "\n" regardless multiline mode.
	    * Thus just let do a small adustment (hacking with shift of offset or length to NTS).
	    * Note we should do it always in order to regard enable multiline by exec using `(?m)`.
	    * If offset > stringLength, it avoids bad offset error (PCRE_ERROR_BADOFFSET).
	    */
	    if (offset >= stringLength) {
		int bol;
		/* avoid match {^$} without multiline, if we are out of range */
		if (!numMatches && offset > stringLength) {
		    eflags |= PCRE_NOTBOL;
		}
		/* safe offset to correct indices if empty match found */
		offsetDiff = offsetC;
		offset = stringLength;	/* offset after last char */
		if ((flags & TCL_REG_RETALL) && numMatches && offset) {
		    /* 
		    if (utfstr) {
			bol = *(Tcl_UtfPrev(matchstr + offset, matchstr)) == '\n';
		    } else {
			bol = matchstr[offset-1] == '\n';
		    }
		    */
		    bol = matchstr[offset-1] == '\n';
		    /* fast fallback if we are not begin of new-line (cannot match anyway) */
		    if (!bol) {
			break;
		    } else {
			/* hacking PCRE to accept this "extra" new-line (after newline empty match). */
			matchstr = "";
			offset = 0;
			stringLength = 0;
			eflags |= PCRE_ANCHORED;
		    }
		}
		flags &= ~TCL_REG_RETALL;		/* don't repeat */
	    }
	}

	if (!(regexpPtr->flags & TCL_REG_PCDFA)) {
	    match = pcre_exec(re, study, matchstr, stringLength,
		offset, eflags, offsets, matchelems);
	} else {
	    do {
		match = pcre_dfa_exec(re, study, matchstr, stringLength,
		    offset, eflags, offsets, matchelems,
		    reStorage->wrkSpace, reStorage->wrkSpCnt);
		if (match == PCRE_ERROR_DFA_WSSIZE) {
		    EnlargeWrkSpaceStorage(regexpPtr);
		    continue;
	        }
		if (match || !(objc || (flags & TCL_REG_DOINLINE))) break;
		/* insufficient capture space - enlarge vectors buffer */
		regexpPtr->re.re_nsub = (regexpPtr->re.re_nsub+1)*2;
		AllocCaptStorage(regexpPtr);
		offsets = reStorage->offsets;
		matchelems = reStorage->offsCnt;
	    } while(1);
	}

	if (match < -1) {
	    /* offset is out of range (bad utf, wrong length etc) */
	    if (match == PCRE_ERROR_BADOFFSET) {
		match = PCRE_ERROR_NOMATCH;
	    } else {
		char buf[32 + TCL_INTEGER_SPACE];
		sprintf(buf, "pcre_exec returned error code %d", match);
		Tcl_AppendResult(interp, buf, NULL);
		return TCL_ERROR;
	    }
	}

	if (!match) {
	    if ((objc || (flags & TCL_REG_DOINLINE))) {
		Tcl_AppendResult(interp,
		    "pcre_exec had insufficient capture space", NULL);
		return TCL_ERROR;
	    }
	    match = 1;
	}

	if (match == PCRE_ERROR_NOMATCH) {
	   /* 
	    * In order to process last line correctly by multiline processing e. g. `(?m)^`
	    * try to find match after string (in case of findall and already matched something).
	    * Option pcrecflags & PCRE_ANCHORED is not set in multiline mode (resp. `(?m)`),
	    * in this case no match means - we will find nothing at all, so don't repeat.
	    */
	    if (!(flags & TCL_REG_RETALL) || !numMatches || !stringLength || (pcrecflags & PCRE_ANCHORED)) {
		break;
	    }
	    /* If we tried unshifted search - repeat from next offset */
	    if (eflags & PCRE_NOTEMPTY_ATSTART) {
		eflags &= ~(PCRE_NOTEMPTY_ATSTART|PCRE_ANCHORED);
		offsetC++;
		if (utfstr && offset < stringLength) {
		    offset = Tcl_UtfNext(matchstr + offset) - matchstr;
		} else {
		    offset++;
		}
		continue;
	    }
	    /* offset to end of string */
	    offset = stringLength;
	    if (utfstr && (flags & TCL_REG_RETIDX)) {
		offsetC = Tcl_NumUtfChars(matchstr, stringLength);
	    } else {
	        offsetC = offset;
	    }

	    /* repeat once search at end */
	    continue;
	}

	/*
	 * If additional variable names have been specified, return index
	 * information in those variables.
	 */

	if (flags & TCL_REG_DOINLINE) {
	    /*
	     * It's the number of substitutions, plus one for the matchVar at
	     * index 0
	     * Note we can get fewer matches as specified (thus just use [-1, -1] indices).
	     * In case of DFA we've count of matched alternatives here (sorted by longest match).
	     */

	    objc = (!(regexpPtr->flags & TCL_REG_PCDFA)) ? 
		regexpPtr->re.re_nsub + 1 : match;
	    if (!resultPtr) {
		/* empty list with reserved elements by current matched count */
		resultPtr = Tcl_NewListObj(objc, NULL);
	    }
	}

	for (i = 0; i < objc; i++) {
	    Tcl_Obj *newPtr;
	    int start, end;

	    if (i < match) {
		start = offsets[i*2];
		end = offsets[i*2 + 1];
		/* OFFS_BYTE2CHAR: convert offset in bytes to offset in chars */
		if (!(flags & TCL_REG_BYTEOFFS) && (flags & TCL_REG_RETIDX)) {
		  if (!offsetDiff) {
		    if (start > 0) {
		    	start = BOffs2COffs(mb2c, matchstr, start);
		    }
		    if (end > 0) {
		    	end = BOffs2COffs(mb2c, matchstr, end);
		    }
		    /* first is whole match, so store its indices as 0th and 3rd */
		    if (i == 0) {
		    	/* mostly smallest */
			mb2c[0] = offsets[0];
			mb2c[0+4] = start;
			/* mostly largest */
			mb2c[3] = offsets[1];
			mb2c[3+4] = end;
		    }
		  } else {
		    /* if out of range we've always empty match [offs, offs-1] */
		    end = start = offsetDiff;
		  }
		}
	    } else {
		start = -1;
		end = 0;
	    }
	    if (flags & TCL_REG_RETIDX) {
		Tcl_Obj *objs[2];

		objs[0] = Tcl_NewLongObj(start);
		objs[1] = Tcl_NewLongObj(end >= 0 ? end-1 : end);

		newPtr = Tcl_NewListObj(2, objs);
	    } else {
		if (i < match) {
		    if (utfstr) {
			newPtr = Tcl_NewStringObj(matchstr + start, end - start);
		    } else {
			newPtr = Tcl_NewByteArrayObj((const unsigned char *)(matchstr + start), end - start);
		    }
		} else {
		    newPtr = Tcl_NewObj();
		}
	    }
	    if (flags & TCL_REG_DOINLINE) {
		if (Tcl_ListObjAppendElement(interp, resultPtr, newPtr)
			!= TCL_OK) {
		    Tcl_DecrRefCount(newPtr);
		    Tcl_DecrRefCount(resultPtr);
		    return TCL_ERROR;
		}
	    } else {
		Tcl_Obj *valuePtr;
		valuePtr = Tcl_ObjSetVar2(interp, objv[i], NULL, newPtr, 0);
		if (valuePtr == NULL) {
		    Tcl_AppendResult(interp, "couldn't set variable \"",
			    TclGetString(objv[i]), "\"", NULL);
		    return TCL_ERROR;
		}
	    }
	}

	numMatches++;
	if (!(flags & TCL_REG_RETALL)) {
	    break;
	}

	/*
	 * Adjust the offset to the character just after the last one in the
	 * matchVar.
	 * In order to correct find all empty matches (X..X-1), we'll use
	 * PCRE_NOTEMPTY_ATSTART|PCRE_ANCHORED pair for start next try from the
	 * same offset (if not found again, break the cycle above).
	 */

	if (offsets[1] > offsets[0]) {
	    offset = offsets[1];
	    offsetC = mb2c[3+4]; /* only used by indices as offsetDiff */
	} else {
	    offset = offsets[0];
	    offsetC = mb2c[0+4]; /* only used by indices as offsetDiff */
	    eflags |= (PCRE_NOTEMPTY_ATSTART|PCRE_ANCHORED);
	}
    }

    /*
     * Set the interpreter's object result to an integer object with numMatches
     * (the number of times through the while - 1) if -inline wasn't specified,
     * otherwise it's a list with matches.
     */

    if (flags & TCL_REG_DOINLINE) {
	Tcl_SetObjResult(interp, resultPtr ? resultPtr : Tcl_NewObj());
    } else {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(numMatches));
    }
    return TCL_OK;

#undef utfstr

#else /* !HAVE_PCRE */
    Tcl_AppendResult(interp, "PCRE not available", NULL);
    return TCL_ERROR;
#endif
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
