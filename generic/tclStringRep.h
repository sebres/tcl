/*
 * tclStringRep.h --
 *
 *  This file contains the definition of internal representations of a string
 *  and macros to access it.
 *
 *  Conceptually, a string is a sequence of Unicode code points. Internally
 *  it may be stored in an encoding form such as a modified version of UTF-8
 *  or UTF-16 (when TCL_UTF_MAX=3) or UTF-32.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLSTRINGREP
#define _TCLSTRINGREP


/*
 * The following structure is the internal rep for a String object. It keeps
 * track of how much memory has been used and how much has been allocated for
 * the various representations to enable growing and shrinking of
 * the String object with fewer mallocs. To optimize string
 * length and indexing operations, this structure also stores the number of
 * code points (independent of encoding form) once that value has been computed.
 */

typedef struct {
    Tcl_Size numChars;		/* The number of chars in the string.
				 * TCL_INDEX_NONE means this value has not been
				 * calculated. Any other means that there is a valid
				 * Unicode rep, or that the number of UTF bytes ==
				 * the number of chars. */
    Tcl_Size allocated;		/* The amount of space allocated for
				 * the UTF-8 string. Does not include nul
				 * terminator so actual allocation is
				 * (allocated+1). */
    Tcl_Size maxChars;		/* Max number of chars that can fit in the
				 * space allocated for the Unicode array. */
    int hasUnicode;		/* Boolean determining whether the string has
				 * a Tcl_UniChar representation. */
    unsigned short unicode[TCLFLEXARRAY];	/* The array of Tcl_UniChar units.
				 * The actual size of this field depends on
				 * the maxChars field above. */
} String;

/* Limit on string lengths. The -1 because limit does not include the nul */
#define STRING_MAXCHARS \
    (Tcl_Size)(((size_t)UINT_MAX - offsetof(String, unicode))/sizeof(unsigned short) - 1)
#define STRING_SIZE(numChars) \
    (offsetof(String, unicode) + sizeof(unsigned short) + ((numChars) * sizeof(unsigned short)))
#define stringCheckLimits(numChars) \
    do {								\
	if ((numChars) < 0 || (numChars) > STRING_MAXCHARS) {		\
	    Tcl_Panic("max length for a Tcl unicode value (%d chars) exceeded", \
		      STRING_MAXCHARS);					\
	}								\
    } while (0)
#define stringAttemptAlloc(numChars) \
    (String *) attemptckalloc(STRING_SIZE(numChars))
#define stringAlloc(numChars) \
    (String *) ckalloc(STRING_SIZE(numChars))
#define stringRealloc(ptr, numChars) \
    (String *) ckrealloc((ptr), STRING_SIZE(numChars))
#define stringAttemptRealloc(ptr, numChars) \
    (String *) attemptckrealloc((ptr), STRING_SIZE(numChars))
#define GET_STRING(objPtr) \
    ((String *) (objPtr)->internalRep.twoPtrValue.ptr1)
#define SET_STRING(objPtr, stringPtr) \
    ((objPtr)->internalRep.twoPtrValue.ptr2 = NULL),			\
    ((objPtr)->internalRep.twoPtrValue.ptr1 = (void *) (stringPtr))

#endif /*  _TCLSTRINGREP */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
