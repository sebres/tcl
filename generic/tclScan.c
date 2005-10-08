/*
 * tclScan.c --
 *
 *	This file contains the implementation of the "scan" command.
 *
 * Copyright (c) 1998 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclScan.c,v 1.19 2005/10/08 14:42:45 dgp Exp $
 */

#include "tclInt.h"

/*
 * Flag values used by Tcl_ScanObjCmd.
 */

#define SCAN_NOSKIP	0x1		/* Don't skip blanks. */
#define SCAN_SUPPRESS	0x2		/* Suppress assignment. */
#define SCAN_UNSIGNED	0x4		/* Read an unsigned value. */
#define SCAN_WIDTH	0x8		/* A width value was supplied. */

#if 0
#define SCAN_SIGNOK	0x10		/* A +/- character is allowed. */
#define SCAN_NODIGITS	0x20		/* No digits have been scanned. */
#define SCAN_NOZERO	0x40		/* No zero digits have been scanned. */
#define SCAN_XOK	0x80		/* An 'x' is allowed. */
#define SCAN_PTOK	0x100		/* Decimal point is allowed. */
#define SCAN_EXPOK	0x200		/* An exponent is allowed. */
#endif

#define SCAN_LONGER	0x400		/* Asked for a wide value. */
#define SCAN_BIG	0x800		/* Asked for a bignum value. */

/*
 * The following structure contains the information associated with a
 * character set.
 */

typedef struct CharSet {
    int exclude;		/* 1 if this is an exclusion set. */
    int nchars;
    Tcl_UniChar *chars;
    int nranges;
    struct Range {
	Tcl_UniChar start;
	Tcl_UniChar end;
    } *ranges;
} CharSet;

/*
 * Declarations for functions used only in this file.
 */

static char *	BuildCharSet _ANSI_ARGS_((CharSet *cset, char *format));
static int	CharInSet _ANSI_ARGS_((CharSet *cset, int ch));
static void	ReleaseCharSet _ANSI_ARGS_((CharSet *cset));
static int	ValidateFormat _ANSI_ARGS_((Tcl_Interp *interp, char *format,
		    int numVars, int *totalVars));

/*
 *----------------------------------------------------------------------
 *
 * BuildCharSet --
 *
 *	This function examines a character set format specification and builds
 *	a CharSet containing the individual characters and character ranges
 *	specified.
 *
 * Results:
 *	Returns the next format position.
 *
 * Side effects:
 *	Initializes the charset.
 *
 *----------------------------------------------------------------------
 */

static char *
BuildCharSet(cset, format)
    CharSet *cset;
    char *format;		/* Points to first char of set. */
{
    Tcl_UniChar ch, start;
    int offset, nranges;
    char *end;

    memset(cset, 0, sizeof(CharSet));

    offset = Tcl_UtfToUniChar(format, &ch);
    if (ch == '^') {
	cset->exclude = 1;
	format += offset;
	offset = Tcl_UtfToUniChar(format, &ch);
    }
    end = format + offset;

    /*
     * Find the close bracket so we can overallocate the set.
     */

    if (ch == ']') {
	end += Tcl_UtfToUniChar(end, &ch);
    }
    nranges = 0;
    while (ch != ']') {
	if (ch == '-') {
	    nranges++;
	}
	end += Tcl_UtfToUniChar(end, &ch);
    }

    cset->chars = (Tcl_UniChar *) ckalloc(sizeof(Tcl_UniChar)
	    * (end - format - 1));
    if (nranges > 0) {
	cset->ranges = (struct Range *) ckalloc(sizeof(struct Range)*nranges);
    } else {
	cset->ranges = NULL;
    }

    /*
     * Now build the character set.
     */

    cset->nchars = cset->nranges = 0;
    format += Tcl_UtfToUniChar(format, &ch);
    start = ch;
    if (ch == ']' || ch == '-') {
	cset->chars[cset->nchars++] = ch;
	format += Tcl_UtfToUniChar(format, &ch);
    }
    while (ch != ']') {
	if (*format == '-') {
	    /*
	     * This may be the first character of a range, so don't add it
	     * yet.
	     */

	    start = ch;
	} else if (ch == '-') {
	    /*
	     * Check to see if this is the last character in the set, in which
	     * case it is not a range and we should add the previous character
	     * as well as the dash.
	     */

	    if (*format == ']') {
		cset->chars[cset->nchars++] = start;
		cset->chars[cset->nchars++] = ch;
	    } else {
		format += Tcl_UtfToUniChar(format, &ch);

		/*
		 * Check to see if the range is in reverse order.
		 */

		if (start < ch) {
		    cset->ranges[cset->nranges].start = start;
		    cset->ranges[cset->nranges].end = ch;
		} else {
		    cset->ranges[cset->nranges].start = ch;
		    cset->ranges[cset->nranges].end = start;
		}
		cset->nranges++;
	    }
	} else {
	    cset->chars[cset->nchars++] = ch;
	}
	format += Tcl_UtfToUniChar(format, &ch);
    }
    return format;
}

/*
 *----------------------------------------------------------------------
 *
 * CharInSet --
 *
 *	Check to see if a character matches the given set.
 *
 * Results:
 *	Returns non-zero if the character matches the given set.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CharInSet(cset, c)
    CharSet *cset;
    int c;			/* Character to test, passed as int because of
				 * non-ANSI prototypes. */
{
    Tcl_UniChar ch = (Tcl_UniChar) c;
    int i, match = 0;
    for (i = 0; i < cset->nchars; i++) {
	if (cset->chars[i] == ch) {
	    match = 1;
	    break;
	}
    }
    if (!match) {
	for (i = 0; i < cset->nranges; i++) {
	    if ((cset->ranges[i].start <= ch)
		    && (ch <= cset->ranges[i].end)) {
		match = 1;
		break;
	    }
	}
    }
    return (cset->exclude ? !match : match);
}

/*
 *----------------------------------------------------------------------
 *
 * ReleaseCharSet --
 *
 *	Free the storage associated with a character set.
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
ReleaseCharSet(cset)
    CharSet *cset;
{
    ckfree((char *)cset->chars);
    if (cset->ranges) {
	ckfree((char *)cset->ranges);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ValidateFormat --
 *
 *	Parse the format string and verify that it is properly formed and that
 *	there are exactly enough variables on the command line.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May place an error in the interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
ValidateFormat(interp, format, numVars, totalSubs)
    Tcl_Interp *interp;		/* Current interpreter. */
    char *format;		/* The format string. */
    int numVars;		/* The number of variables passed to the scan
				 * command. */
    int *totalSubs;		/* The number of variables that will be
				 * required. */
{
#define STATIC_LIST_SIZE 16
    int gotXpg, gotSequential, value, i, flags;
    char *end;
    Tcl_UniChar ch;
    int staticAssign[STATIC_LIST_SIZE];
    int *nassign = staticAssign;
    int objIndex, xpgSize, nspace = STATIC_LIST_SIZE;
    char buf[TCL_UTF_MAX+1];

    /*
     * Initialize an array that records the number of times a variable is
     * assigned to by the format string. We use this to detect if a variable
     * is multiply assigned or left unassigned.
     */

    if (numVars > nspace) {
	nassign = (int*)ckalloc(sizeof(int) * numVars);
	nspace = numVars;
    }
    for (i = 0; i < nspace; i++) {
	nassign[i] = 0;
    }

    xpgSize = objIndex = gotXpg = gotSequential = 0;

    while (*format != '\0') {
	format += Tcl_UtfToUniChar(format, &ch);

	flags = 0;

	if (ch != '%') {
	    continue;
	}
	format += Tcl_UtfToUniChar(format, &ch);
	if (ch == '%') {
	    continue;
	}
	if (ch == '*') {
	    flags |= SCAN_SUPPRESS;
	    format += Tcl_UtfToUniChar(format, &ch);
	    goto xpgCheckDone;
	}

	if ((ch < 0x80) && isdigit(UCHAR(ch))) { /* INTL: "C" locale. */
	    /*
	     * Check for an XPG3-style %n$ specification. Note: there must
	     * not be a mixture of XPG3 specs and non-XPG3 specs in the same
	     * format string.
	     */

	    value = strtoul(format-1, &end, 10); /* INTL: "C" locale. */
	    if (*end != '$') {
		goto notXpg;
	    }
	    format = end+1;
	    format += Tcl_UtfToUniChar(format, &ch);
	    gotXpg = 1;
	    if (gotSequential) {
		goto mixedXPG;
	    }
	    objIndex = value - 1;
	    if ((objIndex < 0) || (numVars && (objIndex >= numVars))) {
		goto badIndex;
	    } else if (numVars == 0) {
		/*
		 * In the case where no vars are specified, the user can
		 * specify %9999$ legally, so we have to consider special
		 * rules for growing the assign array. 'value' is guaranteed
		 * to be > 0.
		 */
		xpgSize = (xpgSize > value) ? xpgSize : value;
	    }
	    goto xpgCheckDone;
	}

    notXpg:
	gotSequential = 1;
	if (gotXpg) {
	mixedXPG:
	    Tcl_SetResult(interp,
		    "cannot mix \"%\" and \"%n$\" conversion specifiers",
		    TCL_STATIC);
	    goto error;
	}

    xpgCheckDone:
	/*
	 * Parse any width specifier.
	 */

	if ((ch < 0x80) && isdigit(UCHAR(ch))) { /* INTL: "C" locale. */
	    value = strtoul(format-1, &format, 10); /* INTL: "C" locale. */
	    flags |= SCAN_WIDTH;
	    format += Tcl_UtfToUniChar(format, &ch);
	}

	/*
	 * Handle any size specifier.
	 */

	switch (ch) {
	case 'l':
	    if (*format == 'l') {
		flags |= SCAN_BIG;
		format += 1;
		format += Tcl_UtfToUniChar(format, &ch);
		break;
	    }
	case 'L':
	    flags |= SCAN_LONGER;
	case 'h':
	    format += Tcl_UtfToUniChar(format, &ch);
	}

	if (!(flags & SCAN_SUPPRESS) && numVars && (objIndex >= numVars)) {
	    goto badIndex;
	}

	/*
	 * Handle the various field types.
	 */

	switch (ch) {
	case 'c':
	    if (flags & SCAN_WIDTH) {
		Tcl_SetResult(interp,
			"field width may not be specified in %c conversion",
			TCL_STATIC);
		goto error;
	    }
	    /*
	     * Fall through!
	     */
	case 'n':
	case 's':
	    if (flags & (SCAN_LONGER|SCAN_BIG)) {
	    invalidFieldSize:
		buf[Tcl_UniCharToUtf(ch, buf)] = '\0';
		Tcl_AppendResult(interp,
			"field size modifier may not be specified in %", buf,
			" conversion", NULL);
		goto error;
	    }
	    /*
	     * Fall through!
	     */
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'i':
	case 'o':
	case 'x':
	    break;
	case 'u':
	    if (flags & SCAN_BIG) {
		Tcl_SetResult(interp,
			"unsigned bignum scans are invalid", TCL_STATIC);
		goto error;
	    }
	    break;
	    /*
	     * Bracket terms need special checking
	     */
	case '[':
	    if (flags & (SCAN_LONGER|SCAN_BIG)) {
		goto invalidFieldSize;
	    }
	    if (*format == '\0') {
		goto badSet;
	    }
	    format += Tcl_UtfToUniChar(format, &ch);
	    if (ch == '^') {
		if (*format == '\0') {
		    goto badSet;
		}
		format += Tcl_UtfToUniChar(format, &ch);
	    }
	    if (ch == ']') {
		if (*format == '\0') {
		    goto badSet;
		}
		format += Tcl_UtfToUniChar(format, &ch);
	    }
	    while (ch != ']') {
		if (*format == '\0') {
		    goto badSet;
		}
		format += Tcl_UtfToUniChar(format, &ch);
	    }
	    break;
	badSet:
	    Tcl_SetResult(interp, "unmatched [ in format string",
		    TCL_STATIC);
	    goto error;
	default:
	    {
		char buf[TCL_UTF_MAX+1];

		buf[Tcl_UniCharToUtf(ch, buf)] = '\0';
		Tcl_AppendResult(interp, "bad scan conversion character \"",
			buf, "\"", NULL);
		goto error;
	    }
	}
	if (!(flags & SCAN_SUPPRESS)) {
	    if (objIndex >= nspace) {
		/*
		 * Expand the nassign buffer. If we are using XPG specifiers,
		 * make sure that we grow to a large enough size. xpgSize is
		 * guaranteed to be at least one larger than objIndex.
		 */

		value = nspace;
		if (xpgSize) {
		    nspace = xpgSize;
		} else {
		    nspace += STATIC_LIST_SIZE;
		}
		if (nassign == staticAssign) {
		    nassign = (void *)ckalloc(nspace * sizeof(int));
		    for (i = 0; i < STATIC_LIST_SIZE; ++i) {
			nassign[i] = staticAssign[i];
		    }
		} else {
		    nassign = (void *)ckrealloc((void *)nassign,
			    nspace * sizeof(int));
		}
		for (i = value; i < nspace; i++) {
		    nassign[i] = 0;
		}
	    }
	    nassign[objIndex]++;
	    objIndex++;
	}
    }

    /*
     * Verify that all of the variable were assigned exactly once.
     */

    if (numVars == 0) {
	if (xpgSize) {
	    numVars = xpgSize;
	} else {
	    numVars = objIndex;
	}
    }
    if (totalSubs) {
	*totalSubs = numVars;
    }
    for (i = 0; i < numVars; i++) {
	if (nassign[i] > 1) {
	    Tcl_SetResult(interp,
		    "variable is assigned by multiple \"%n$\" conversion specifiers",
		    TCL_STATIC);
	    goto error;
	} else if (!xpgSize && (nassign[i] == 0)) {
	    /*
	     * If the space is empty, and xpgSize is 0 (means XPG wasn't used,
	     * and/or numVars != 0), then too many vars were given
	     */

	    Tcl_SetResult(interp,
		    "variable is not assigned by any conversion specifiers",
		    TCL_STATIC);
	    goto error;
	}
    }

    if (nassign != staticAssign) {
	ckfree((char *)nassign);
    }
    return TCL_OK;

  badIndex:
    if (gotXpg) {
	Tcl_SetResult(interp, "\"%n$\" argument index out of range",
		TCL_STATIC);
    } else {
	Tcl_SetResult(interp,
		"different numbers of variable names and field specifiers",
		TCL_STATIC);
    }

  error:
    if (nassign != staticAssign) {
	ckfree((char *)nassign);
    }
    return TCL_ERROR;
#undef STATIC_LIST_SIZE
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ScanObjCmd --
 *
 *	This function is invoked to process the "scan" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_ScanObjCmd(dummy, interp, objc, objv)
    ClientData dummy;    	/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    char *format;
    int numVars, nconversions, totalVars = -1;
    int objIndex, offset, i, result, code;
    long value;
    CONST char *string, *end, *baseString;
    char op = 0;
    int underflow = 0;
    size_t width;
    Tcl_WideInt wideValue;
    Tcl_UniChar ch, sch;
    Tcl_Obj **objs = NULL, *objPtr = NULL;
    int flags;
    char buf[513];		/* Temporary buffer to hold scanned number
				 * strings before they are passed to
				 * strtoul. */
#if 0
    int base = 0;
    long (*fn) _ANSI_ARGS_((char*,void*,int)) = NULL;
#ifndef TCL_WIDE_INT_IS_LONG
    Tcl_WideInt (*lfn) _ANSI_ARGS_((char*,void*,int)) = NULL;
#endif
#endif

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"string format ?varName varName ...?");
	return TCL_ERROR;
    }

    format = Tcl_GetStringFromObj(objv[2], NULL);
    numVars = objc-3;

    /*
     * Check for errors in the format string.
     */

    if (ValidateFormat(interp, format, numVars, &totalVars) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Allocate space for the result objects.
     */

    if (totalVars > 0) {
	objs = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj*) * totalVars);
	for (i = 0; i < totalVars; i++) {
	    objs[i] = NULL;
	}
    }

    string = Tcl_GetStringFromObj(objv[1], NULL);
    baseString = string;

    /*
     * Iterate over the format string filling in the result objects until we
     * reach the end of input, the end of the format string, or there is a
     * mismatch.
     */

    objIndex = 0;
    nconversions = 0;
    while (*format != '\0') {
	int parseFlag = 0;
	format += Tcl_UtfToUniChar(format, &ch);

	flags = 0;

	/*
	 * If we see whitespace in the format, skip whitespace in the string.
	 */

	if (Tcl_UniCharIsSpace(ch)) {
	    offset = Tcl_UtfToUniChar(string, &sch);
	    while (Tcl_UniCharIsSpace(sch)) {
		if (*string == '\0') {
		    goto done;
		}
		string += offset;
		offset = Tcl_UtfToUniChar(string, &sch);
	    }
	    continue;
	}

	if (ch != '%') {
	literal:
	    if (*string == '\0') {
		underflow = 1;
		goto done;
	    }
	    string += Tcl_UtfToUniChar(string, &sch);
	    if (ch != sch) {
		goto done;
	    }
	    continue;
	}

	format += Tcl_UtfToUniChar(format, &ch);
	if (ch == '%') {
	    goto literal;
	}

	/*
	 * Check for assignment suppression ('*') or an XPG3-style assignment
	 * ('%n$').
	 */

	if (ch == '*') {
	    flags |= SCAN_SUPPRESS;
	    format += Tcl_UtfToUniChar(format, &ch);
	} else if ((ch < 0x80) && isdigit(UCHAR(ch))) {	/* INTL: "C" locale. */
	    char *formatEnd;
	    value = strtoul(format-1, &formatEnd, 10);/* INTL: "C" locale. */
	    if (*formatEnd == '$') {
		format = formatEnd+1;
		format += Tcl_UtfToUniChar(format, &ch);
		objIndex = (int) value - 1;
	    }
	}

	/*
	 * Parse any width specifier.
	 */

	if ((ch < 0x80) && isdigit(UCHAR(ch))) {	/* INTL: "C" locale. */
	    width = strtoul(format-1, &format, 10);	/* INTL: "C" locale. */
	    format += Tcl_UtfToUniChar(format, &ch);
	} else {
	    width = 0;
	}

	/*
	 * Handle any size specifier.
	 */

	switch (ch) {
	case 'l':
	    if (*format == 'l') {
		flags |= SCAN_BIG;
		format += 1;
		format += Tcl_UtfToUniChar(format, &ch);
		break;
	    }
	case 'L':
	    flags |= SCAN_LONGER;
	    /*
	     * Fall through so we skip to the next character.
	     */
	case 'h':
	    format += Tcl_UtfToUniChar(format, &ch);
	}

	/*
	 * Handle the various field types.
	 */

	switch (ch) {
	case 'n':
	    if (!(flags & SCAN_SUPPRESS)) {
		objPtr = Tcl_NewIntObj(string - baseString);
		Tcl_IncrRefCount(objPtr);
		objs[objIndex++] = objPtr;
	    }
	    nconversions++;
	    continue;

	case 'd':
	    op = 'i';
	    parseFlag = TCL_PARSE_DECIMAL_ONLY;
#if 0
	    base = 10;
	    fn = (long (*) _ANSI_ARGS_((char*,void*,int)))strtol;
#ifndef TCL_WIDE_INT_IS_LONG
	    lfn = (Tcl_WideInt (*)_ANSI_ARGS_((char*,void*,int)))strtoll;
#endif
#endif
	    break;
	case 'i':
	    op = 'i';
	    parseFlag = TCL_PARSE_SCAN_PREFIXES;
#if 0
	    base = 0;
	    fn = (long (*)_ANSI_ARGS_((char*,void*,int)))strtol;
#ifndef TCL_WIDE_INT_IS_LONG
	    lfn = (Tcl_WideInt (*)_ANSI_ARGS_((char*,void*,int)))strtoll;
#endif
#endif
	    break;
	case 'o':
	    op = 'i';
	    parseFlag = TCL_PARSE_OCTAL_ONLY | TCL_PARSE_SCAN_PREFIXES;
#if 0
	    base = 8;
	    fn = (long (*)_ANSI_ARGS_((char*,void*,int)))strtoul;
#ifndef TCL_WIDE_INT_IS_LONG
	    lfn = (Tcl_WideInt (*)_ANSI_ARGS_((char*,void*,int)))strtoull;
#endif
#endif
	    break;
	case 'x':
	    op = 'i';
	    parseFlag = TCL_PARSE_HEXADECIMAL_ONLY;
#if 0
	    base = 16;
	    fn = (long (*)_ANSI_ARGS_((char*,void*,int)))strtoul;
#ifndef TCL_WIDE_INT_IS_LONG
	    lfn = (Tcl_WideInt (*)_ANSI_ARGS_((char*,void*,int)))strtoull;
#endif
#endif
	    break;
	case 'u':
	    op = 'i';
	    flags |= SCAN_UNSIGNED;
#if 0
	    base = 10;
	    fn = (long (*)_ANSI_ARGS_((char*,void*,int)))strtoul;
#ifndef TCL_WIDE_INT_IS_LONG
	    lfn = (Tcl_WideInt (*)_ANSI_ARGS_((char*,void*,int)))strtoull;
#endif
#endif
	    break;

	case 'f':
	case 'e':
	case 'g':
	    op = 'f';
	    break;

	case 's':
	    op = 's';
	    break;

	case 'c':
	    op = 'c';
	    flags |= SCAN_NOSKIP;
	    break;
	case '[':
	    op = '[';
	    flags |= SCAN_NOSKIP;
	    break;
	}

	/*
	 * At this point, we will need additional characters from the string
	 * to proceed.
	 */

	if (*string == '\0') {
	    underflow = 1;
	    goto done;
	}

	/*
	 * Skip any leading whitespace at the beginning of a field unless the
	 * format suppresses this behavior.
	 */

	if (!(flags & SCAN_NOSKIP)) {
	    while (*string != '\0') {
		offset = Tcl_UtfToUniChar(string, &sch);
		if (!Tcl_UniCharIsSpace(sch)) {
		    break;
		}
		string += offset;
	    }
	    if (*string == '\0') {
		underflow = 1;
		goto done;
	    }
	}

	/*
	 * Perform the requested scanning operation.
	 */

	switch (op) {
	case 's':
	    /*
	     * Scan a string up to width characters or whitespace.
	     */

	    if (width == 0) {
		width = (size_t) ~0;
	    }
	    end = string;
	    while (*end != '\0') {
		offset = Tcl_UtfToUniChar(end, &sch);
		if (Tcl_UniCharIsSpace(sch)) {
		    break;
		}
		end += offset;
		if (--width == 0) {
		    break;
		}
	    }
	    if (!(flags & SCAN_SUPPRESS)) {
		objPtr = Tcl_NewStringObj(string, end-string);
		Tcl_IncrRefCount(objPtr);
		objs[objIndex++] = objPtr;
	    }
	    string = end;
	    break;

	case '[': {
	    CharSet cset;

	    if (width == 0) {
		width = (size_t) ~0;
	    }
	    end = string;

	    format = BuildCharSet(&cset, format);
	    while (*end != '\0') {
		offset = Tcl_UtfToUniChar(end, &sch);
		if (!CharInSet(&cset, (int)sch)) {
		    break;
		}
		end += offset;
		if (--width == 0) {
		    break;
		}
	    }
	    ReleaseCharSet(&cset);

	    if (string == end) {
		/*
		 * Nothing matched the range, stop processing.
		 */
		goto done;
	    }
	    if (!(flags & SCAN_SUPPRESS)) {
		objPtr = Tcl_NewStringObj(string, end-string);
		Tcl_IncrRefCount(objPtr);
		objs[objIndex++] = objPtr;
	    }
	    string = end;

	    break;
	}
	case 'c':
	    /*
	     * Scan a single Unicode character.
	     */

	    string += Tcl_UtfToUniChar(string, &sch);
	    if (!(flags & SCAN_SUPPRESS)) {
		objPtr = Tcl_NewIntObj((int)sch);
		Tcl_IncrRefCount(objPtr);
		objs[objIndex++] = objPtr;
	    }
	    break;

	case 'i':
	    /*
	     * Scan an unsigned or signed integer.
	     */

#if 0
	    if ((width == 0) || (width > sizeof(buf) - 1)) {
		width = sizeof(buf) - 1;
	    }
	    flags |= SCAN_SIGNOK | SCAN_NODIGITS | SCAN_NOZERO;
	    for (end = buf; width > 0; width--) {
		switch (*string) {
		    /*
		     * The 0 digit has special meaning at the beginning of a
		     * number. If we are unsure of the base, it indicates that
		     * we are in base 8 or base 16 (if it is followed by an
		     * 'x').
		     *
		     * 8.1 - 8.3.4 incorrectly handled 0x... base-16 cases for
		     * %x by not reading the 0x as the auto-prelude for
		     * base-16. [Bug #495213]
		     */
		case '0':
		    if (base == 0) {
			base = 8;
			flags |= SCAN_XOK;
		    }
		    if (base == 16) {
			flags |= SCAN_XOK;
		    }
		    if (flags & SCAN_NOZERO) {
			flags &= ~(SCAN_SIGNOK | SCAN_NODIGITS | SCAN_NOZERO);
		    } else {
			flags &= ~(SCAN_SIGNOK | SCAN_XOK | SCAN_NODIGITS);
		    }
		    goto addToInt;

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		    if (base == 0) {
			base = 10;
		    }
		    flags &= ~(SCAN_SIGNOK | SCAN_XOK | SCAN_NODIGITS);
		    goto addToInt;

		case '8': case '9':
		    if (base == 0) {
			base = 10;
		    }
		    if (base <= 8) {
			break;
		    }
		    flags &= ~(SCAN_SIGNOK | SCAN_XOK | SCAN_NODIGITS);
		    goto addToInt;

		case 'A': case 'B': case 'C':
		case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c':
		case 'd': case 'e': case 'f':
		    if (base <= 10) {
			break;
		    }
		    flags &= ~(SCAN_SIGNOK | SCAN_XOK | SCAN_NODIGITS);
		    goto addToInt;

		case '+': case '-':
		    if (flags & SCAN_SIGNOK) {
			flags &= ~SCAN_SIGNOK;
			goto addToInt;
		    }
		    break;

		case 'x': case 'X':
		    if ((flags & SCAN_XOK) && (end == buf+1)) {
			base = 16;
			flags &= ~SCAN_XOK;
			goto addToInt;
		    }
		    break;
		}

		/*
		 * We got an illegal character so we are done accumulating.
		 */

		break;

	    addToInt:
		/*
		 * Add the character to the temporary buffer.
		 */

		*end++ = *string++;
		if (*string == '\0') {
		    break;
		}
	    }

	    /*
	     * Check to see if we need to back up because we only got a sign
	     * or a trailing x after a 0.
	     */

	    if (flags & SCAN_NODIGITS) {
		if (*string == '\0') {
		    underflow = 1;
		}
		goto done;
	    } else if (end[-1] == 'x' || end[-1] == 'X') {
		end--;
		string--;
	    }

	    /*
	     * Scan the value from the temporary buffer. If we are returning a
	     * large unsigned value, we have to convert it back to a string
	     * since Tcl only supports signed values.
	     */

	    if (!(flags & SCAN_SUPPRESS)) {
		*end = '\0';
#ifndef TCL_WIDE_INT_IS_LONG
		if (flags & SCAN_LONGER) {
		    wideValue = (Tcl_WideInt) (*lfn)(buf, NULL, base);
		    if ((flags & SCAN_UNSIGNED) && (wideValue < 0)) {
			/* INTL: ISO digit */
			sprintf(buf, "%" TCL_LL_MODIFIER "u",
				(Tcl_WideUInt)wideValue);
			objPtr = Tcl_NewStringObj(buf, -1);
		    } else {
			objPtr = Tcl_NewWideIntObj(wideValue);
		    }
		} else {
#endif /* !TCL_WIDE_INT_IS_LONG */
		    value = (long) (*fn)(buf, NULL, base);
		    if ((flags & SCAN_UNSIGNED) && (value < 0)) {
			sprintf(buf, "%lu", value);	/* INTL: ISO digit */
			objPtr = Tcl_NewStringObj(buf, -1);
		    } else if ((flags & SCAN_LONGER)
			    || (unsigned long) value > UINT_MAX) {
			objPtr = Tcl_NewLongObj(value);
		    } else {
			objPtr = Tcl_NewIntObj(value);
		    }
#ifndef TCL_WIDE_INT_IS_LONG
		}
#endif
		Tcl_IncrRefCount(objPtr);
		objs[objIndex++] = objPtr;
	    }

	    break;
#else
	    objPtr = Tcl_NewLongObj(0);
	    Tcl_IncrRefCount(objPtr);
	    if (width == 0) {
		width = -1;
	    }
	    if (TclParseNumber(NULL, objPtr, NULL, string, width, &end,
		    TCL_PARSE_INTEGER_ONLY | parseFlag) != TCL_OK) {
		Tcl_DecrRefCount(objPtr);
		/* TODO: set underflow?  test scan-4.44 */
		goto done;
	    }
	    string = end;
	    if (flags & SCAN_SUPPRESS) {
		Tcl_DecrRefCount(objPtr);
		break;
	    }
	    if (flags & SCAN_LONGER) {
		if (Tcl_GetWideIntFromObj(NULL, objPtr, &wideValue) != TCL_OK) {
		    wideValue = ~(Tcl_WideUInt)0 >> 1;	/* WIDE_MAX */
		    if (Tcl_GetString(objPtr)[0] == '-') {
			wideValue++;	/* WIDE_MAX + 1 = WIDE_MIN */
		    }
		}
		if ((flags & SCAN_UNSIGNED) && (wideValue < 0)) {
		    sprintf(buf, "%" TCL_LL_MODIFIER "u",
			    (Tcl_WideUInt)wideValue);
		    Tcl_SetStringObj(objPtr, buf, -1);
		} else {
		    Tcl_SetWideIntObj(objPtr, wideValue);
		}
	    } else if (!(flags & SCAN_BIG)) {
		if (Tcl_GetLongFromObj(NULL, objPtr, &value) != TCL_OK) {
		    if (Tcl_GetString(objPtr)[0] == '-') {
			value = LONG_MIN;
		    } else {
			value = LONG_MAX;
		    }
		}
		if ((flags & SCAN_UNSIGNED) && (value < 0)) {
		    sprintf(buf, "%lu", value);	/* INTL: ISO digit */
		    Tcl_SetStringObj(objPtr, buf, -1);
		} else {
		    Tcl_SetLongObj(objPtr, value);
		}
	    }
	    objs[objIndex++] = objPtr;
	    break;
#endif

	case 'f':
	    /*
	     * Scan a floating point number
	     */

	    objPtr = Tcl_NewDoubleObj(0.0);
	    Tcl_IncrRefCount(objPtr);
	    if (width == 0) {
		width = -1;
	    }
	    if (TclParseNumber(NULL, objPtr, NULL, string, width, &end,
			       TCL_PARSE_DECIMAL_ONLY) != TCL_OK) {
		/* TODO: set underflow?  test scan-4.55 */
		Tcl_DecrRefCount(objPtr);
		goto done;
	    } else if (flags & SCAN_SUPPRESS) {
		Tcl_DecrRefCount(objPtr);
		string = end;
	    } else {
		double dvalue;
		if (Tcl_GetDoubleFromObj(NULL, objPtr, &dvalue) != TCL_OK) {
#ifdef ACCEPT_NAN
		    if (objPtr->typePtr == &tclDoubleType) {
			dValue = objPtr->internalRep.doubleValue;
		    } else
#endif
		    {
		    Tcl_DecrRefCount(objPtr);
		    goto done;
		    }
		}
		Tcl_SetDoubleObj(objPtr, dvalue);
		objs[objIndex++] = objPtr;
		string = end;
	    }
	}
	nconversions++;
    }

  done:
    result = 0;
    code = TCL_OK;

    if (numVars) {
	/*
	 * In this case, variables were specified (classic scan).
	 */

	for (i = 0; i < totalVars; i++) {
	    if (objs[i] == NULL) {
		continue;
	    }
	    result++;
	    if (Tcl_ObjSetVar2(interp, objv[i+3], NULL, objs[i], 0) == NULL) {
		Tcl_AppendResult(interp, "couldn't set variable \"",
			TclGetString(objv[i+3]), "\"", (char *) NULL);
		code = TCL_ERROR;
	    }
	    Tcl_DecrRefCount(objs[i]);
	}
    } else {
	/*
	 * Here no vars were specified, we want a list returned (inline scan)
	 */

	objPtr = Tcl_NewObj();
	for (i = 0; i < totalVars; i++) {
	    if (objs[i] != NULL) {
		Tcl_ListObjAppendElement(NULL, objPtr, objs[i]);
		Tcl_DecrRefCount(objs[i]);
	    } else {
		/*
		 * More %-specifiers than matching chars, so we just spit out
		 * empty strings for these.
		 */

		Tcl_ListObjAppendElement(NULL, objPtr, Tcl_NewObj());
	    }
	}
    }
    if (objs != NULL) {
	ckfree((char*) objs);
    }
    if (code == TCL_OK) {
	if (underflow && (nconversions == 0)) {
	    if (numVars) {
		objPtr = Tcl_NewIntObj(-1);
	    } else {
		if (objPtr) {
		    Tcl_SetListObj(objPtr, 0, NULL);
		} else {
		    objPtr = Tcl_NewObj();
		}
	    }
	} else if (numVars) {
	    objPtr = Tcl_NewIntObj(result);
	}
	Tcl_SetObjResult(interp, objPtr);
    }
    return code;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
