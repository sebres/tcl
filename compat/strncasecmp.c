/* 
 * strncasecmp.c --
 *
 *	Source code for the "strncasecmp" library routine.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: strncasecmp.c,v 1.3 2007/04/16 13:36:34 dkf Exp $
 */

#include "tclPort.h"

/*
 * This array is designed for mapping upper and lower case letter together for
 * a case independent comparison. The mappings are based upon ASCII character
 * sequences.
 */

static unsigned char charmap[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xe1, 0xe2, 0xe3, 0xe4, 0xc5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

/*
 * Here are the prototypes just in case they are not included in tclPort.h.
 */

int		strncasecmp(CONST char *s1, CONST char *s2, size_t n);
int		strcasecmp(CONST char *s1, CONST char *s2);

/*
 *----------------------------------------------------------------------
 *
 * strcasecmp --
 *
 *	Compares two strings, ignoring case differences.
 *
 * Results:
 *	Compares two null-terminated strings s1 and s2, returning -1, 0, or 1
 *	if s1 is lexicographically less than, equal to, or greater than s2.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
strcasecmp(
    CONST char *s1,		/* First string. */
    CONST char *s2)		/* Second string. */
{
    unsigned char u1, u2;

    for ( ; ; s1++, s2++) {
	u1 = (unsigned char) *s1;
	u2 = (unsigned char) *s2;
	if ((u1 == '\0') || (charmap[u1] != charmap[u2])) {
	    break;
	}
    }
    return charmap[u1] - charmap[u2];
}

/*
 *----------------------------------------------------------------------
 *
 * strncasecmp --
 *
 *	Compares two strings, ignoring case differences.
 *
 * Results:
 *	Compares up to length chars of s1 and s2, returning -1, 0, or 1 if s1
 *	is lexicographically less than, equal to, or greater than s2 over
 *	those characters.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
strncasecmp(
    CONST char *s1,		/* First string. */
    CONST char *s2,		/* Second string. */
    size_t length)		/* Maximum number of characters to compare
				 * (stop earlier if the end of either string
				 * is reached). */
{
    unsigned char u1, u2;

    for (; length != 0; length--, s1++, s2++) {
	u1 = (unsigned char) *s1;
	u2 = (unsigned char) *s2;
	if (charmap[u1] != charmap[u2]) {
	    return charmap[u1] - charmap[u2];
	}
	if (u1 == '\0') {
	    return 0;
	}
    }
    return 0;
}
