/*
 *----------------------------------------------------------------------
 *
 * tclStrToD.c --
 *
 *	This file contains a TclStrToD procedure that handles conversion of
 *	string to double, with correct rounding even where extended precision
 *	is needed to achieve that.  It also contains a TclDoubleDigits
 *	procedure that handles conversion of double to string (at least the
 *	significand), and several utility functions for interconverting
 *	'double' and the integer types.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclStrToD.c,v 1.1.2.25 2005/08/22 15:48:26 dgp Exp $
 *
 *----------------------------------------------------------------------
 */

#include <tclInt.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <tommath.h>

#if 0
/* Hack is out of date.  tclInt.h unconditionally #include's errno.h
 * (via tclPort.h).
 */
/*
 * The stuff below is a bit of a hack so that this file can be used in
 * environments that include no UNIX, i.e. no errno: just arrange to use the
 * errno from tclExecute.c here.
 */

#ifdef TCL_GENERIC_ONLY
#   define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
extern int errno;			/* Use errno from tclExecute.c. */
#   define ERANGE 34
#endif
#endif

#if (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MAX_EXP == 1024)
#   define IEEE_FLOATING_POINT
#endif

/*
 * gcc on x86 needs access to rounding controls.  It is tempting to include
 * fpu_control.h, but that file exists only on Linux; it is missing on Cygwin
 * and MinGW. Most gcc-isms and ix86-isms are factored out here.
 */

#if defined(__GNUC__) && defined(__i386)
typedef unsigned int fpu_control_t __attribute__ ((__mode__ (__HI__)));
#define _FPU_GETCW(cw) __asm__ __volatile__ ("fnstcw %0" : "=m" (*&cw))
#define _FPU_SETCW(cw) __asm__ __volatile__ ("fldcw %0" : : "m" (*&cw))
#   define FPU_IEEE_ROUNDING	0x027f
#   define ADJUST_FPU_CONTROL_WORD
#endif

/*
 * HP's PA_RISC architecture uses 7ff4000000000000 to represent a quiet NaN.
 * Everyone else uses 7ff8000000000000.  (Why, HP, why?)
 */

#ifdef __hppa
#   define NAN_START 0x7ff4
#   define NAN_MASK (((Tcl_WideUInt) 1) << 50)
#else
#   define NAN_START 0x7ff8
#   define NAN_MASK (((Tcl_WideUInt) 1) << 51)
#endif

/* The powers of ten that can be represented exactly as wide integers */

static int maxpow10_wide;
static Tcl_WideUInt *pow10_wide;

/* The number of decimal digits that fit in an mp_digit */

static int log10_DIGIT_MAX;

/* The powers of ten that can be represented exactly as IEEE754 doubles. */

#define MAXPOW 22
static double pow10 [MAXPOW+1];

static int mmaxpow;		/* Largest power of ten that can be
				 * represented exactly in a 'double'. */

/* Inexact higher powers of ten */

static CONST double pow_10_2_n [] = {
    1.0,
    100.0,
    10000.0,
    1.0e+8,
    1.0e+16,
    1.0e+32,
    1.0e+64,
    1.0e+128,
    1.0e+256
};

/* Logarithm of the floating point radix. */

static int log2FLT_RADIX;

/* Number of bits in a double's significand */

static int mantBits;

/* Table of powers of 5**(2**n), up to 5**256 */

static mp_int pow5[9];

/* The smallest representable double */

static double tiny;

/* The maximum number of digits to the left of the decimal point of a
 * double. */

static int maxDigits;

/* The maximum number of digits to the right of the decimal point in a
 * double. */

static int minDigits;

/* Number of mp_digit's needed to hold the significand of a double */

static int mantDIGIT;

/* Static functions defined in this file */

static int AccumulateDecimalDigit _ANSI_ARGS_((unsigned, int, 
					       Tcl_WideUInt*, mp_int*, int));
static double MakeLowPrecisionDouble _ANSI_ARGS_((int signum,
						  Tcl_WideUInt significand,
						  int nSigDigs,
						  int exponent));
static double MakeHighPrecisionDouble _ANSI_ARGS_((int signum,
						   mp_int* significand,
						   int nSigDigs,
						   int exponent));
static double MakeNaN _ANSI_ARGS_(( int signum, Tcl_WideUInt tag ));
static double RefineApproximation _ANSI_ARGS_((double approx,
					       mp_int* exactSignificand,
					       int exponent));
static double RefineResult _ANSI_ARGS_((double approx, CONST char* start,
					int nDigits, long exponent));
static double ParseNaN _ANSI_ARGS_(( int signum, CONST char** end ));
static double BignumToBiasedFrExp _ANSI_ARGS_(( mp_int* big, int* machexp ));
static double Pow10TimesFrExp _ANSI_ARGS_(( int exponent,
					    double fraction,
					    int* machexp ));
static double SafeLdExp _ANSI_ARGS_(( double fraction, int exponent ));


/*
 *----------------------------------------------------------------------
 *
 * TclParseNumber --
 *
 *	Place a "numeric" internal representation on a Tcl object.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Stores an internal representation appropriate to the string.
 *	The internal representation may be an integer, a wide integer,
 *	a bignum, or a double.
 *
 * TclMakeObjNumeric is called as a common scanner in routines
 * that expect numbers in Tcl_Obj's.  It scans the string representation
 * of a given Tcl_Obj and stores an internal rep that represents
 * a "canonical" version of its numeric value.  The value of the
 * canonicalization is that a routine can determine simply by
 * examining the type pointer whether an object LooksLikeInt,
 * what size of integer is needed to hold it, and similar questions,
 * and never needs to refer back to the string representation, even
 * for "impure" objects.
 *
 * The 'strPtr' and 'endPtrPtr' arguments allow for recognizing a number
 * that is in a substring of a Tcl_Obj, for example a screen metric or
 * "end-" index.  If 'strPtr' is not NULL, it designates where the
 * number begins within the string.  (The default is the start of
 * objPtr's string rep, which will be constructed if necessary.)
 *
 * If 'strPtr' is supplied, 'objPtr' may be NULL.  In this case,
 * no internal representation will be generated; instead, the routine
 * will simply check for a syntactically correct number, returning
 * TCL_OK or TCL_ERROR as appropriate, and setting *endPtrPtr if
 * necessary.
 *
 * If 'endPtrPtr' is not NULL, it designates the first character
 * after the scanned number.  In this case, successfully recognizing
 * any digits will yield a return code of TCL_OK.  Only in the case
 * where no leading string of 'strPtr' (or of objPtr's internal rep)
 * represents a number will TCL_ERROR be returned.
 *
 * When only a partial string is being recognized, it is the caller's
 * responsibility to destroy the internal representation, or at
 * least change its type.  Failure to do so will lead to subsequent
 * problems where a string that does not represent a number will
 * be recognized as one because it has a numeric internal representation.
 *
 *----------------------------------------------------------------------
 */

int
TclParseNumber( Tcl_Interp* interp,
				/* Tcl interpreter for error reporting.
				 * May be NULL */
		Tcl_Obj* objPtr,
				/* Object to receive the internal rep */
		CONST char* type,
				/* Type of number being parsed ("integer",
				 * "wide integer", etc. */
		CONST char* string,
				/* Pointer to the start of the string to
				 * scan, see above */
		size_t length,	/* Maximum length of the string to scan,
				 * see above. */
		CONST char** endPtrPtr )
				/* (Output) pointer to the end of the
				 * scanned number, see above */
{

    enum State {
	INITIAL, SIGNUM, ZERO, ZERO_X, 
	HEXADECIMAL, OCTAL, BAD_OCTAL, DECIMAL, 
	LEADING_RADIX_POINT, FRACTION,
	EXPONENT_START, EXPONENT_SIGNUM, EXPONENT,
	sI, sIN, sINF, sINFI, sINFIN, sINFINI, sINFINIT, sINFINITY
#ifdef IEEE_FLOATING_POINT
	, sN, sNA, sNAN, sNANPAREN, sNANHEX, sNANFINISH
#endif
    } state = INITIAL;
    enum State acceptState = INITIAL;

    int signum = 0;		/* Sign of the number being parsed */
    Tcl_WideUInt significandWide = 0;
				/* Significand of the number being
				 * parsed (if no overflow) */
    mp_int significandBig;	/* Significand of the number being
				 * parsed (if it overflows significandWide) */
    int significandOverflow = 0;
				/* Flag==1 iff significandBig is used */
    Tcl_WideUInt octalSignificandWide = 0;
				/* Significand of an octal number; needed
				 * because we don't know whether a number
				 * with a leading zero is octal or decimal
				 * until we've scanned forward to a '.' or
				 * 'e' */
    mp_int octalSignificandBig;	/* Significand of octal number once
				 * octalSignificandWide overflows */
    int octalSignificandOverflow = 0;
				/* Flag==1 if octalSignificandBig is used */
    int numSigDigs = 0;		/* Number of significant digits in the
				 * decimal significand */
    int numTrailZeros = 0;	/* Number of trailing zeroes at the
				 * current point in the parse. */
    int numDigitsAfterDp = 0;	/* Number of digits scanned after the
				 * decimal point */
    int exponentSignum = 0;	/* Signum of the exponent of a floating
				 * point number */
    long exponent = 0;		/* Exponent of a floating point number */
    CONST char* p;		/* Pointer to next character to scan */
    size_t len;			/* Number of characters remaining after p */
    CONST char* acceptPoint;	/* Pointer to position after last character
				 * in an acceptable number */
    size_t acceptLen;		/* Number of characters following that point */
    int status = TCL_OK;	/* Status to return to caller */
    char d;			/* Last hexadecimal digit scanned */
    int shift = 0;		/* Amount to shift when accumulating binary */

    /* 
     * Initialize string to start of the object's string rep if
     * the caller didn't pass anything else.
     */

    if ( string == NULL ) {
	string = Tcl_GetStringFromObj( objPtr, NULL );
    }

    p = string;
    len = length;
    acceptPoint = p;
    acceptLen = len;
    while ( 1 ) {
	char c = len ? *p : '\0';
	switch (state) {

	case INITIAL:
	    /*
	     * Initial state. Acceptable characters are +, -, digits,
	     * period, I, N, and whitespace.
	     */
	    if (isspace(UCHAR(c))) {
		break;
	    } else if (c == '+') {
		state = SIGNUM;
		break;
	    } else if (c == '-') {
		signum = 1;
		state = SIGNUM;
		break;
	    } 
	    /* FALLTHROUGH */
	    
	case SIGNUM:
	    /*
	     * Scanned a leading + or -. Acceptable characters are
	     * digits, period, I, and N.
	     */
	    if (c == '0') {
		state = ZERO;
		break;
	    } else if (isdigit(UCHAR(c))) {
		significandWide = c - '0';
		numSigDigs = 1;
		state = DECIMAL;
		break;
	    } else if (c == '.') {
		state = LEADING_RADIX_POINT;
		break;
	    } else if (c == 'I' || c == 'i') {
		state = sI;
		break;
#ifdef IEEE_FLOATING_POINT
	    } else if (c == 'N' || c == 'n') {
		state = sN;
		break;
#endif
	    }
	    goto endgame;

	case ZERO:
	    /*
	     * Scanned a leading zero (perhaps with a + or -).
	     * Acceptable inputs are digits, period, X, and E.
	     * If 8 or 9 is encountered, the number can't be
	     * octal.  This state and the OCTAL state differ only
	     * in whether they recognize 'X'.
	     */
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if (c == 'x' || c == 'X') {
		state = ZERO_X;
		break;
	    }
	    /* FALLTHROUGH */

	case OCTAL:
	    /*
	     * Scanned an optional + or -, followed by a string of
	     * octal digits.  Acceptable inputs are more digits,
	     * period, or E.  If 8 or 9 is encountered, commit to
	     * floating point.
	     */
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if (c == '0') {
		++numTrailZeros;
		state = OCTAL;
		break;
	    } else if (c >= '1' && c <= '7') {
		if (objPtr != NULL) {
		    shift = 3 * (numTrailZeros + 1);
		    significandOverflow = 
			AccumulateDecimalDigit((unsigned)(c-'0'),
					       numTrailZeros, 
					       &significandWide,
					       &significandBig, 
					       significandOverflow);
		    
		    if (!octalSignificandOverflow) {
			/*
			 * Shifting by more bits than are in the value being
			 * shifted is at least de facto nonportable.  Check
			 * for too large shifts first.
			 */
			if ((octalSignificandWide != 0) 
				&& ((shift >= CHAR_BIT*sizeof(Tcl_WideUInt)) 
				|| (octalSignificandWide 
				> (~(Tcl_WideUInt)0 >> shift)))) {
			    octalSignificandOverflow = 1;
			    TclBNInitBignumFromWideUInt(&octalSignificandBig,
							octalSignificandWide);
			}
		    }
		    if (!octalSignificandOverflow) {
			octalSignificandWide
			    = (octalSignificandWide << shift) + (c - '0');
		    } else {
			mp_mul_2d(&octalSignificandBig, shift,
				  &octalSignificandBig);
			mp_add_d(&octalSignificandBig, (mp_digit)(c - '0'),
				 &octalSignificandBig);
		    }
		}
		if ( numSigDigs != 0 ) {
		    numSigDigs += ( numTrailZeros + 1 );
		} else {
		    numSigDigs = 1;
		}
		numTrailZeros = 0;
		state = OCTAL;
		break;
	    }
	    /* FALLTHROUGH */

	case BAD_OCTAL:
	    /*
	     * Scanned a number with a leading zero that contains an
	     * 8, 9, radix point or E.  This is an invalid octal number,
	     * but might still be floating point.  
	     */
	    if (c == '0') {
		++numTrailZeros;
		state = BAD_OCTAL;
		break;
	    } else if (isdigit(UCHAR(c))) {
		if (objPtr != NULL) {
		    significandOverflow = 
			AccumulateDecimalDigit((unsigned)(c-'0'),
					       numTrailZeros, 
					       &significandWide,
					       &significandBig, 
					       significandOverflow);
		}
		if ( numSigDigs != 0 ) {
		    numSigDigs += ( numTrailZeros + 1 );
		} else {
		    numSigDigs = 1;
		}
		numTrailZeros = 0;
		state = BAD_OCTAL;
		break;
	    } else if (c == '.') {
		state = FRACTION;
		break;
	    } else if (c == 'E' || c == 'e') {
		state = EXPONENT_START;
		break;
	    } 
	    goto endgame;

	    /*
	     * Scanned 0x.  If state is HEXADECIMAL, scanned at least
	     * one character following the 0x.  The only acceptable
	     * inputs are hexadecimal digits.
	     */
	case HEXADECIMAL:
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    /* FALLTHROUGH */
	case ZERO_X:
	    if (c == '0') {
		++numTrailZeros;
		state = HEXADECIMAL;
		break;
	    } else if (isdigit(UCHAR(c))) {
		d = (c-'0');
	    } else if (c >= 'A' && c <= 'F') {
		d = (c-'A'+10);
	    } else if (c >= 'a' && c <= 'f') {
		d = (c-'a'+10);
	    } else {
		goto endgame;
	    }
	    if (objPtr != NULL) {
		shift = 4 * (numTrailZeros + 1);
		if (!significandOverflow) {
		    /*
		     * Shifting by more bits than are in the value being
		     * shifted is at least de facto nonportable.  Check
		     * for too large shifts first.
		     */
		    if (significandWide != 0 
			    && (shift >= CHAR_BIT*sizeof(Tcl_WideUInt)
			    || significandWide > (~(Tcl_WideUInt)0 >> shift))) {
			significandOverflow = 1;
			TclBNInitBignumFromWideUInt(&significandBig,
						    significandWide);
		    }
		}
		if (!significandOverflow) {
		    significandWide
			= (significandWide << shift) + d;
		} else {
		    mp_mul_2d(&significandBig, shift,
			      &significandBig);
		    mp_add_d(&significandBig, (mp_digit) d,
			     &significandBig);
		}
	    }
	    numTrailZeros = 0;
	    state = HEXADECIMAL;
	    break;

	case DECIMAL:
	    /*
	     * Scanned an optional + or - followed by a string of
	     * decimal digits.
	     */
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if (c == '0') {
		++numTrailZeros;
		state = DECIMAL;
		break;
	    } else if (isdigit(UCHAR(c))) {
		if (objPtr != NULL) {
		    significandOverflow = 
			AccumulateDecimalDigit((unsigned)(c - '0'), 
					       numTrailZeros, 
					       &significandWide,
					       &significandBig, 
					       significandOverflow);
		}
		if ( numSigDigs != 0 ) {
		    numSigDigs += ( numTrailZeros + 1 );
		} else {
		    numSigDigs = 1;
		}
		numTrailZeros = 0;
		state = DECIMAL;
		break;
	    } else if (c == '.') {
		state = FRACTION;
		break;
	    } else if (c == 'E' || c == 'e') {
		state = EXPONENT_START;
		break;
	    }
	    goto endgame;

	    /* 
	     * Found a decimal point.  If no digits have yet been scanned,
	     * E is not allowed; otherwise, it introduces the exponent.
	     * If at least one digit has been found, we have a possible
	     * complete number.
	     */
	case FRACTION:
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if (c == 'E' || c=='e') {
		state = EXPONENT_START;
		break;
	    }
	    /* FALLTHROUGH */
	case LEADING_RADIX_POINT:
	    if (c == '0') {
		++numDigitsAfterDp;
		++numTrailZeros;
		state = FRACTION;
		break;
	    } else if (isdigit(UCHAR(c))) {
		++numDigitsAfterDp;
		if (objPtr != NULL) {
		    significandOverflow = 
			AccumulateDecimalDigit((unsigned)(c-'0'),
					       numTrailZeros, 
					       &significandWide,
					       &significandBig, 
					       significandOverflow);
		}
		if ( numSigDigs != 0 ) {
		    numSigDigs += ( numTrailZeros + 1 );
		} else {
		    numSigDigs = 1;
		}
		numTrailZeros = 0;
		state = FRACTION;
		break;
	    }
	    goto endgame;

	case EXPONENT_START:
	    /* 
	     * Scanned the E at the start of an exponent. Make sure
	     * a legal character follows before using the C library
	     * strtol routine, which allows whitespace.
	     */
	    if (c == '+') {
		state = EXPONENT_SIGNUM;
		break;
	    } else if (c == '-') {
		exponentSignum = 1;
		state = EXPONENT_SIGNUM;
		break;
	    }
	    /* FALLTHROUGH */

	case EXPONENT_SIGNUM:
	    /*
	     * Found the E at the start of the exponent, followed by
	     * a sign character.
	     */
	    if (isdigit(UCHAR(c))) {
		exponent = c - '0';
		state = EXPONENT;
		break;
	    }
	    goto endgame;

	case EXPONENT:
	    /* 
	     * Found an exponent with at least one digit. 
	     * Accumulate it, making sure to hard-pin it to LONG_MAX
	     * on overflow.
	     */
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if (isdigit(UCHAR(c))) {
		if (exponent < (LONG_MAX - 9) / 10) {
		    exponent = 10 * exponent + (c - '0');
		} else {
		    exponent = LONG_MAX;
		}
		state = EXPONENT;
		break;
	    }
	    goto endgame;

	    /*
	     * Parse out INFINITY by simply spelling it out.
	     * INF is accepted as an abbreviation; other prefices are
	     * not.
	     */
	case sI:
	    if ( c == 'n' || c == 'N' ) {
		state = sIN;
		break;
	    } 
	    goto endgame;
	case sIN:
	    if ( c == 'f' || c == 'F' ) {
		state = sINF;
		break;
	    } 
	    goto endgame;
	case sINF:
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if ( c == 'i' || c == 'I' ) {
		state = sINFI;
		break;
	    }
	    goto endgame;
	case sINFI:
	    if ( c == 'n' || c == 'N' ) {
		state = sINFIN;
		break;
	    }
	    goto endgame;
	case sINFIN:
	    if ( c == 'i' || c == 'I' ) {
		state = sINFINI;
		break;
	    }
	    goto endgame;
	case sINFINI:
	    if ( c == 't' || c == 'T' ) {
		state = sINFINIT;
		break;
	    }
	    goto endgame;
	case sINFINIT:
	    if ( c == 'y' || c == 'Y' ) {
		state = sINFINITY;
		break;
	    }
	    goto endgame;

	    /*
	     * Parse NaN's.
	     */
#ifdef IEEE_FLOATING_POINT
	case sN:
	    if ( c == 'a' || c == 'A' ) {
		state = sNA;
		break;
	    }
	    goto endgame;
	case sNA:
	    if ( c == 'n' || c == 'N' ) {
		state = sNAN;
		break;
	    }
	case sNAN:
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    if ( c == '(' ) {
		state = sNANPAREN;
		break;
	    }
	    goto endgame;

	    /*
	     * Parse NaN(hexdigits)
	     */
	case sNANHEX:
	    if ( c == ')' ) {
		state = sNANFINISH;
		break;
	    }
	    /* FALLTHROUGH */
	case sNANPAREN:
	    if ( isspace(UCHAR(c)) ) {
		break;
	    }
	    if ( numSigDigs < 13 ) {
		if ( c >= '0' && c <= '9' ) {
		    d = c - '0';
		} else if ( c >= 'a' && c <= 'f' ) {
		    d = 10 + c - 'a';
		} else if ( c >= 'A' && c <= 'F' ) {
		    d = 10 + c - 'A';
		}
		significandWide = (significandWide << 4) + c;
		state = sNANHEX;
		break;
	    }
	    goto endgame;
	case sNANFINISH:
#endif
	case sINFINITY:
	    acceptState = state;
	    acceptPoint = p;
	    acceptLen = len;
	    goto endgame;
	}
	++p; 
	--len;
    }

  endgame:

    /* Back up to the last accepting state in the lexer */

    if (acceptState == INITIAL) {
	status = TCL_ERROR;
    }
    p = acceptPoint;
    len = acceptLen;

    /* Skip past trailing whitespace */

    if (endPtrPtr != NULL) {
	*endPtrPtr = p;
    }

    while (len > 0 && isspace(UCHAR(*p))) {
	++p;
	--len;
    }

    /* Determine whether a partial string is acceptable. */

    if (endPtrPtr == NULL && len != 0 && *p != '\0') {
	status = TCL_ERROR;
    }

    /* Generate and store the appropriate internal rep */

    if (status == TCL_OK && objPtr != NULL) {
	if ( acceptState != INITIAL ) {
	    TclFreeIntRep( objPtr );
	}
	switch (acceptState) {

	case INITIAL:
	    status = TCL_ERROR;
	    break;

	case SIGNUM:
	case BAD_OCTAL:
	case ZERO_X:
	case LEADING_RADIX_POINT:
	case EXPONENT_START:
	case EXPONENT_SIGNUM:
	case sI:
	case sIN:
	case sINFI:
	case sINFIN:
	case sINFINI:
	case sINFINIT:
	case sN:
	case sNA:
	case sNANPAREN:
	case sNANHEX:
	    panic("in TclParseNumber: bad acceptState, can't happen.");

	case HEXADECIMAL:
	    /* Returning a hex integer.  Final scaling step */
	    if (!significandOverflow) {
		shift = 4 * numTrailZeros;
		if (significandWide !=0 
			&& (shift >= CHAR_BIT*sizeof(Tcl_WideUInt) 
			|| significandWide 
			> (((~(Tcl_WideUInt)0) >> 1) + signum) >> shift )) {
		    significandOverflow = 1;
		    TclBNInitBignumFromWideUInt(&significandBig,
						significandWide);
		}
	    }
	    if (shift) {
		if ( !significandOverflow ) {
		    significandWide <<= shift;
		} else {
		    mp_mul_2d( &significandBig, shift, &significandBig );
		}
	    }
	    goto returnInteger;

	case OCTAL:
	    /* Returning an octal integer.  Final scaling step */
	    if (!octalSignificandOverflow) {
		shift = 3 * numTrailZeros;
		if (octalSignificandWide != 0
			&& (shift >= CHAR_BIT*sizeof(Tcl_WideUInt) 
			|| octalSignificandWide 
			> (((~(Tcl_WideUInt)0) >> 1) + signum) >> shift )) {
		    octalSignificandOverflow = 1;
		    TclBNInitBignumFromWideUInt(&octalSignificandBig,
						octalSignificandWide);
		}
	    }
	    if ( shift ) {
		if ( !octalSignificandOverflow ) {
		    octalSignificandWide <<= shift;
		} else {
		    mp_mul_2d( &octalSignificandBig, shift,
			       &octalSignificandBig );
		}
	    }
	    if (!octalSignificandOverflow) {
		if (octalSignificandWide > 
			(Tcl_WideUInt)(((~(unsigned long)0) >> 1) + signum)) {
#ifndef NO_WIDE_TYPE
		    if (octalSignificandWide 
			    <= (((~(Tcl_WideUInt)0) >> 1) + signum)) {
			objPtr->typePtr = &tclWideIntType;
			if (signum) {
			    objPtr->internalRep.wideValue =
				    - (Tcl_WideInt) octalSignificandWide;
			} else {
			    objPtr->internalRep.wideValue =
				    (Tcl_WideInt) octalSignificandWide;
			}
			break;
		    }
#endif
		    TclBNInitBignumFromWideUInt(&octalSignificandBig,
						octalSignificandWide);
		    octalSignificandOverflow = 1;
		} else {
		    objPtr->typePtr = &tclIntType;
		    if (signum) {
			objPtr->internalRep.longValue =
				- (long) octalSignificandWide;
		    } else {
			objPtr->internalRep.longValue =
				(long) octalSignificandWide;
		    }
		}
	    }
	    if (octalSignificandOverflow) {
		if (signum) {
		    octalSignificandBig.sign = MP_NEG;
		} else {
		    octalSignificandBig.sign = MP_ZPOS;
		}
		TclSetBignumIntRep(objPtr, &octalSignificandBig);
		octalSignificandOverflow = 0;
	    }		
	    break;

	case ZERO:
	case DECIMAL:
	    significandOverflow =
		AccumulateDecimalDigit( 0, numTrailZeros-1,
					&significandWide, &significandBig,
					significandOverflow );
	    if (!significandOverflow
		&& (significandWide
		    > (((~(Tcl_WideUInt)0) >> 1) + signum))) {
		significandOverflow = 1;
		TclBNInitBignumFromWideUInt(&significandBig,
					    significandWide);
	    }
	  returnInteger:
	    if (!significandOverflow) {
		if (significandWide > 
			(Tcl_WideUInt)(((~(unsigned long)0) >> 1) + signum)) {
#ifndef NO_WIDE_TYPE
		    if (significandWide 
			    <= (((~(Tcl_WideUInt)0) >> 1) + signum)) {
			objPtr->typePtr = &tclWideIntType;
			if (signum) {
			    objPtr->internalRep.wideValue =
				    - (Tcl_WideInt) significandWide;
			} else {
			    objPtr->internalRep.wideValue =
				    (Tcl_WideInt) significandWide;
			}
			break;
		    }
#endif
		    TclBNInitBignumFromWideUInt(&significandBig,
						significandWide);
		    significandOverflow = 1;
		} else {
		    objPtr->typePtr = &tclIntType;
		    if (signum) {
			objPtr->internalRep.longValue =
				- (long) significandWide;
		    } else {
			objPtr->internalRep.longValue =
				(long) significandWide;
		    }
		}
	    }
	    if (significandOverflow) {
		if (signum) {
		    significandBig.sign = MP_NEG;
		} else {
		    significandBig.sign = MP_ZPOS;
		}
		TclSetBignumIntRep(objPtr, &significandBig);
		significandOverflow = 0;
	    }
	    break;

	case FRACTION:
	case EXPONENT:

	    /*
	     * Here, we're parsing a floating-point number.
	     * 'significandWide' or 'significandBig' contains the
	     * exact significand, according to whether
	     * 'significandOverflow' is set.  The desired floating
	     * point value is significand * 10**k, where
	     * k = numTrailZeros+exponent-numDigitsAfterDp.
	     */

	    objPtr->typePtr = &tclDoubleType;
	    if ( exponentSignum ) {
		exponent = - exponent;
	    }
	    if ( !significandOverflow ) {
		objPtr->internalRep.doubleValue = 
		    MakeLowPrecisionDouble( signum, 
					    significandWide,
					    numSigDigs,
					    ( numTrailZeros
					      + exponent
					      - numDigitsAfterDp ) );
	    } else {
		objPtr->internalRep.doubleValue =
		    MakeHighPrecisionDouble( signum,
					     &significandBig,
					     numSigDigs,
					     ( numTrailZeros
					       + exponent
					       - numDigitsAfterDp ) );
	    }
	    break;

	case sINF:
	case sINFINITY:
	    if ( signum ) {
		objPtr->internalRep.doubleValue = HUGE_VAL;
	    } else {
		objPtr->internalRep.doubleValue = -HUGE_VAL;
	    }
	    objPtr->typePtr = &tclDoubleType;
	    break;

	case sNAN:
	case sNANFINISH:
	    objPtr->internalRep.doubleValue
		= MakeNaN( signum, significandWide );
	    objPtr->typePtr = &tclDoubleType;
	    break;
	    
	}
    }

    /* Format an error message when an invalid number is encountered. */

    if ( status != TCL_OK ) {
	if ( interp != NULL ) {
	    Tcl_Obj *msg = Tcl_NewStringObj( "expected ", -1 );
	    Tcl_AppendToObj( msg, type, -1 );
	    Tcl_AppendToObj( msg, " but got \"", -1 );
	    TclAppendLimitedToObj( msg, string, length, 50, "" );
	    Tcl_AppendToObj( msg, "\"", -1 );
	    if ( state == BAD_OCTAL ) {
		Tcl_AppendToObj( msg, " (looks like invalid octal number)",
				 -1 );
	    }
	    Tcl_SetObjResult( interp, msg );
	}
    }

    /* Free memory */

    if (octalSignificandOverflow) {
	mp_clear(&octalSignificandBig);
    }
    if (significandOverflow) {
	mp_clear(&significandBig);
    }
    return status;
}

/*
 *----------------------------------------------------------------------
 *
 * AccumulateDecimalDigit --
 *
 *	Consume a decimal digit in a number being scanned.
 *
 * Results:
 *	Returns 1 if the number has overflowed to a bignum, 0 if it
 *	still fits in a wide integer.
 *
 * Side effects:
 *	Updates either the wide or bignum representation.
 *
 *----------------------------------------------------------------------
 */

static int
AccumulateDecimalDigit( unsigned digit,
				/* Digit being scanned */
			int numZeros,
				/* Count of zero digits preceding the
				 * digit being scanned */
			Tcl_WideUInt* wideRepPtr,
				/* Representation of the partial number
				 * as a wide integer */
			mp_int* bignumRepPtr,
				/* Representation of the partial number
				 * as a bignum */
			int bignumFlag )
				/* Flag == 1 if the number overflowed
				 * previous to this digit. */
{
    int i, n;

    /* Check if the number still fits in a wide */

    if (!bignumFlag) {
	if (*wideRepPtr != 0) {
	    if ((numZeros >= maxpow10_wide)
		|| (*wideRepPtr > (((~(Tcl_WideUInt)0) - digit)
				   / pow10_wide[numZeros+1]))) {
		/* Oops, it's overflowed, have to allocate a bignum */
		TclBNInitBignumFromWideUInt (bignumRepPtr, *wideRepPtr);
		bignumFlag = 1;
	    }
	}
    }

    /* Multiply the number by 10**numZeros+1 and add in the new digit. */

    if (!bignumFlag) {

	/* Wide multiplication */

	*wideRepPtr = *wideRepPtr * pow10_wide[numZeros+1] + digit;
    } else if (numZeros < log10_DIGIT_MAX ) {

	/* Up to about 8 zeros - single digit multiplication */

	mp_mul_d (bignumRepPtr, (mp_digit) pow10_wide[numZeros+1],
		  bignumRepPtr);
	mp_add_d (bignumRepPtr, (mp_digit) digit, bignumRepPtr);

    } else {

	/* 
	 * More than single digit multiplication. Multiply by the appropriate
	 * small powers of 5, and then shift.  Large strings of zeroes are
	 * eaten 256 at a time; this is less efficient than it could be,
	 * but seems implausible.  We presume that DIGIT_BIT is at least 27.
	 * The first multiplication, by up to 10**7, is done with a
	 * one-DIGIT multiply (this presumes that DIGIT_BIT >= 24).
	 */

	n = numZeros + 1;
	mp_mul_d (bignumRepPtr, (mp_digit) pow10_wide[n&0x7], bignumRepPtr);
	for (i = 3; i <= 7; ++i) {
	    if (n & (1 << i)) {
		mp_mul (bignumRepPtr, pow5+i, bignumRepPtr);
	    }
	}
	while (n >= 256) {
	    mp_mul (bignumRepPtr, pow5+8, bignumRepPtr);
	    n -= 256;
	}
	mp_mul_2d (bignumRepPtr, (int)(numZeros+1)&~0x7, bignumRepPtr);
    }

    return bignumFlag;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeLowPrecisionDouble --
 *
 *	Makes the double precision number, signum*significand*10**exponent.
 *
 * Results:
 *	Returns the constructed number.
 *
 * Common cases, where there are few enough digits that the number can
 * be represented with at most roundoff, are handled specially here.
 * If the number requires more than one rounded operation to compute,
 * the code promotes the significand to a bignum and calls
 * MakeHighPrecisionDouble to do it instead.
 *
 *----------------------------------------------------------------------
 */

static double
MakeLowPrecisionDouble( int signum,
				/* 1 if the number is negative, 0 otherwise */
			Tcl_WideUInt significand,
				/* Significand of the number */
			int numSigDigs,
				/* Number of digits in the significand */
			int exponent )
				/* Power of ten */
{
    double retval;		/* Value of the number */
    mp_int significandBig;	/* Significand expressed as a bignum */

    /*
     * With gcc on x86, the floating point rounding mode is double-extended.
     * This causes the result of double-precision calculations to be rounded
     * twice: once to the precision of double-extended and then again to the
     * precision of double.  Double-rounding introduces gratuitous errors of
     * 1 ulp, so we need to change rounding mode to 53-bits.
     */

#if defined(__GNUC__) && defined(__i386)
    fpu_control_t roundTo53Bits = 0x027f;
    fpu_control_t oldRoundingMode;
    _FPU_GETCW( oldRoundingMode );
    _FPU_SETCW( roundTo53Bits );
#endif

    /* Test for the easy cases */

    if ( numSigDigs <= DBL_DIG ) {
	if ( exponent >= 0 ) {
	    if ( exponent <= mmaxpow ) {

		/* 
		 * The significand is an exact integer, and so is 
		 * 10**exponent. The product will be correct to within
		 * 1/2 ulp without special handling.
		 */

		retval = (double)(Tcl_WideInt)significand * pow10[ exponent ];
		goto returnValue;

	    } else {
		int diff = DBL_DIG - numSigDigs;
		if ( exponent-diff <= mmaxpow ) {

		    /* 
		     * 10**exponent is not an exact integer, but
		     * 10**(exponent-diff) is exact, and so is
		     * significand*10**diff, so we can still compute
		     * the value with only one roundoff.
		     */
		    volatile double factor
			= (double)(Tcl_WideInt)significand * pow10[diff];
		    retval = factor * pow10[exponent-diff];
		    goto returnValue;
		}
	    }
	} else {
	    if ( exponent >= -mmaxpow ) {

		/* 
		 * 10**-exponent is an exact integer, and so is the
		 * significand. Compute the result by one division,
		 * again with only one rounding.
		 */

		retval = (double)(Tcl_WideInt)significand / pow10[-exponent];
		goto returnValue;
	    }
	}
    }

    /*
     * All the easy cases have failed.  Promote ths significand
     * to bignum and call MakeHighPrecisionDouble to do it the hard way.
     */

    TclBNInitBignumFromWideUInt (&significandBig, significand);
    retval = MakeHighPrecisionDouble( 0, &significandBig, numSigDigs,
				      exponent );
    
    /* Come here to return the computed value */

  returnValue:

    if ( signum ) {
	retval = -retval;
    }

    /* On gcc on x86, restore the floating point mode word. */

#if defined(__GNUC__) && defined(__i386)
    _FPU_SETCW( oldRoundingMode );
#endif

    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeHighPrecisionDouble --
 *
 *	Makes the double precision number, signum*significand*10**exponent.
 *
 * Results:
 *	Returns the constructed number.
 *
 * MakeHighPrecisionDouble is used when arbitrary-precision arithmetic
 * is needed to ensure correct rounding.  It begins by calculating a
 * low-precision approximation to the desired number, and then refines
 * the answer in high precision.
 *
 *----------------------------------------------------------------------
 */

static double
MakeHighPrecisionDouble( int signum,
				/* 1=negative, 0=nonnegative */
			 mp_int* significand,
				/* Exact significand of the number */
			 int numSigDigs,
				/* Number of significant digits */
			 int exponent )
				/* Power of 10 by which to multiply */
{

    double retval;
    int machexp;		/* Machine exponent of a power of 10 */

    /*
     * With gcc on x86, the floating point rounding mode is double-extended.
     * This causes the result of double-precision calculations to be rounded
     * twice: once to the precision of double-extended and then again to the
     * precision of double.  Double-rounding introduces gratuitous errors of
     * 1 ulp, so we need to change rounding mode to 53-bits.
     */

#if defined(__GNUC__) && defined(__i386)
    fpu_control_t roundTo53Bits = 0x027f;
    fpu_control_t oldRoundingMode;
    _FPU_GETCW( oldRoundingMode );
    _FPU_SETCW( roundTo53Bits );
#endif

    /* Quick checks for over/underflow */

    if ( numSigDigs + exponent - 1 > maxDigits ) {
	retval = HUGE_VAL;
	goto returnValue;
    }
    if ( numSigDigs + exponent - 1 < minDigits ) {
	retval = 0;
	goto returnValue;
    }

    /* 
     * Develop a first approximation to the significand. It is tempting
     * simply to force bignum to double, but that will overflow on input
     * numbers like 1.[string repeat 0 1000]1; while this is a not terribly
     * likely scenario, we still have to deal with it. Use fraction and
     * exponent instead.  Once we have the significand, multiply by
     * 10**exponent. Test for overflow. Convert back to a double, and
     * test for underflow.
     */

    retval = BignumToBiasedFrExp( significand, &machexp );
    retval = Pow10TimesFrExp( exponent, retval, &machexp );
    if ( machexp > DBL_MAX_EXP * log2FLT_RADIX ) {
	retval = HUGE_VAL;
	goto returnValue;
    }
    retval = SafeLdExp( retval, machexp );
    if ( retval < tiny ) {
	retval = tiny;
    }

    /* 
     * Refine the result twice. (The second refinement should be 
     * necessary only if the best approximation is a power of 2
     * minus 1/2 ulp).
     */

    retval = RefineApproximation( retval, significand, exponent );
    retval = RefineApproximation( retval, significand, exponent );

    /* Come here to return the computed value */

  returnValue:
    if ( signum ) {
	retval = -retval;
    }

    /* On gcc on x86, restore the floating point mode word. */

#if defined(__GNUC__) && defined(__i386)
    _FPU_SETCW( oldRoundingMode );
#endif
    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeNaN --
 *
 *	Makes a "Not a Number" given a set of bits to put in the
 *	tag bits
 *
 * Note that a signalling NaN is never returned.
 *
 *----------------------------------------------------------------------
 */

#ifdef IEEE_FLOATING_POINT
static double
MakeNaN( int signum,		/* Sign bit (1=negative, 0=nonnegative */
	 Tcl_WideUInt tags ) 	/* Tag bits to put in the NaN */
{
    union {
	Tcl_WideUInt iv;
	double dv;
    } theNaN;

    theNaN.iv = tags;
    theNaN.iv &= ( ((Tcl_WideUInt) 1) << 51 ) - 1;
    if ( signum ) {
	theNaN.iv |= ((Tcl_WideUInt) (0x8000 | NAN_START)) << 48;
    } else {
	theNaN.iv |= ((Tcl_WideUInt) NAN_START) << 48;
    }

    return theNaN.dv;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * RefineApproximation --
 *
 *	Given a poor approximation to a floating point number, returns
 *	a better one (The better approximation is correct to within
 *	1 ulp, and is entirely correct if the poor approximation is
 *	correct to 1 ulp.)
 *
 * Results:
 *	Returns the improved result.
 *
 *----------------------------------------------------------------------
 */

static double
RefineApproximation( double approxResult, 
				/* Approximate result of conversion */
		     mp_int* exactSignificand,
				/* Integer significand */
		     int exponent )
				/* Power of 10 to multiply by significand */
{

    int M2, M5;			/*  Powers of 2 and of 5 needed to put
				 * the decimal and binary numbers over
				 * a common denominator. */
    double significand;		/* Sigificand of the binary number */
    int binExponent;		/* Exponent of the binary number */

    int msb;			/* Most significant bit position of an
				 * intermediate result */
    int nDigits;		/* Number of mp_digit's in an intermediate
				 * result */
    mp_int twoMv;		/* Approx binary value expressed as an
				 * exact integer scaled by the multiplier 2M */
    mp_int twoMd;		/* Exact decimal value expressed as an
				 * exact integer scaled by the multiplier 2M */
    int scale;			/* Scale factor for M */
    int multiplier;		/* Power of two to scale M */
    double num, den;		/* Numerator and denominator of the
				 * correction term */
    double quot;		/* Correction term */
    double minincr;		/* Lower bound on the absolute value
				 * of the correction term. */
    int i;

    /*
     * The first approximation is always low.  If we find that
     * it's HUGE_VAL, we're done.
     */

    if ( approxResult == HUGE_VAL ) {
	return approxResult;
    }

    /*
     * Find a common denominator for the decimal and binary fractions.
     * The common denominator will be 2**M2 + 5**M5.
     */

    significand = frexp( approxResult, &binExponent );
    i = mantBits - binExponent;
    if ( i < 0 ) {
	M2 = 0;
    } else {
	M2 = i;
    }
    if ( exponent > 0 ) {
	M5 = 0;
    } else {
	M5 = -exponent;
	if ( (M5-1) > M2 ) {
	    M2 = M5-1;
	}
    }

    /* 
     * The floating point number is significand*2**binExponent.
     * Compute the large integer significand*2**(binExponent+M2+1)
     * The 2**-1 bit of the significand (the most significant) 
     * corresponds to the 2**(binExponent+M2 + 1) bit of 2*M2*v.
     * Allocate enough digits to hold that quantity, then
     * convert the significand to a large integer, scaled
     * appropriately. Then multiply by the appropriate power of 5.
     */

    msb = binExponent + M2;  /* 1008 */
    nDigits = msb / DIGIT_BIT + 1;
    mp_init_size( &twoMv, nDigits );
    i = ( msb % DIGIT_BIT + 1 ); 
    twoMv.used = nDigits;
    significand *= SafeLdExp( 1.0, i );
    while ( -- nDigits >= 0 ) {
	twoMv.dp[nDigits] = (mp_digit) significand;
	significand -= (mp_digit) significand;
	significand = SafeLdExp( significand, DIGIT_BIT );
    }
    for ( i = 0; i <= 8; ++i ) {
	if ( M5 & ( 1 << i ) ) {
	    mp_mul( &twoMv, pow5+i, &twoMv );
	}
    }
    
    /* 
     * Collect the decimal significand as a high precision integer.
     * The least significant bit corresponds to bit M2+exponent+1
     * so it will need to be shifted left by that many bits after
     * being multiplied by 5**(M5+exponent).
     */

    mp_init_copy( &twoMd, exactSignificand );
    for ( i = 0; i <= 8; ++i ) {
	if ( (M5+exponent) & ( 1 << i ) ) {
	    mp_mul( &twoMd, pow5+i, &twoMd );
	}
    }
    mp_mul_2d( &twoMd, M2+exponent+1, &twoMd );
    mp_sub( &twoMd, &twoMv, &twoMd );

    /*
     * The result, 2Mv-2Md, needs to be divided by 2M to yield a correction
     * term. Because 2M may well overflow a double, we need to scale the
     * denominator by a factor of 2**binExponent-mantBits
     */

    scale = binExponent - mantBits - 1;

    mp_set( &twoMv, 1 );
    for ( i = 0; i <= 8; ++i ) {
	if ( M5 & ( 1 << i ) ) {
	    mp_mul( &twoMv, pow5+i, &twoMv );
	}
    }
    multiplier = M2 + scale + 1;
    if ( multiplier > 0 ) {
	mp_mul_2d( &twoMv, multiplier, &twoMv );
    } else if ( multiplier < 0 ) {
	mp_div_2d( &twoMv, -multiplier, &twoMv, NULL );
    }

    /*
     * If the result is less than unity, the error is less than 1/2 unit
     * in the last place, so there's no correction to make.
     */

    if ( mp_cmp_mag( &twoMd, &twoMv ) == MP_LT ) {
	return approxResult;
    }

    /* 
     * Convert the numerator and denominator of the corrector term
     * accurately to floating point numbers.
     */

    num = TclBignumToDouble( &twoMd );
    den = TclBignumToDouble( &twoMv );

    quot = SafeLdExp( num/den, scale );
    minincr = SafeLdExp( 1.0, binExponent - mantBits );

    if ( quot < 0. && quot > -minincr ) {
	quot = -minincr;
    } else if ( quot > 0. && quot < minincr ) {
	quot = minincr;
    }

    mp_clear( &twoMd );
    mp_clear( &twoMv );

    
    return approxResult + quot;
}

/*
 *----------------------------------------------------------------------
 *
 * TclStrToD --
 *
 *	Scans a double from a string.
 *
 * Results:
 *	Returns the scanned number. In the case of underflow, returns an
 *	appropriately signed zero; in the case of overflow, returns an
 *	appropriately signed HUGE_VAL.
 *
 * Side effects:
 *	Stores a pointer to the end of the scanned number in '*endPtr', if
 *	endPtr is not NULL. If '*endPtr' is equal to 's' on return from this
 *	function, it indicates that the input string could not be recognized
 *	as a number.  In the case of underflow or overflow, 'errno' is set to
 *	ERANGE.
 *
 *------------------------------------------------------------------------
 */

double
TclStrToD(CONST char *s,	/* String to scan. */
	  CONST char **endPtr)	/* Pointer to the end of the scanned number. */
{
    const char *p = s;
    const char *startOfSignificand = NULL;
				/* Start of the significand in the string. */
    int signum = 0;		/* Sign of the significand. */
    double exactSignificand = 0.0;
				/* Significand, represented exactly as a
				 * floating-point number. */
    int seenDigit = 0;		/* Flag == 1 if a digit has been seen. */
    int nSigDigs = 0;		/* Number of significant digits presented. */
    int nDigitsAfterDp = 0;	/* Number of digits after the decimal point. */
    int nTrailZero = 0;		/* Number of trailing zeros in the
				 * significand. */
    long exponent = 0;		/* Exponent. */
    int seenDp = 0;		/* Flag == 1 if decimal point has been seen. */
    char c;			/* One character extracted from the input. */
    volatile double v;		/* Scanned value; must be 'volatile double' on
				 * gc-ix86 to force correct rounding to IEEE
				 * double and not Intel double-extended. */
    int machexp;		/* Exponent of the machine rep of the scanned
				 * value. */
    int expt2;			/* Exponent for computing first approximation
				 * to the true value. */
    int i, j;

    /*
     * With gcc on x86, the floating point rounding mode is double-extended.
     * This causes the result of double-precision calculations to be rounded
     * twice: once to the precision of double-extended and then again to the
     * precision of double.  Double-rounding introduces gratuitous errors of
     * one ulp, so we need to change rounding mode to 53-bits.
     */

#ifdef ADJUST_FPU_CONTROL_WORD
    fpu_control_t roundTo53Bits = FPU_IEEE_ROUNDING;
    fpu_control_t oldRoundingMode;
    _FPU_GETCW(oldRoundingMode);
    _FPU_SETCW(roundTo53Bits);
#   define RestoreRoundingMode()	_FPU_SETCW(oldRoundingMode)
#else
#   define RestoreRoundingMode()	(void) 0 /* Do nothing */
#endif

    /*
     * Discard leading whitespace from input.
     */

    while (isspace(UCHAR(*p))) {
	++p;
    }

    /*
     * Determine the sign of the significand.
     */

    switch (*p) {
	case '-':
	    signum = 1;
	    /* FALLTHROUGH */
	case '+':
	    ++p;
    }

    /*
     * Discard leading zeroes from input.
     */

    while (*p == '0') {
	seenDigit = 1;
	++p;
    }

    /*
     * Scan digits from the significand. Simultaneously, keep track of the
     * number of digits after the decimal point. Maintain a pointer to the
     * start of the significand. Keep "exactSignificand" equal to the
     * conversion of the DBL_DIG most significant digits.
     */

    for (;;) {
	c = *p;
	if (c == '.' && !seenDp) {
	    seenDp = 1;
	    ++p;
	} else if (isdigit(UCHAR(c))) {
	    if (c == '0') {
		if (startOfSignificand != NULL) {
		    ++nTrailZero;
		}
	    } else {
		if (startOfSignificand == NULL) {
		    startOfSignificand = p;
		} else if (nTrailZero) {
		    if (nTrailZero + nSigDigs < DBL_DIG) {
			exactSignificand *= pow10[nTrailZero];
		    } else if (nSigDigs < DBL_DIG) {
			exactSignificand *= pow10[DBL_DIG - nSigDigs];
		    }
		    nSigDigs += nTrailZero;
		}
		if (nSigDigs < DBL_DIG) {
		    exactSignificand = 10. * exactSignificand + (c - '0');
		}
		++nSigDigs;
		nTrailZero = 0;
	    }
	    if (seenDp) {
		++nDigitsAfterDp;
	    }
	    seenDigit = 1;
	    ++p;
	} else {
	    break;
	}
    }

    /*
     * At this point, we've scanned the significand, and p points to the
     * character beyond it. "startOfSignificand" is the first non-zero
     * character in the significand. "nSigDigs" is the number of significant
     * digits of the significand, not including any trailing zeroes.
     * "exactSignificand" is a floating point number that represents, without
     * loss of precision, the first min(DBL_DIG,n) digits of the significand.
     * "nDigitsAfterDp" is the number of digits after the decimal point, again
     * excluding trailing zeroes.
     *
     * Now scan 'E' format
     */

    exponent = 0;
    if (seenDigit && (*p == 'e' || *p == 'E')) {
	const char* stringSave = p;
	++p;
	c = *p;
	if (isdigit(UCHAR(c)) || c == '+' || c == '-') {
	    errno = 0;
	    exponent = strtol(p, (char**)&p, 10);
	    if (errno == ERANGE) {
		if (exponent > 0) {
		    v = HUGE_VAL;
		} else {
		    v = 0.0;
		}
		*endPtr = p;
		goto returnValue;
	    }
	}
	if (p == stringSave+1) {
	    p = stringSave;
	    exponent = 0;
	}
    }
    exponent += nTrailZero - nDigitsAfterDp;

    /*
     * If we come here with no significant digits, we might still be looking
     * at Inf or NaN.  Go parse them.
     */

    if (!seenDigit) {
	/*
	 * Test for Inf or Infinity (in any case).
	 */

	if (c == 'I' || c == 'i') {
	    if ((p[1] == 'N' || p[1] == 'n')
		    && (p[2] == 'F' || p[2] == 'f')) {
		p += 3;
		if ((p[0] == 'I' || p[0] == 'i')
			&& (p[1] == 'N' || p[1] == 'n')
			&& (p[2] == 'I' || p[2] == 'i')
			&& (p[3] == 'T' || p[3] == 't')
			&& (p[4] == 'Y' || p[1] == 'y')) {
		    p += 5;
		}
		errno = ERANGE;
		v = HUGE_VAL;
		if (endPtr != NULL) {
		    *endPtr = p;
		}
		goto returnValue;
	    }

#ifdef IEEE_FLOATING_POINT
	    /*
	     * Only IEEE floating point supports NaN
	     */
	} else if ((c == 'N' || c == 'n')
		&& (sizeof(Tcl_WideUInt) == sizeof(double))) {
	    if ((p[1] == 'A' || p[1] == 'a')
		    && (p[2] == 'N' || p[2] == 'n')) {
		p += 3;

		if (endPtr != NULL) {
		    *endPtr = p;
		}

		/*
		 * Restore FPU mode word.
		 */

		RestoreRoundingMode();
		return ParseNaN(signum, endPtr);
	    }
#endif
	}

	goto error;
    }

    /*
     * We've successfully scanned; update the end-of-element pointer.
     */

    if (endPtr != NULL) {
	*endPtr = p;
    }

    /*
     * Test for zero.
     */

    if (nSigDigs == 0) {
	v = 0.0;
	goto returnValue;
    }

    /*
     * The easy cases are where we have an exact significand and the exponent
     * is small enough that we can compute the value with only one roundoff.
     * In addition to the cases where we can multiply or divide an
     * exact-integer significand by an exact-integer power of 10, there is
     * also David Gay's case where we can scale the significand by a power of
     * 10 (still keeping it exact) and then multiply by an exact power of 10.
     * The last case enables combinations like 83e25 that would otherwise
     * require high precision arithmetic.
     */

    if (nSigDigs <= DBL_DIG) {
	if (exponent >= 0) {
	    if (exponent <= mmaxpow) {
		v = exactSignificand * pow10[exponent];
		goto returnValue;
	    } else {
		int diff = DBL_DIG - nSigDigs;
		if ( exponent - diff <= (int) mmaxpow ) {
		    volatile double factor = exactSignificand * pow10[ diff ];
		    v = factor * pow10[ exponent - diff ];
		    goto returnValue;
		}
	    }
	} else if (exponent >= -mmaxpow) {
	    v = exactSignificand / pow10[-exponent];
	    goto returnValue;
	}
    }

    /*
     * We don't have one of the easy cases, so we can't compute the scanned
     * number exactly, and have to do it in multiple precision.  Begin by
     * testing for obvious overflows and underflows.
     */

    if (nSigDigs + exponent - 1 > maxDigits) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }
    if (nSigDigs + exponent - 1 < minDigits) {
	errno = ERANGE;
	v = 0.;
	goto returnValue;
    }

    /*
     * Nothing exceeds the boundaries of the tables, at least.  Compute an
     * approximate value for the number, with no possibility of overflow
     * because we manage the exponent separately.
     */

    if (nSigDigs > DBL_DIG) {
	expt2 = exponent + nSigDigs - DBL_DIG;
    } else {
	expt2 = exponent;
    }
    v = frexp(exactSignificand, &machexp);
    if (expt2 > 0) {
	v = frexp(v * pow10[expt2 & 0xf], &j);
	machexp += j;
	for (i=4 ; i<9 ; ++i) {
	    if (expt2 & (1 << i)) {
		v = frexp(v * pow_10_2_n[i], &j);
		machexp += j;
	    }
	}
    } else {
	v = frexp(v / pow10[(-expt2) & 0xf], &j);
	machexp += j;
	for (i=4 ; i<9 ; ++i) {
	    if ((-expt2) & (1 << i)) {
		v = frexp(v / pow_10_2_n[i], &j);
		machexp += j;
	    }
	}
    }

    /*
     * A first approximation is that the result will be v * 2 ** machexp. v is
     * greater than or equal to 0.5 and less than 1. If machexp >
     * DBL_MAX_EXP*log2(FLT_RADIX), there is an overflow. Constrain the result
     * to the smallest representible number to avoid premature underflow.
     */

    if (machexp > DBL_MAX_EXP * log2FLT_RADIX) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }

    v = SafeLdExp(v, machexp);
    if (v < tiny) {
	v = tiny;
    }

    /*
     * We have a first approximation in v. Now we need to refine it.
     */

    v = RefineResult(v, startOfSignificand, nSigDigs, exponent);

    /*
     * In a very few cases, a second iteration is needed. e.g., 457e-102
     */

    v = RefineResult(v, startOfSignificand, nSigDigs, exponent);

    /*
     * Handle underflow.
     */

  returnValue:
    if (nSigDigs != 0 && v == 0.0) {
	errno = ERANGE;
    }

    /*
     * Return a number with correct sign.
     */

    if (signum) {
	v = -v;
    }

    /*
     * Restore FPU mode word and return.
     */

    RestoreRoundingMode();
    return v;

    /*
     * Come here on an invalid input.
     */

  error:
    if (endPtr != NULL) {
	*endPtr = s;
    }

    /*
     * Restore FPU mode word and return.
     */

    RestoreRoundingMode();
    return 0.0;
}

/*
 *----------------------------------------------------------------------
 *
 * RefineResult --
 *
 *	Given a poor approximation to a floating point number, returns a
 *	better one. (The better approximation is correct to within 1 ulp, and
 *	is entirely correct if the poor approximation is correct to 1 ulp.)
 *
 * Results:
 *	Returns the improved result.
 *
 *----------------------------------------------------------------------
 */

static double
RefineResult(double approxResult, /* Approximate result of conversion. */
	     CONST char* sigStart,
				/* Pointer to start of significand in input
				 * string. */
	     int nSigDigs,	/* Number of significant digits. */
	     long exponent)	/* Power of ten to multiply by significand. */
{
    int M2, M5;			/* Powers of 2 and of 5 needed to put the
				 * decimal and binary numbers over a common
				 * denominator. */
    double significand;		/* Sigificand of the binary number. */
    int binExponent;		/* Exponent of the binary number. */
    int msb;			/* Most significant bit position of an
				 * intermediate result. */
    int nDigits;		/* Number of mp_digit's in an intermediate
				 * result. */
    mp_int twoMv;		/* Approx binary value expressed as an exact
				 * integer scaled by the multiplier 2M. */
    mp_int twoMd;		/* Exact decimal value expressed as an exact
				 * integer scaled by the multiplier 2M. */
    int scale;			/* Scale factor for M. */
    int multiplier;		/* Power of two to scale M. */
    double num, den;		/* Numerator and denominator of the correction
				 * term. */
    double quot;		/* Correction term. */
    double minincr;		/* Lower bound on the absolute value of the
				 * correction term. */
    int i;
    const char* p;

    /*
     * The first approximation is always low. If we find that it's HUGE_VAL,
     * we're done.
     */

    if (approxResult == HUGE_VAL) {
	return approxResult;
    }

    /*
     * Find a common denominator for the decimal and binary fractions. The
     * common denominator will be 2**M2 + 5**M5.
     */

    significand = frexp(approxResult, &binExponent);
    i = mantBits - binExponent;
    if (i < 0) {
	M2 = 0;
    } else {
	M2 = i;
    }
    if (exponent > 0) {
	M5 = 0;
    } else {
	M5 = -exponent;
	if ((M5-1) > M2) {
	    M2 = M5-1;
	}
    }

    /*
     * The floating point number is significand*2**binExponent. The 2**-1 bit
     * of the significand (the most significant) corresponds to the
     * 2**(binExponent+M2 + 1) bit of 2*M2*v. Allocate enough digits to hold
     * that quantity, then convert the significand to a large integer, scaled
     * appropriately. Then multiply by the appropriate power of 5.
     */

    msb = binExponent + M2;	/* 1008 */
    nDigits = msb / DIGIT_BIT + 1;
    mp_init_size(&twoMv, nDigits);
    i = (msb % DIGIT_BIT + 1);
    twoMv.used = nDigits;
    significand *= SafeLdExp(1.0, i);
    while (--nDigits >= 0) {
	twoMv.dp[nDigits] = (mp_digit) significand;
	significand -= (mp_digit) significand;
	significand = SafeLdExp(significand, DIGIT_BIT);
    }
    for (i=0 ; i<=8 ; ++i) {
	if (M5 & (1 << i)) {
	    mp_mul(&twoMv, pow5+i, &twoMv);
	}
    }

    /*
     * Collect the decimal significand as a high precision integer. The least
     * significant bit corresponds to bit M2+exponent+1 so it will need to be
     * shifted left by that many bits after being multiplied by
     * 5**(M5+exponent).
     */

    mp_init(&twoMd);
    mp_zero(&twoMd);
    i = nSigDigs;
    for (p=sigStart ;; ++p) {
	char c = *p;
	if (isdigit(UCHAR(c))) {
	    mp_mul_d(&twoMd, (unsigned) 10, &twoMd);
	    mp_add_d(&twoMd, (unsigned) (c - '0'), &twoMd);
	    --i;
	    if (i == 0) {
		break;
	    }
	}
    }
    for (i=0 ; i<=8 ; ++i) {
	if ((M5+exponent) & (1 << i)) {
	    mp_mul(&twoMd, pow5+i, &twoMd);
	}
    }
    mp_mul_2d(&twoMd, M2+exponent+1, &twoMd);
    mp_sub(&twoMd, &twoMv, &twoMd);

    /*
     * The result, 2Mv-2Md, needs to be divided by 2M to yield a correction
     * term. Because 2M may well overflow a double, we need to scale the
     * denominator by a factor of 2**binExponent-mantBits
     */

    scale = binExponent - mantBits - 1;

    mp_set(&twoMv, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (M5 & (1 << i)) {
	    mp_mul(&twoMv, pow5+i, &twoMv);
	}
    }
    multiplier = M2 + scale + 1;
    if (multiplier > 0) {
	mp_mul_2d(&twoMv, multiplier, &twoMv);
    } else if (multiplier < 0) {
	mp_div_2d(&twoMv, -multiplier, &twoMv, NULL);
    }

    /*
     * If the result is less than unity, the error is less than 1/2 unit in
     * the last place, so there's no correction to make.
     */

    if (mp_cmp_mag(&twoMd, &twoMv) == MP_LT) {
	mp_clear(&twoMd);
	mp_clear(&twoMv);
	return approxResult;
    }

    /*
     * Convert the numerator and denominator of the corrector term accurately
     * to floating point numbers.
     */

    num = TclBignumToDouble(&twoMd);
    den = TclBignumToDouble(&twoMv);

    quot = SafeLdExp(num/den, scale);
    minincr = SafeLdExp(1.0, binExponent - mantBits);

    if (quot<0. && quot>-minincr) {
	quot = -minincr;
    } else if (quot>0. && quot<minincr) {
	quot = minincr;
    }

    mp_clear(&twoMd);
    mp_clear(&twoMv);

    return approxResult + quot;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseNaN --
 *
 *	Parses a "not a number" from an input string, and returns the double
 *	precision NaN corresponding to it.
 *
 * Side effects:
 *	Advances endPtr to follow any (hex) in the input string.
 *
 *	If the NaN is followed by a left paren, a string of spaes and
 *	hexadecimal digits, and a right paren, endPtr is advanced to follow
 *	it.
 *
 *	The string of hexadecimal digits is OR'ed into the resulting NaN, and
 *	the signum is set as well.  Note that a signalling NaN is never
 *	returned.
 *
 *----------------------------------------------------------------------
 */

#ifdef IEEE_FLOATING_POINT
static double
ParseNaN(int signum,		/* Flag == 1 if minus sign has been seen in
				 * front of NaN. */
	 CONST char** endPtr)	/* Pointer-to-pointer to char following "NaN"
				 * in the input string. */
{
    const char* p = *endPtr;
    char c;
    union {
	Tcl_WideUInt iv;
	double dv;
    } theNaN;

    /*
     * Scan off a hex number in parentheses.  Embedded blanks are ok.
     */

    theNaN.iv = 0;
    if (*p == '(') {
	++p;
	for (;;) {
	    c = *p++;
	    if (isspace(UCHAR(c))) {
		continue;
	    } else if (c == ')') {
		*endPtr = p;
		break;
	    } else if (isdigit(UCHAR(c))) {
		c -= '0';
	    } else if (c >= 'A' && c <= 'F') {
		c -= 'A' + 10;
	    } else if (c >= 'a' && c <= 'f') {
		c -= 'a' + 10;
	    } else {
		theNaN.iv = (((Tcl_WideUInt) NAN_START) << 48)
			| (((Tcl_WideUInt) signum) << 63);
		return theNaN.dv;
	    }
	    theNaN.iv = (theNaN.iv << 4) | c;
	}
    }

    /*
     * Mask the hex number down to the least significant 51 bits.
     */

    theNaN.iv &= ( ((Tcl_WideUInt) 1) << 51 ) - 1;
    if ( signum ) {
	theNaN.iv |= ((Tcl_WideUInt) (0x8000 | NAN_START)) << 48;
    } else {
	theNaN.iv |= ((Tcl_WideUInt) NAN_START) << 48;
    }

    *endPtr = p;
    return theNaN.dv;
}
#endif /* IEEE_FLOATING_POINT */

/*
 *----------------------------------------------------------------------
 *
 * TclDoubleDigits --
 *
 *	Converts a double to a string of digits.
 *
 * Results:
 *	Returns the position of the character in the string after which the
 *	decimal point should appear.  Since the string contains only
 *	significant digits, the position may be less than zero or greater than
 *	the length of the string.
 *
 * Side effects:
 *	Stores the digits in the given buffer and sets 'signum' according to
 *	the sign of the number.
 *
 *----------------------------------------------------------------------
 */

int
TclDoubleDigits( char * string,	/* Buffer in which to store the result,
				 * must have at least 18 chars */
		 double v,	/* Number to convert. Must be
				 * finite, and not NaN */
		 int *signum )	/* Output: 1 if the number is negative.
				 * Should handle -0 correctly on the
				 * IEEE architecture. */
{
    double f;			/* Significand of v. */
    int e;			/* Power of FLT_RADIX that satisfies
				 * v = f * FLT_RADIX**e */
    int lowOK, highOK;
    mp_int r;			/* Scaled significand. */
    mp_int s;			/* Divisor such that v = r / s */
    mp_int mplus;		/* Scaled epsilon: (r + 2* mplus) == v(+)
				 * where v(+) is the floating point successor
				 * of v. */
    mp_int mminus;		/* Scaled epsilon: (r - 2*mminus) == v(-)
				 * where v(-) is the floating point
				 * predecessor of v. */
    mp_int temp;
    int rfac2 = 0;		/* Powers of 2 and 5 by which large */
    int rfac5 = 0;		/* integers should be scaled.	    */
    int sfac2 = 0;
    int sfac5 = 0;
    int mplusfac2 = 0;
    int mminusfac2 = 0;
    double a;
    char c;
    int i, k, n;

    /*
     * Take the absolute value of the number, and report the number's sign.
     * Take special steps to preserve signed zeroes in IEEE floating point.
     * (We can't use fpclassify, because that's a C9x feature and we still
     * have to build on C89 compilers.)
     */

#ifndef IEEE_FLOATING_POINT
    if (v >= 0.0) {
	*signum = 0;
    } else {
	*signum = 1;
	v = -v;
    }
#else
    union {
	Tcl_WideUInt iv;
	double dv;
    } bitwhack;
    bitwhack.dv = v;
    if (bitwhack.iv & ((Tcl_WideUInt) 1 << 63)) {
	*signum = 1;
	bitwhack.iv &= ~((Tcl_WideUInt) 1 << 63);
	v = bitwhack.dv;
    } else {
	*signum = 0;
    }
#endif

    /*
     * Handle zero specially.
     */

    if ( v == 0.0 ) {
	*string++ = '0';
	*string++ = '\0';
	return 1;
    }

    /*
     * Develop f and e such that v = f * FLT_RADIX**e, with
     * 1.0/FLT_RADIX <= f < 1.
     */

    f = frexp(v, &e);
    n = e % log2FLT_RADIX;
    if (n > 0) {
	n -= log2FLT_RADIX;
	e += 1;
    }
    f *= ldexp(1.0, n);
    e = (e - n) / log2FLT_RADIX;
    if (f == 1.0) {
	f = 1.0 / FLT_RADIX;
	e += 1;
    }

    /*
     * If the original number was denormalized, adjust e and f to be denormal
     * as well.
     */

    if (e < DBL_MIN_EXP) {
	n = mantBits + (e - DBL_MIN_EXP)*log2FLT_RADIX;
	f = ldexp(f, (e - DBL_MIN_EXP)*log2FLT_RADIX);
	e = DBL_MIN_EXP;
	n = (n + DIGIT_BIT - 1) / DIGIT_BIT;
    } else {
	n = mantDIGIT;
    }

    /*
     * Now extract the base-2**DIGIT_BIT digits of f into a multi-precision
     * integer r. Preserve the invariant v = r * 2**rfac2 * FLT_RADIX**e by
     * adjusting e.
     */

    a = f;
    n = mantDIGIT;
    mp_init_size(&r, n);
    r.used = n;
    r.sign = MP_ZPOS;
    i = (mantBits % DIGIT_BIT);
    if (i == 0) {
	i = DIGIT_BIT;
    }
    while (n > 0) {
	a *= ldexp(1.0, i);
	i = DIGIT_BIT;
	r.dp[--n] = (mp_digit) a;
	a -= (mp_digit) a;
    }
    e -= DBL_MANT_DIG;

    lowOK = highOK = (mp_iseven(&r));

    /*
     * We are going to want to develop integers r, s, mplus, and mminus such
     * that v = r / s, v(+)-v / 2 = mplus / s; v-v(-) / 2 = mminus / s and
     * then scale either s or r, mplus, mminus by an appropriate power of ten.
     *
     * We actually do this by keeping track of the powers of 2 and 5 by which
     * f is multiplied to yield v and by which 1 is multiplied to yield s,
     * mplus, and mminus.
     */

    if (e >= 0) {
	int bits = e * log2FLT_RADIX;

	if (f != 1.0/FLT_RADIX) {
	    /*
	     * Normal case, m+ and m- are both FLT_RADIX**e
	     */

	    rfac2 += bits + 1;
	    sfac2 = 1;
	    mplusfac2 = bits;
	    mminusfac2 = bits;
	} else {
	    /*
	     * If f is equal to the smallest significand, then we need another
	     * factor of FLT_RADIX in s to cope with stepping to the next
	     * smaller exponent when going to e's predecessor.
	     */

	    rfac2 += bits + log2FLT_RADIX - 1;
	    sfac2 = 1 + log2FLT_RADIX;
	    mplusfac2 = bits + log2FLT_RADIX;
	    mminusfac2 = bits;
	}
    } else {
	/*
	 * v has digits after the binary point
	 */

	if (e <= DBL_MIN_EXP-DBL_MANT_DIG || f != 1.0/FLT_RADIX) {
	    /*
	     * Either f isn't the smallest significand or e is the smallest
	     * exponent.  mplus and mminus will both be 1.
	     */

	    rfac2 += 1;
	    sfac2 = 1 - e * log2FLT_RADIX;
	    mplusfac2 = 0;
	    mminusfac2 = 0;
	} else {
	    /*
	     * f is the smallest significand, but e is not the smallest
	     * exponent. We need to scale by FLT_RADIX again to cope with the
	     * fact that v's predecessor has a smaller exponent.
	     */

	    rfac2 += 1 + log2FLT_RADIX;
	    sfac2 = 1 + log2FLT_RADIX * (1 - e);
	    mplusfac2 = FLT_RADIX;
	    mminusfac2 = 0;
	}
    }

    /*
     * Estimate the highest power of ten that will be needed to hold the
     * result.
     */

    k = (int) ceil(log(v) / log(10.));
    if (k >= 0) {
	sfac2 += k;
	sfac5 = k;
    } else {
	rfac2 -= k;
	mplusfac2 -= k;
	mminusfac2 -= k;
	rfac5 = -k;
    }

    /*
     * Scale r, s, mplus, mminus by the appropriate powers of 2 and 5.
     */

    mp_init_set(&mplus, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (rfac5 & (1 << i)) {
	    mp_mul(&mplus, pow5+i, &mplus);
	}
    }
    mp_mul(&r, &mplus, &r);
    mp_mul_2d(&r, rfac2, &r);
    mp_init_copy(&mminus, &mplus);
    mp_mul_2d(&mplus, mplusfac2, &mplus);
    mp_mul_2d(&mminus, mminusfac2, &mminus);
    mp_init_set(&s, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (sfac5 & (1 << i)) {
	    mp_mul(&s, pow5+i, &s);
	}
    }
    mp_mul_2d(&s, sfac2, &s);

    /*
     * It is possible for k to be off by one because we used an inexact
     * logarithm.
     */

    mp_init(&temp);
    mp_add(&r, &mplus, &temp);
    i = mp_cmp_mag(&temp, &s);
    if (i>0 || (highOK && i==0)) {
	mp_mul_d(&s, 10, &s);
	++k;
    } else {
	mp_mul_d(&temp, 10, &temp);
	i = mp_cmp_mag(&temp, &s);
	if (i<0 || (highOK && i==0)) {
	    mp_mul_d(&r, 10, &r);
	    mp_mul_d(&mplus, 10, &mplus);
	    mp_mul_d(&mminus, 10, &mminus);
	    --k;
	}
    }

    /*
     * At this point, k contains the power of ten by which we're scaling the
     * result. r/s is at least 1/10 and strictly less than ten, and v = r/s *
     * 10**k.  mplus and mminus give the rounding limits.
     */

    for (;;) {
	int tc1, tc2;

	mp_mul_d(&r, 10, &r);
	mp_div(&r, &s, &temp, &r);	/* temp = 10r / s; r = 10r mod s */
	i = temp.dp[0];
	mp_mul_d(&mplus, 10, &mplus);
	mp_mul_d(&mminus, 10, &mminus);
	tc1 = mp_cmp_mag(&r, &mminus);
	if (lowOK) {
	    tc1 = (tc1 <= 0);
	} else {
	    tc1 = (tc1 < 0);
	}
	mp_add(&r, &mplus, &temp);
	tc2 = mp_cmp_mag(&temp, &s);
	if (highOK) {
	    tc2 = (tc2 >= 0);
	} else {
	    tc2= (tc2 > 0);
	}
	if ( ! tc1 ) {
	    if ( !tc2 ) {
		*string++ = '0' + i;
	    } else {
		c = (char) (i + '1');
		break;
	    }
	} else {
	    if (!tc2) {
		c = (char) (i + '0');
	    } else {
		mp_mul_2d(&r, 1, &r);
		n = mp_cmp_mag(&r, &s);
		if (n < 0) {
		    c = (char) (i + '0');
		} else {
		    c = (char) (i + '1');
		}
	    }
	    break;
	}
    };
    *string++ = c;
    *string++ = '\0';

    /*
     * Free memory, and return.
     */

    mp_clear_multi(&r, &s, &mplus, &mminus, &temp, NULL);
    return k;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitDoubleConversion --
 *
 *	Initializes constants that are needed for conversions to and from
 *	'double'
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The log base 2 of the floating point radix, the number of bits in a
 *	double mantissa, and a table of the powers of five and ten are
 *	computed and stored.
 *
 *----------------------------------------------------------------------
 */

void
TclInitDoubleConversion(void)
{
    int i;
    int x;
    Tcl_WideUInt u;
    double d;

    /*
     * Initialize table of powers of 10 expressed as wide integers.
     */

    maxpow10_wide =
	(int) floor(sizeof (Tcl_WideUInt) * CHAR_BIT * log (2.) / log (10.));
    pow10_wide = (Tcl_WideUInt*) Tcl_Alloc ((maxpow10_wide + 1)
					    * sizeof (Tcl_WideUInt));
    u = 1;
    for (i = 0; i < maxpow10_wide; ++i) {
	pow10_wide[i] = u;
	u *= 10;
    }
    pow10_wide[i] = u;

    /*
     * Determine how many bits of precision a double has, and how many
     * decimal digits that represents.
     */

    if ( frexp( (double) FLT_RADIX, &log2FLT_RADIX ) != 0.5 ) {
	Tcl_Panic( "This code doesn't work on a decimal machine!" );
    }
    --log2FLT_RADIX;
    mantBits = DBL_MANT_DIG * log2FLT_RADIX;
    d = 1.0;

    /* 
     * Initialize a table of powers of ten that can be exactly represented
     * in a double.
     */

    x = (int) (DBL_MANT_DIG * log((double) FLT_RADIX) / log( 5.0 ));
    if ( x < MAXPOW ) {
	mmaxpow = x;
    } else {
	mmaxpow = MAXPOW;
    }
    for (i=0 ; i<=mmaxpow ; ++i) {
	pow10[i] = d;
	d *= 10.0;
    }

    /* Initialize a table of large powers of five. */

    for ( i = 0; i < 9; ++i ) {
	mp_init( pow5 + i );
    }
    mp_set( pow5, 5 );
    for ( i = 0; i < 8; ++i ) {
	mp_sqr( pow5+i, pow5+i+1 );
    }

    /* 
     * Determine the number of decimal digits to the left and right of the
     * decimal point in the largest and smallest double, the smallest double
     * that differs from zero, and the number of mp_digits needed to represent 
     * the significand of a double.
     */

    tiny = SafeLdExp( 1.0, DBL_MIN_EXP * log2FLT_RADIX - mantBits );
    maxDigits = (int) ((DBL_MAX_EXP * log((double) FLT_RADIX)
			+ 0.5 * log(10.))
		       / log( 10. ));
    minDigits = (int) floor ( ( DBL_MIN_EXP - DBL_MANT_DIG )
			      * log( (double) FLT_RADIX ) / log( 10. ) );
    mantDIGIT = ( mantBits + DIGIT_BIT - 1 ) / DIGIT_BIT;
    log10_DIGIT_MAX = (int) floor (DIGIT_BIT * log(2.) / log (10.));
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeDoubleConversion --
 *
 *	Cleans up this file on exit.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Memory allocated by TclInitDoubleConversion is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeDoubleConversion()
{
    int i;
    Tcl_Free ((char*)pow10_wide);
    for ( i = 0; i < 9; ++i ) {
	mp_clear( pow5 + i );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitBignumFromDouble --
 *
 *	Extracts the integer part of a double and converts it to
 *	an arbitrary precision integer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the bignum supplied, and stores the converted number
 *      in it.
 *
 *----------------------------------------------------------------------
 */

void
TclInitBignumFromDouble(double d,	/* Number to convert */
			mp_int* b)	/* Place to store the result */
{
    double fract;
    int expt;
    fract = frexp(d,&expt);
    if (expt <= 0) {
	mp_init(b);
	mp_zero(b);
    } else {
	Tcl_WideInt w = (Tcl_WideInt) ldexp(fract, mantBits);
	int signum = 0;
	int shift = expt - mantBits;
	Tcl_WideUInt uw;
	if (w < 0) {
	    uw = (Tcl_WideUInt)-w;
	    signum = 1;
	} else {
	    uw = w;
	}
	TclBNInitBignumFromWideUInt(b, uw);
	if (shift < 0) {
	    mp_div_2d(b, -shift, b, NULL);
	} else if (shift > 0) {
	    mp_mul_2d(b, shift, b);
	}
	if (signum) {
	    b->sign = MP_NEG;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclBignumToDouble --
 *
 *	Convert an arbitrary-precision integer to a native floating point
 *	number.
 *
 * Results:
 *	Returns the converted number.  Sets errno to ERANGE if the number is
 *	too large to convert.
 *
 *----------------------------------------------------------------------
 */

double
TclBignumToDouble(mp_int *a)	/* Integer to convert. */
{
    mp_int b;
    int bits;
    int shift;
    int i;
    double r;

    /*
     * Determine how many bits we need, and extract that many from the input.
     * Round to nearest unit in the last place.
     */

    bits = mp_count_bits(a);
    if (bits > DBL_MAX_EXP*log2FLT_RADIX) {
	errno = ERANGE;
	return HUGE_VAL;
    }
    shift = mantBits + 1 - bits;
    mp_init(&b);
    if (shift > 0) {
	mp_mul_2d(a, shift, &b);
    } else if (shift < 0) {
	mp_div_2d(a, -shift, &b, NULL);
    } else {
	mp_copy(a, &b);
    }
    mp_add_d(&b, 1, &b);
    mp_div_2d(&b, 1, &b, NULL);

    /*
     * Accumulate the result, one mp_digit at a time.
     */

    r = 0.0;
    for (i=b.used-1 ; i>=0 ; --i) {
	r = ldexp(r, DIGIT_BIT) + b.dp[i];
    }
    mp_clear(&b);

    /*
     * Scale the result to the correct number of bits.
     */

    r = ldexp(r, bits - mantBits);

    /*
     * Return the result with the appropriate sign.
     */

    if (a->sign == MP_ZPOS) {
	return r;
    } else {
	return -r;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BignumToBiasedFrExp --
 *
 *	Convert an arbitrary-precision integer to a native floating
 *	point number in the range [0.5,1) times a power of two.
 *	NOTE: Intentionally converts to a number that's a few
 *	ulp too small, so that RefineApproximation will not overflow
 *	near the high end of the machine's arithmetic range.
 *
 * Results:
 *	Returns the converted number.
 *
 * Side effects:
 *	Stores the exponent of two in 'machexp'.
 *
 *----------------------------------------------------------------------
 */

static double
BignumToBiasedFrExp( mp_int* a,
				/* Integer to convert */
		     int* machexp )
				/* Power of two */
{
    mp_int b;
    int bits;
    int shift;
    int i;
    double r;

    /* Determine how many bits we need, and extract that many from 
     * the input. Round to nearest unit in the last place. */

    bits = mp_count_bits( a );
    shift = mantBits - 2 - bits;
    mp_init( &b );
    if ( shift > 0 ) {
	mp_mul_2d( a, shift, &b );
    } else if ( shift < 0 ) {
	mp_div_2d( a, -shift, &b, NULL );
    } else {
	mp_copy( a, &b );
    }

    /* Accumulate the result, one mp_digit at a time */

    r = 0.0;
    for ( i = b.used-1; i >= 0; --i ) {
	r = ldexp( r, DIGIT_BIT ) + b.dp[i];
    }
    mp_clear( &b );

    /* Return the result with the appropriate sign. */

    *machexp = bits - mantBits + 2;
    if ( a->sign == MP_ZPOS ) {
	return r;
    } else {
	return -r;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Pow10TimesFrExp --
 *
 *	Multiply a power of ten by a number expressed as fraction and
 *	exponent.
 *
 * Results:
 *	Returns the significand of the result.
 *
 * Side effects:
 *	Overwrites the 'machexp' parameter with the exponent of the
 *	result.
 *
 * Assumes that 'exponent' is such that 10**exponent would be a double,
 * even though 'fraction*10**(machexp+exponent)' might overflow.
 *
 *----------------------------------------------------------------------
 */

static double
Pow10TimesFrExp( int exponent, 	/* Power of 10 to multiply by */
		 double fraction,
				/* Significand of multiplicand */
		 int* machexp )	/* On input, exponent of multiplicand.
				 * On output, exponent of result. */
{
    int i, j;
    int expt = *machexp;
    double retval = fraction;

    if ( exponent > 0 ) {

	/* Multiply by 10**exponent */
	
	retval = frexp( retval * pow10[ exponent & 0xf ], &j );
	expt += j;
	for ( i = 4; i < 9; ++i ) {
	    if ( exponent & (1<<i) ) {
		retval = frexp( retval * pow_10_2_n[ i ], &j );
		expt += j;
	    }
	}
    } else if ( exponent < 0 ) {

	/* Divide by 10**-exponent */

	retval = frexp( retval / pow10[ (-exponent) & 0xf ], &j );
	expt += j;
	for ( i = 4; i < 9; ++i ) {
	    if ( (-exponent) & (1<<i) ) {
		retval = frexp( retval / pow_10_2_n[ i ], &j );
		expt += j;
	    }
	}
    }

    *machexp = expt;
    return retval;
    
}

/*
 *----------------------------------------------------------------------
 *
 * SafeLdExp --
 *
 *	Do an 'ldexp' operation, but handle denormals gracefully.
 *
 * Results:
 *	Returns the appropriately scaled value.
 *
 *	On some platforms, 'ldexp' fails when presented with a number too
 *	small to represent as a normalized double. This routine does 'ldexp'
 *	in two steps for those numbers, to return correctly denormalized
 *	values.
 *
 *----------------------------------------------------------------------
 */

static double
SafeLdExp(double fract, int expt)
{
    int minexpt = DBL_MIN_EXP * log2FLT_RADIX;
    volatile double a, b, retval;
    if (expt < minexpt) {
	a = ldexp(fract, expt - mantBits - minexpt);
	b = ldexp(1.0, mantBits + minexpt);
	retval = a * b;
    } else {
	retval = ldexp(fract, expt);
    }
    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFormatNaN --
 *
 *	Makes the string representation of a "Not a Number"
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the string representation in the supplied buffer, which must be
 *	at least TCL_DOUBLE_SPACE characters.
 *
 *----------------------------------------------------------------------
 */

void
TclFormatNaN(double value,	/* The Not-a-Number to format. */
	     char *buffer)	/* String representation. */
{
#ifndef IEEE_FLOATING_POINT
    strcpy(buffer, "NaN");
    return;
#else
    union {
	double dv;
	Tcl_WideUInt iv;
    } bitwhack;

    bitwhack.dv = value;
    if (bitwhack.iv & ((Tcl_WideUInt) 1 << 63)) {
	bitwhack.iv &= ~ ((Tcl_WideUInt) 1 << 63);
	*buffer++ = '-';
    }
    *buffer++ = 'N';
    *buffer++ = 'a';
    *buffer++ = 'N';
    bitwhack.iv &= (((Tcl_WideUInt) 1) << 51) - 1;
    if (bitwhack.iv != 0) {
	sprintf(buffer, "(%" TCL_LL_MODIFIER "x)", bitwhack.iv);
    } else {
	*buffer = '\0';
    }
#endif /* IEEE_FLOATING_POINT */
}
