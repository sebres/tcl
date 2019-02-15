#include "tommath_private.h"
#ifdef BN_MP_GET_LONG_LONG_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * SPDX-License-Identifier: Unlicense
 */

/* get the lower unsigned long long of an mp_int, platform dependent */
unsigned TCL_WIDE_INT_TYPE mp_get_long_long(const mp_int *a)
{
   int i;
   unsigned TCL_WIDE_INT_TYPE res;

   if (a->used == 0) {
      return 0;
   }

   /* get number of digits of the lsb we have to read */
   i = MIN(a->used, ((((int)sizeof(unsigned TCL_WIDE_INT_TYPE) * CHAR_BIT) + DIGIT_BIT - 1) / DIGIT_BIT)) - 1;

   /* get most significant digit of result */
   res = (unsigned TCL_WIDE_INT_TYPE)a->dp[i];

#if DIGIT_BIT < 64
   while (--i >= 0) {
      res = (res << DIGIT_BIT) | (unsigned TCL_WIDE_INT_TYPE)a->dp[i];
   }
#endif
   return res;
}
#endif

/* ref:         $Format:%D$ */
/* git commit:  $Format:%H$ */
/* commit time: $Format:%ai$ */
