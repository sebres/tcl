#include "tommath_private.h"
#ifdef BN_MP_INIT_C
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

/* init a new mp_int */
int mp_init(mp_int *a)
{
   int i;

   /* allocate memory required and clear it */
   a->dp = (mp_digit *) XMALLOC(MP_PREC * sizeof(mp_digit));
   if (a->dp == NULL) {
      return MP_MEM;
   }

   /* set the digits to zero */
   for (i = 0; i < MP_PREC; i++) {
      a->dp[i] = 0;
   }

   /* set the used to zero, allocated digits to the default precision
    * and sign to positive */
   a->used  = 0;
   a->alloc = MP_PREC;
   a->sign  = MP_ZPOS;

   return MP_OKAY;
}
#endif

/* ref:         $Format:%D$ */
/* git commit:  $Format:%H$ */
/* commit time: $Format:%ai$ */
