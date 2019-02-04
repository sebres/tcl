/*
 *----------------------------------------------------------------------
 *
 * tclTomMathDecls.h --
 *
 *	This file contains the declarations for the 'libtommath'
 *	functions that are exported by the Tcl library.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLTOMMATHDECLS
#define _TCLTOMMATHDECLS

#include "tcl.h"

/*
 * Define the version of the Stubs table that's exported for tommath
 */

#define TCLTOMMATH_EPOCH 0
#define TCLTOMMATH_REVISION 0

#define Tcl_TomMath_InitStubs(interp,version) \
    (TclTomMathInitializeStubs((interp),(version),\
                               TCLTOMMATH_EPOCH,TCLTOMMATH_REVISION))

/* Define custom memory allocation for libtommath */

/* MODULE_SCOPE void* TclBNAlloc( size_t ); */
#define TclBNAlloc(s) ((void*)ckalloc((size_t)(s)))
/* MODULE_SCOPE void* TclBNRealloc( void*, size_t ); */
#define TclBNRealloc(x,s) ((void*)ckrealloc((char*)(x),(size_t)(s)))
/* MODULE_SCOPE void  TclBNFree( void* ); */
#define TclBNFree(x) (ckfree((char*)(x)))
/* MODULE_SCOPE void* TclBNCalloc( size_t, size_t ); */
/* unused - no macro */

#define XMALLOC(x) TclBNAlloc(x)
#define XFREE(x) TclBNFree(x)
#define XREALLOC(x,n) TclBNRealloc(x,n)
#define XCALLOC(n,x) TclBNCalloc(n,x)

/* Rename the global symbols in libtommath to avoid linkage conflicts */

#define KARATSUBA_MUL_CUTOFF TclBNKaratsubaMulCutoff
#define KARATSUBA_SQR_CUTOFF TclBNKaratsubaSqrCutoff
#define TOOM_MUL_CUTOFF TclBNToomMulCutoff
#define TOOM_SQR_CUTOFF TclBNToomSqrCutoff

#define bn_reverse TclBN_reverse
#define fast_s_mp_mul_digs TclBN_fast_s_mp_mul_digs
#define fast_s_mp_sqr TclBN_fast_s_mp_sqr
#define mp_add TclBN_mp_add
#define mp_add_d TclBN_mp_add_d
#define mp_and TclBN_mp_and
#define mp_clamp TclBN_mp_clamp
#define mp_clear TclBN_mp_clear
#define mp_clear_multi TclBN_mp_clear_multi
#define mp_cmp TclBN_mp_cmp
#define mp_cmp_d TclBN_mp_cmp_d
#define mp_cmp_mag TclBN_mp_cmp_mag
#define mp_cnt_lsb TclBN_mp_cnt_lsb
#define mp_copy TclBN_mp_copy
#define mp_count_bits TclBN_mp_count_bits
#define mp_div TclBN_mp_div
#define mp_div_2 TclBN_mp_div_2
#define mp_div_2d TclBN_mp_div_2d
#define mp_div_3 TclBN_mp_div_3
#define mp_div_d TclBN_mp_div_d
#define mp_exch TclBN_mp_exch
#define mp_expt_d TclBN_mp_expt_d
#define mp_expt_d_ex TclBN_mp_expt_d_ex
#define mp_grow TclBN_mp_grow
#define mp_init TclBN_mp_init
#define mp_init_copy TclBN_mp_init_copy
#define mp_init_multi TclBN_mp_init_multi
#define mp_init_set TclBN_mp_init_set
#define mp_init_set_int TclBN_mp_init_set_int
#define mp_init_size TclBN_mp_init_size
#define mp_karatsuba_mul TclBN_mp_karatsuba_mul
#define mp_karatsuba_sqr TclBN_mp_karatsuba_sqr
#define mp_lshd TclBN_mp_lshd
#define mp_mod TclBN_mp_mod
#define mp_mod_2d TclBN_mp_mod_2d
#define mp_mul TclBN_mp_mul
#define mp_mul_2 TclBN_mp_mul_2
#define mp_mul_2d TclBN_mp_mul_2d
#define mp_mul_d TclBN_mp_mul_d
#define mp_neg TclBN_mp_neg
#define mp_or TclBN_mp_or
#define mp_radix_size TclBN_mp_radix_size
#define mp_read_radix TclBN_mp_read_radix
#define mp_rshd TclBN_mp_rshd
#define mp_set TclBN_mp_set
#define mp_set_int TclBN_mp_set_int
#define mp_shrink TclBN_mp_shrink
#define mp_sqr TclBN_mp_sqr
#define mp_sqrt TclBN_mp_sqrt
#define mp_sub TclBN_mp_sub
#define mp_sub_d TclBN_mp_sub_d
#define mp_tc_and TclBN_mp_tc_and
#define mp_tc_div_2d TclBN_mp_tc_div_2d
#define mp_tc_or TclBN_mp_tc_or
#define mp_tc_xor TclBN_mp_tc_xor
#define mp_to_unsigned_bin TclBN_mp_to_unsigned_bin
#define mp_to_unsigned_bin_n TclBN_mp_to_unsigned_bin_n
#define mp_toom_mul TclBN_mp_toom_mul
#define mp_toom_sqr TclBN_mp_toom_sqr
#define mp_toradix_n TclBN_mp_toradix_n
#define mp_unsigned_bin_size TclBN_mp_unsigned_bin_size
#define mp_xor TclBN_mp_xor
#define mp_zero TclBN_mp_zero
#define s_mp_add TclBN_s_mp_add
#define s_mp_mul_digs TclBN_s_mp_mul_digs
#define s_mp_sqr TclBN_s_mp_sqr
#define s_mp_sub TclBN_s_mp_sub

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#      define TCL_STORAGE_CLASS
#   else
#      define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN int		TclBN_epoch(void);
/* 1 */
EXTERN int		TclBN_revision(void);
/* 2 */
EXTERN int		TclBN_mp_add(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 3 */
EXTERN int		TclBN_mp_add_d(const mp_int *a, mp_digit b,
				mp_int *c);
/* 4 */
EXTERN int		TclBN_mp_and(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 5 */
EXTERN void		TclBN_mp_clamp(mp_int *a);
/* 6 */
EXTERN void		TclBN_mp_clear(mp_int *a);
/* 7 */
EXTERN void		TclBN_mp_clear_multi(mp_int *a, ...);
/* 8 */
EXTERN int		TclBN_mp_cmp(const mp_int *a, const mp_int *b);
/* 9 */
EXTERN int		TclBN_mp_cmp_d(const mp_int *a, mp_digit b);
/* 10 */
EXTERN int		TclBN_mp_cmp_mag(const mp_int *a, const mp_int *b);
/* 11 */
EXTERN int		TclBN_mp_copy(const mp_int *a, mp_int *b);
/* 12 */
EXTERN int		TclBN_mp_count_bits(const mp_int *a);
/* 13 */
EXTERN int		TclBN_mp_div(const mp_int *a, const mp_int *b,
				mp_int *q, mp_int *r);
/* 14 */
EXTERN int		TclBN_mp_div_d(const mp_int *a, mp_digit b,
				mp_int *q, mp_digit *r);
/* 15 */
EXTERN int		TclBN_mp_div_2(const mp_int *a, mp_int *q);
/* 16 */
EXTERN int		TclBN_mp_div_2d(const mp_int *a, int b, mp_int *q,
				mp_int *r);
/* 17 */
EXTERN int		TclBN_mp_div_3(const mp_int *a, mp_int *q,
				mp_digit *r);
/* 18 */
EXTERN void		TclBN_mp_exch(mp_int *a, mp_int *b);
/* 19 */
EXTERN int		TclBN_mp_expt_d(const mp_int *a, mp_digit b,
				mp_int *c);
/* 20 */
EXTERN int		TclBN_mp_grow(mp_int *a, int size);
/* 21 */
EXTERN int		TclBN_mp_init(mp_int *a);
/* 22 */
EXTERN int		TclBN_mp_init_copy(mp_int *a, const mp_int *b);
/* 23 */
EXTERN int		TclBN_mp_init_multi(mp_int *a, ...);
/* 24 */
EXTERN int		TclBN_mp_init_set(mp_int *a, mp_digit b);
/* 25 */
EXTERN int		TclBN_mp_init_size(mp_int *a, int size);
/* 26 */
EXTERN int		TclBN_mp_lshd(mp_int *a, int shift);
/* 27 */
EXTERN int		TclBN_mp_mod(const mp_int *a, const mp_int *b,
				mp_int *r);
/* 28 */
EXTERN int		TclBN_mp_mod_2d(const mp_int *a, int b, mp_int *r);
/* 29 */
EXTERN int		TclBN_mp_mul(const mp_int *a, const mp_int *b,
				mp_int *p);
/* 30 */
EXTERN int		TclBN_mp_mul_d(const mp_int *a, mp_digit b,
				mp_int *p);
/* 31 */
EXTERN int		TclBN_mp_mul_2(const mp_int *a, mp_int *p);
/* 32 */
EXTERN int		TclBN_mp_mul_2d(const mp_int *a, int d, mp_int *p);
/* 33 */
EXTERN int		TclBN_mp_neg(const mp_int *a, mp_int *b);
/* 34 */
EXTERN int		TclBN_mp_or(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 35 */
EXTERN int		TclBN_mp_radix_size(const mp_int *a, int radix,
				int *size);
/* 36 */
EXTERN int		TclBN_mp_read_radix(mp_int *a, const char *str,
				int radix);
/* 37 */
EXTERN void		TclBN_mp_rshd(mp_int *a, int shift);
/* 38 */
EXTERN int		TclBN_mp_shrink(mp_int *a);
/* 39 */
EXTERN void		TclBN_mp_set(mp_int *a, mp_digit b);
/* 40 */
EXTERN int		TclBN_mp_sqr(const mp_int *a, mp_int *b);
/* 41 */
EXTERN int		TclBN_mp_sqrt(const mp_int *a, mp_int *b);
/* 42 */
EXTERN int		TclBN_mp_sub(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 43 */
EXTERN int		TclBN_mp_sub_d(const mp_int *a, mp_digit b,
				mp_int *c);
/* 44 */
EXTERN int		TclBN_mp_to_unsigned_bin(const mp_int *a,
				unsigned char *b);
/* 45 */
EXTERN int		TclBN_mp_to_unsigned_bin_n(const mp_int *a,
				unsigned char *b, unsigned long *outlen);
/* 46 */
EXTERN int		TclBN_mp_toradix_n(const mp_int *a, char *str,
				int radix, int maxlen);
/* 47 */
EXTERN int		TclBN_mp_unsigned_bin_size(const mp_int *a);
/* 48 */
EXTERN int		TclBN_mp_xor(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 49 */
EXTERN void		TclBN_mp_zero(mp_int *a);
/* 50 */
EXTERN void		TclBN_reverse(unsigned char *s, int len);
/* 51 */
EXTERN int		TclBN_fast_s_mp_mul_digs(const mp_int *a,
				const mp_int *b, mp_int *c, int digs);
/* 52 */
EXTERN int		TclBN_fast_s_mp_sqr(const mp_int *a, mp_int *b);
/* 53 */
EXTERN int		TclBN_mp_karatsuba_mul(const mp_int *a,
				const mp_int *b, mp_int *c);
/* 54 */
EXTERN int		TclBN_mp_karatsuba_sqr(const mp_int *a, mp_int *b);
/* 55 */
EXTERN int		TclBN_mp_toom_mul(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 56 */
EXTERN int		TclBN_mp_toom_sqr(const mp_int *a, mp_int *b);
/* 57 */
EXTERN int		TclBN_s_mp_add(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 58 */
EXTERN int		TclBN_s_mp_mul_digs(const mp_int *a, const mp_int *b,
				mp_int *c, int digs);
/* 59 */
EXTERN int		TclBN_s_mp_sqr(const mp_int *a, mp_int *b);
/* 60 */
EXTERN int		TclBN_s_mp_sub(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 61 */
EXTERN int		TclBN_mp_init_set_int(mp_int *a, unsigned long i);
/* 62 */
EXTERN int		TclBN_mp_set_int(mp_int *a, unsigned long i);
/* 63 */
EXTERN int		TclBN_mp_cnt_lsb(const mp_int *a);
/* 64 */
EXTERN void		TclBNInitBignumFromLong(mp_int *bignum, long initVal);
/* 65 */
EXTERN void		TclBNInitBignumFromWideInt(mp_int *bignum,
				Tcl_WideInt initVal);
/* 66 */
EXTERN void		TclBNInitBignumFromWideUInt(mp_int *bignum,
				Tcl_WideUInt initVal);
/* 67 */
EXTERN int		TclBN_mp_expt_d_ex(const mp_int *a, mp_digit b,
				mp_int *c, int fast);
/* 73 */
EXTERN int		TclBN_mp_tc_and(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 74 */
EXTERN int		TclBN_mp_tc_or(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 75 */
EXTERN int		TclBN_mp_tc_xor(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 76 */
EXTERN int		TclBN_mp_tc_div_2d(const mp_int *a, int b, mp_int *c);

typedef struct TclTomMathStubs {
    int magic;
    void *hooks;

    int (*tclBN_epoch) (void); /* 0 */
    int (*tclBN_revision) (void); /* 1 */
    int (*tclBN_mp_add) (const mp_int *a, const mp_int *b, mp_int *c); /* 2 */
    int (*tclBN_mp_add_d) (const mp_int *a, mp_digit b, mp_int *c); /* 3 */
    int (*tclBN_mp_and) (const mp_int *a, const mp_int *b, mp_int *c); /* 4 */
    void (*tclBN_mp_clamp) (mp_int *a); /* 5 */
    void (*tclBN_mp_clear) (mp_int *a); /* 6 */
    void (*tclBN_mp_clear_multi) (mp_int *a, ...); /* 7 */
    int (*tclBN_mp_cmp) (const mp_int *a, const mp_int *b); /* 8 */
    int (*tclBN_mp_cmp_d) (const mp_int *a, mp_digit b); /* 9 */
    int (*tclBN_mp_cmp_mag) (const mp_int *a, const mp_int *b); /* 10 */
    int (*tclBN_mp_copy) (const mp_int *a, mp_int *b); /* 11 */
    int (*tclBN_mp_count_bits) (const mp_int *a); /* 12 */
    int (*tclBN_mp_div) (const mp_int *a, const mp_int *b, mp_int *q, mp_int *r); /* 13 */
    int (*tclBN_mp_div_d) (const mp_int *a, mp_digit b, mp_int *q, mp_digit *r); /* 14 */
    int (*tclBN_mp_div_2) (const mp_int *a, mp_int *q); /* 15 */
    int (*tclBN_mp_div_2d) (const mp_int *a, int b, mp_int *q, mp_int *r); /* 16 */
    int (*tclBN_mp_div_3) (const mp_int *a, mp_int *q, mp_digit *r); /* 17 */
    void (*tclBN_mp_exch) (mp_int *a, mp_int *b); /* 18 */
    int (*tclBN_mp_expt_d) (const mp_int *a, mp_digit b, mp_int *c); /* 19 */
    int (*tclBN_mp_grow) (mp_int *a, int size); /* 20 */
    int (*tclBN_mp_init) (mp_int *a); /* 21 */
    int (*tclBN_mp_init_copy) (mp_int *a, const mp_int *b); /* 22 */
    int (*tclBN_mp_init_multi) (mp_int *a, ...); /* 23 */
    int (*tclBN_mp_init_set) (mp_int *a, mp_digit b); /* 24 */
    int (*tclBN_mp_init_size) (mp_int *a, int size); /* 25 */
    int (*tclBN_mp_lshd) (mp_int *a, int shift); /* 26 */
    int (*tclBN_mp_mod) (const mp_int *a, const mp_int *b, mp_int *r); /* 27 */
    int (*tclBN_mp_mod_2d) (const mp_int *a, int b, mp_int *r); /* 28 */
    int (*tclBN_mp_mul) (const mp_int *a, const mp_int *b, mp_int *p); /* 29 */
    int (*tclBN_mp_mul_d) (const mp_int *a, mp_digit b, mp_int *p); /* 30 */
    int (*tclBN_mp_mul_2) (const mp_int *a, mp_int *p); /* 31 */
    int (*tclBN_mp_mul_2d) (const mp_int *a, int d, mp_int *p); /* 32 */
    int (*tclBN_mp_neg) (const mp_int *a, mp_int *b); /* 33 */
    int (*tclBN_mp_or) (const mp_int *a, const mp_int *b, mp_int *c); /* 34 */
    int (*tclBN_mp_radix_size) (const mp_int *a, int radix, int *size); /* 35 */
    int (*tclBN_mp_read_radix) (mp_int *a, const char *str, int radix); /* 36 */
    void (*tclBN_mp_rshd) (mp_int *a, int shift); /* 37 */
    int (*tclBN_mp_shrink) (mp_int *a); /* 38 */
    void (*tclBN_mp_set) (mp_int *a, mp_digit b); /* 39 */
    int (*tclBN_mp_sqr) (const mp_int *a, mp_int *b); /* 40 */
    int (*tclBN_mp_sqrt) (const mp_int *a, mp_int *b); /* 41 */
    int (*tclBN_mp_sub) (const mp_int *a, const mp_int *b, mp_int *c); /* 42 */
    int (*tclBN_mp_sub_d) (const mp_int *a, mp_digit b, mp_int *c); /* 43 */
    int (*tclBN_mp_to_unsigned_bin) (const mp_int *a, unsigned char *b); /* 44 */
    int (*tclBN_mp_to_unsigned_bin_n) (const mp_int *a, unsigned char *b, unsigned long *outlen); /* 45 */
    int (*tclBN_mp_toradix_n) (const mp_int *a, char *str, int radix, int maxlen); /* 46 */
    int (*tclBN_mp_unsigned_bin_size) (const mp_int *a); /* 47 */
    int (*tclBN_mp_xor) (const mp_int *a, const mp_int *b, mp_int *c); /* 48 */
    void (*tclBN_mp_zero) (mp_int *a); /* 49 */
    void (*tclBN_reverse) (unsigned char *s, int len); /* 50 */
    int (*tclBN_fast_s_mp_mul_digs) (const mp_int *a, const mp_int *b, mp_int *c, int digs); /* 51 */
    int (*tclBN_fast_s_mp_sqr) (const mp_int *a, mp_int *b); /* 52 */
    int (*tclBN_mp_karatsuba_mul) (const mp_int *a, const mp_int *b, mp_int *c); /* 53 */
    int (*tclBN_mp_karatsuba_sqr) (const mp_int *a, mp_int *b); /* 54 */
    int (*tclBN_mp_toom_mul) (const mp_int *a, const mp_int *b, mp_int *c); /* 55 */
    int (*tclBN_mp_toom_sqr) (const mp_int *a, mp_int *b); /* 56 */
    int (*tclBN_s_mp_add) (const mp_int *a, const mp_int *b, mp_int *c); /* 57 */
    int (*tclBN_s_mp_mul_digs) (const mp_int *a, const mp_int *b, mp_int *c, int digs); /* 58 */
    int (*tclBN_s_mp_sqr) (const mp_int *a, mp_int *b); /* 59 */
    int (*tclBN_s_mp_sub) (const mp_int *a, const mp_int *b, mp_int *c); /* 60 */
    int (*tclBN_mp_init_set_int) (mp_int *a, unsigned long i); /* 61 */
    int (*tclBN_mp_set_int) (mp_int *a, unsigned long i); /* 62 */
    int (*tclBN_mp_cnt_lsb) (const mp_int *a); /* 63 */
    void (*tclBNInitBignumFromLong) (mp_int *bignum, long initVal); /* 64 */
    void (*tclBNInitBignumFromWideInt) (mp_int *bignum, Tcl_WideInt initVal); /* 65 */
    void (*tclBNInitBignumFromWideUInt) (mp_int *bignum, Tcl_WideUInt initVal); /* 66 */
    int (*tclBN_mp_expt_d_ex) (const mp_int *a, mp_digit b, mp_int *c, int fast); /* 67 */
    int (*tclBN_mp_tc_and) (const mp_int *a, const mp_int *b, mp_int *c); /* 73 */
    int (*tclBN_mp_tc_or) (const mp_int *a, const mp_int *b, mp_int *c); /* 74 */
    int (*tclBN_mp_tc_xor) (const mp_int *a, const mp_int *b, mp_int *c); /* 75 */
    int (*tclBN_mp_tc_div_2d) (const mp_int *a, int b, mp_int *c); /* 76 */
} TclTomMathStubs;

extern const TclTomMathStubs *tclTomMathStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS)

/*
 * Inline function declarations:
 */

#define TclBN_epoch \
	(tclTomMathStubsPtr->tclBN_epoch) /* 0 */
#define TclBN_revision \
	(tclTomMathStubsPtr->tclBN_revision) /* 1 */
#define TclBN_mp_add \
	(tclTomMathStubsPtr->tclBN_mp_add) /* 2 */
#define TclBN_mp_add_d \
	(tclTomMathStubsPtr->tclBN_mp_add_d) /* 3 */
#define TclBN_mp_and \
	(tclTomMathStubsPtr->tclBN_mp_and) /* 4 */
#define TclBN_mp_clamp \
	(tclTomMathStubsPtr->tclBN_mp_clamp) /* 5 */
#define TclBN_mp_clear \
	(tclTomMathStubsPtr->tclBN_mp_clear) /* 6 */
#define TclBN_mp_clear_multi \
	(tclTomMathStubsPtr->tclBN_mp_clear_multi) /* 7 */
#define TclBN_mp_cmp \
	(tclTomMathStubsPtr->tclBN_mp_cmp) /* 8 */
#define TclBN_mp_cmp_d \
	(tclTomMathStubsPtr->tclBN_mp_cmp_d) /* 9 */
#define TclBN_mp_cmp_mag \
	(tclTomMathStubsPtr->tclBN_mp_cmp_mag) /* 10 */
#define TclBN_mp_copy \
	(tclTomMathStubsPtr->tclBN_mp_copy) /* 11 */
#define TclBN_mp_count_bits \
	(tclTomMathStubsPtr->tclBN_mp_count_bits) /* 12 */
#define TclBN_mp_div \
	(tclTomMathStubsPtr->tclBN_mp_div) /* 13 */
#define TclBN_mp_div_d \
	(tclTomMathStubsPtr->tclBN_mp_div_d) /* 14 */
#define TclBN_mp_div_2 \
	(tclTomMathStubsPtr->tclBN_mp_div_2) /* 15 */
#define TclBN_mp_div_2d \
	(tclTomMathStubsPtr->tclBN_mp_div_2d) /* 16 */
#define TclBN_mp_div_3 \
	(tclTomMathStubsPtr->tclBN_mp_div_3) /* 17 */
#define TclBN_mp_exch \
	(tclTomMathStubsPtr->tclBN_mp_exch) /* 18 */
#define TclBN_mp_expt_d \
	(tclTomMathStubsPtr->tclBN_mp_expt_d) /* 19 */
#define TclBN_mp_grow \
	(tclTomMathStubsPtr->tclBN_mp_grow) /* 20 */
#define TclBN_mp_init \
	(tclTomMathStubsPtr->tclBN_mp_init) /* 21 */
#define TclBN_mp_init_copy \
	(tclTomMathStubsPtr->tclBN_mp_init_copy) /* 22 */
#define TclBN_mp_init_multi \
	(tclTomMathStubsPtr->tclBN_mp_init_multi) /* 23 */
#define TclBN_mp_init_set \
	(tclTomMathStubsPtr->tclBN_mp_init_set) /* 24 */
#define TclBN_mp_init_size \
	(tclTomMathStubsPtr->tclBN_mp_init_size) /* 25 */
#define TclBN_mp_lshd \
	(tclTomMathStubsPtr->tclBN_mp_lshd) /* 26 */
#define TclBN_mp_mod \
	(tclTomMathStubsPtr->tclBN_mp_mod) /* 27 */
#define TclBN_mp_mod_2d \
	(tclTomMathStubsPtr->tclBN_mp_mod_2d) /* 28 */
#define TclBN_mp_mul \
	(tclTomMathStubsPtr->tclBN_mp_mul) /* 29 */
#define TclBN_mp_mul_d \
	(tclTomMathStubsPtr->tclBN_mp_mul_d) /* 30 */
#define TclBN_mp_mul_2 \
	(tclTomMathStubsPtr->tclBN_mp_mul_2) /* 31 */
#define TclBN_mp_mul_2d \
	(tclTomMathStubsPtr->tclBN_mp_mul_2d) /* 32 */
#define TclBN_mp_neg \
	(tclTomMathStubsPtr->tclBN_mp_neg) /* 33 */
#define TclBN_mp_or \
	(tclTomMathStubsPtr->tclBN_mp_or) /* 34 */
#define TclBN_mp_radix_size \
	(tclTomMathStubsPtr->tclBN_mp_radix_size) /* 35 */
#define TclBN_mp_read_radix \
	(tclTomMathStubsPtr->tclBN_mp_read_radix) /* 36 */
#define TclBN_mp_rshd \
	(tclTomMathStubsPtr->tclBN_mp_rshd) /* 37 */
#define TclBN_mp_shrink \
	(tclTomMathStubsPtr->tclBN_mp_shrink) /* 38 */
#define TclBN_mp_set \
	(tclTomMathStubsPtr->tclBN_mp_set) /* 39 */
#define TclBN_mp_sqr \
	(tclTomMathStubsPtr->tclBN_mp_sqr) /* 40 */
#define TclBN_mp_sqrt \
	(tclTomMathStubsPtr->tclBN_mp_sqrt) /* 41 */
#define TclBN_mp_sub \
	(tclTomMathStubsPtr->tclBN_mp_sub) /* 42 */
#define TclBN_mp_sub_d \
	(tclTomMathStubsPtr->tclBN_mp_sub_d) /* 43 */
#define TclBN_mp_to_unsigned_bin \
	(tclTomMathStubsPtr->tclBN_mp_to_unsigned_bin) /* 44 */
#define TclBN_mp_to_unsigned_bin_n \
	(tclTomMathStubsPtr->tclBN_mp_to_unsigned_bin_n) /* 45 */
#define TclBN_mp_toradix_n \
	(tclTomMathStubsPtr->tclBN_mp_toradix_n) /* 46 */
#define TclBN_mp_unsigned_bin_size \
	(tclTomMathStubsPtr->tclBN_mp_unsigned_bin_size) /* 47 */
#define TclBN_mp_xor \
	(tclTomMathStubsPtr->tclBN_mp_xor) /* 48 */
#define TclBN_mp_zero \
	(tclTomMathStubsPtr->tclBN_mp_zero) /* 49 */
#define TclBN_reverse \
	(tclTomMathStubsPtr->tclBN_reverse) /* 50 */
#define TclBN_fast_s_mp_mul_digs \
	(tclTomMathStubsPtr->tclBN_fast_s_mp_mul_digs) /* 51 */
#define TclBN_fast_s_mp_sqr \
	(tclTomMathStubsPtr->tclBN_fast_s_mp_sqr) /* 52 */
#define TclBN_mp_karatsuba_mul \
	(tclTomMathStubsPtr->tclBN_mp_karatsuba_mul) /* 53 */
#define TclBN_mp_karatsuba_sqr \
	(tclTomMathStubsPtr->tclBN_mp_karatsuba_sqr) /* 54 */
#define TclBN_mp_toom_mul \
	(tclTomMathStubsPtr->tclBN_mp_toom_mul) /* 55 */
#define TclBN_mp_toom_sqr \
	(tclTomMathStubsPtr->tclBN_mp_toom_sqr) /* 56 */
#define TclBN_s_mp_add \
	(tclTomMathStubsPtr->tclBN_s_mp_add) /* 57 */
#define TclBN_s_mp_mul_digs \
	(tclTomMathStubsPtr->tclBN_s_mp_mul_digs) /* 58 */
#define TclBN_s_mp_sqr \
	(tclTomMathStubsPtr->tclBN_s_mp_sqr) /* 59 */
#define TclBN_s_mp_sub \
	(tclTomMathStubsPtr->tclBN_s_mp_sub) /* 60 */
#define TclBN_mp_init_set_int \
	(tclTomMathStubsPtr->tclBN_mp_init_set_int) /* 61 */
#define TclBN_mp_set_int \
	(tclTomMathStubsPtr->tclBN_mp_set_int) /* 62 */
#define TclBN_mp_cnt_lsb \
	(tclTomMathStubsPtr->tclBN_mp_cnt_lsb) /* 63 */
#define TclBNInitBignumFromLong \
	(tclTomMathStubsPtr->tclBNInitBignumFromLong) /* 64 */
#define TclBNInitBignumFromWideInt \
	(tclTomMathStubsPtr->tclBNInitBignumFromWideInt) /* 65 */
#define TclBNInitBignumFromWideUInt \
	(tclTomMathStubsPtr->tclBNInitBignumFromWideUInt) /* 66 */
#define TclBN_mp_expt_d_ex \
	(tclTomMathStubsPtr->tclBN_mp_expt_d_ex) /* 67 */
#define TclBN_mp_tc_and \
	(tclTomMathStubsPtr->tclBN_mp_tc_and) /* 73 */
#define TclBN_mp_tc_or \
	(tclTomMathStubsPtr->tclBN_mp_tc_or) /* 74 */
#define TclBN_mp_tc_xor \
	(tclTomMathStubsPtr->tclBN_mp_tc_xor) /* 75 */
#define TclBN_mp_tc_div_2d \
	(tclTomMathStubsPtr->tclBN_mp_tc_div_2d) /* 76 */

#endif /* defined(USE_TCL_STUBS) */

/* !END!: Do not edit above this line. */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLINTDECLS */
