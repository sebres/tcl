#include <tommath_private.h>
#ifdef BN_MP_REDUCE_IS_2K_C
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
 * Tom St Denis, tstdenis82@gmail.com, http://libtom.org
 */

/* determines if mp_reduce_2k can be used */
int mp_reduce_is_2k(mp_int *a)
{
   int ix, iy, iw;
   mp_digit iz;
   
   if (a->used == 0) {
      return MP_NO;
   } else if (a->used == 1) {
      return MP_YES;
   } else if (a->used > 1) {
      iy = mp_count_bits(a);
      iz = 1;
      iw = 1;
    
      /* Test every bit from the second digit up, must be 1 */
      for (ix = DIGIT_BIT; ix < iy; ix++) {
          if ((a->dp[iw] & iz) == 0) {
             return MP_NO;
          }
          iz <<= 1;
          if (iz > (mp_digit)MP_MASK) {
             ++iw;
             iz = 1;
          }
      }
   }
   return MP_YES;
}

#endif

/* ref:         HEAD -> release/1.0.1, tag: v1.0.1-rc2 */
/* git commit:  e8c27ba7df0efb90708029115c94d681dfa7812f */
/* commit time: 2017-08-29 10:48:46 +0200 */
