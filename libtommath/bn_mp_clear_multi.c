#include <tommath.h>
#ifdef BN_MP_CLEAR_MULTI_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://math.libtomcrypt.com
 */
#include <stdarg.h>

void mp_clear_multi(mp_int *mp, ...) 
{
    mp_int* next_mp = mp;
    va_list args;
    va_start(args, mp);
    while (next_mp != NULL) {
        mp_clear(next_mp);
        next_mp = va_arg(args, mp_int*);
    }
    va_end(args);
}
#endif

/* $Source: /root/tcl/repos-to-convert/tcl/libtommath/bn_mp_clear_multi.c,v $ */
/* $Revision: 1.1.1.1.4.2 $ */
/* $Date: 2008/01/22 16:55:25 $ */
