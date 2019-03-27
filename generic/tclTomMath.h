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
 */
#ifndef BN_H_
#define BN_H_

#include "tclTomMathDecls.h"
#ifndef MODULE_SCOPE
#define MODULE_SCOPE extern
#endif



#ifdef __cplusplus
extern "C" {
#endif

/* MS Visual C++ doesn't have a 128bit type for words, so fall back to 32bit MPI's (where words are 64bit) */
#if defined(_MSC_VER) || defined(__LLP64__) || defined(__e2k__) || defined(__LCC__)
#   define MP_32BIT
#endif

/* detect 64-bit mode if possible */
#if defined(NEVER)
#   if !(defined(MP_32BIT) || defined(MP_16BIT) || defined(MP_8BIT))
#      if defined(__GNUC__)
/* we support 128bit integers only via: __attribute__((mode(TI))) */
#         define MP_64BIT
#      else
/* otherwise we fall back to MP_32BIT even on 64bit platforms */
#         define MP_32BIT
#      endif
#   endif
#endif

/* some default configurations.
 *
 * A "mp_digit" must be able to hold DIGIT_BIT + 1 bits
 * A "mp_word" must be able to hold 2*DIGIT_BIT + 1 bits
 *
 * At the very least a mp_digit must be able to hold 7 bits
 * [any size beyond that is ok provided it doesn't overflow the data type]
 */
#ifdef MP_8BIT
#ifndef MP_DIGIT_DECLARED
typedef unsigned char        mp_digit;
#define MP_DIGIT_DECLARED
#endif
#ifndef MP_WORD_DECLARED
typedef unsigned short       mp_word;
#define MP_WORD_DECLARED
#endif
#   define MP_SIZEOF_MP_DIGIT 1
#   ifdef DIGIT_BIT
#      error You must not define DIGIT_BIT when using MP_8BIT
#   endif
#elif defined(MP_16BIT)
#ifndef MP_DIGIT_DECLARED
typedef unsigned short       mp_digit;
#define MP_DIGIT_DECLARED
#endif
#ifndef MP_WORD_DECLARED
typedef unsigned int         mp_word;
#define MP_WORD_DECLARED
#endif
#   define MP_SIZEOF_MP_DIGIT 2
#   ifdef DIGIT_BIT
#      error You must not define DIGIT_BIT when using MP_16BIT
#   endif
#elif defined(MP_64BIT)
/* for GCC only on supported platforms */
#ifndef MP_DIGIT_DECLARED
typedef unsigned long long   mp_digit;
#define MP_DIGIT_DECLARED
#endif
typedef unsigned long        mp_word __attribute__((mode(TI)));
#   define DIGIT_BIT 60
#else
/* this is the default case, 28-bit digits */

/* this is to make porting into LibTomCrypt easier :-) */
#ifndef MP_DIGIT_DECLARED
typedef unsigned int         mp_digit;
#define MP_DIGIT_DECLARED
#endif
#ifndef MP_WORD_DECLARED
typedef unsigned long long   mp_word;
#define MP_WORD_DECLARED
#endif

#   ifdef MP_31BIT
/* this is an extension that uses 31-bit digits */
#      define DIGIT_BIT 31
#   else
/* default case is 28-bit digits, defines MP_28BIT as a handy macro to test */
#      define DIGIT_BIT 28
#      define MP_28BIT
#   endif
#endif

/* otherwise the bits per digit is calculated automatically from the size of a mp_digit */
#ifndef DIGIT_BIT
#   define DIGIT_BIT (((CHAR_BIT * MP_SIZEOF_MP_DIGIT) - 1))  /* bits per digit */
typedef unsigned long mp_min_u32;
#else
typedef mp_digit mp_min_u32;
#endif

#define MP_DIGIT_BIT     DIGIT_BIT
#define MP_MASK          ((((mp_digit)1)<<((mp_digit)DIGIT_BIT))-((mp_digit)1))
#define MP_DIGIT_MAX     MP_MASK

/* equalities */
#define MP_LT        -1   /* less than */
#define MP_EQ         0   /* equal to */
#define MP_GT         1   /* greater than */

#define MP_ZPOS       0   /* positive integer */
#define MP_NEG        1   /* negative */

#define MP_OKAY       0   /* ok result */
#define MP_MEM        -2  /* out of mem */
#define MP_VAL        -3  /* invalid input */
#define MP_RANGE      MP_VAL

#define MP_YES        1   /* yes response */
#define MP_NO         0   /* no response */

/* Primality generation flags */
#define LTM_PRIME_BBS      0x0001 /* BBS style prime */
#define LTM_PRIME_SAFE     0x0002 /* Safe prime (p-1)/2 == prime */
#define LTM_PRIME_2MSB_ON  0x0008 /* force 2nd MSB to 1 */

typedef int           mp_err;

/* you'll have to tune these... */
#if defined(BUILD_tcl) || !defined(_WIN32)
MODULE_SCOPE int KARATSUBA_MUL_CUTOFF,
       KARATSUBA_SQR_CUTOFF,
       TOOM_MUL_CUTOFF,
       TOOM_SQR_CUTOFF;
#endif

/* define this to use lower memory usage routines (exptmods mostly) */
/* #define MP_LOW_MEM */

/* default precision */
#ifndef MP_PREC
#   ifndef MP_LOW_MEM
#      define MP_PREC 32        /* default digits of precision */
#   else
#      define MP_PREC 8         /* default digits of precision */
#   endif
#endif

/* size of comba arrays, should be at least 2 * 2**(BITS_PER_WORD - BITS_PER_DIGIT*2) */
#define MP_WARRAY               (1u << (((sizeof(mp_word) * CHAR_BIT) - (2 * DIGIT_BIT)) + 1))

/* the infamous mp_int structure */
#ifndef MP_INT_DECLARED
#define MP_INT_DECLARED
typedef struct mp_int mp_int;
#endif
struct mp_int {
   int used, alloc, sign;
   mp_digit *dp;
};

/* callback for mp_prime_random, should fill dst with random bytes and return how many read [upto len] */
typedef int ltm_prime_callback(unsigned char *dst, int len, void *dat);


#define USED(m)     ((m)->used)
#define DIGIT(m, k) ((m)->dp[(k)])
#define SIGN(m)     ((m)->sign)

/* error code to char* string */
const char *mp_error_to_string(int code);

/* ---> init and deinit bignum functions <--- */
/* init a bignum */
/*
int mp_init(mp_int *a);
*/

/* free a bignum */
/*
void mp_clear(mp_int *a);
*/

/* init a null terminated series of arguments */
/*
int mp_init_multi(mp_int *mp, ...);
*/

/* clear a null terminated series of arguments */
/*
void mp_clear_multi(mp_int *mp, ...);
*/

/* exchange two ints */
/*
void mp_exch(mp_int *a, mp_int *b);
*/

/* shrink ram required for a bignum */
/*
int mp_shrink(mp_int *a);
*/

/* grow an int to a given size */
/*
int mp_grow(mp_int *a, int size);
*/

/* init to a given number of digits */
/*
int mp_init_size(mp_int *a, int size);
*/

/* ---> Basic Manipulations <--- */
#define mp_iszero(a) (((a)->used == 0) ? MP_YES : MP_NO)
#define mp_iseven(a) (!mp_get_bit((a),0))
#define mp_isodd(a)  mp_get_bit((a),0)
#define mp_isneg(a)  (((a)->sign != MP_ZPOS) ? MP_YES : MP_NO)

/* set to zero */
/*
void mp_zero(mp_int *a);
*/

/* set to a digit */
/*
void mp_set(mp_int *a, mp_digit b);
*/

/* set a 32-bit const */
/*
int mp_set_int(mp_int *a, unsigned long b);
*/

/* set a platform dependent unsigned long value */
/*
int mp_set_long(mp_int *a, unsigned long b);
*/

/* set a platform dependent unsigned long long value */
/*
int mp_set_long_long(mp_int *a, unsigned long long b);
*/

/* get a 32-bit value */
/*
unsigned long mp_get_int(const mp_int *a);
*/

/* get a platform dependent unsigned long value */
/*
unsigned long mp_get_long(const mp_int *a);
*/

/* get a platform dependent unsigned long long value */
/*
unsigned long long mp_get_long_long(const mp_int *a);
*/

/* initialize and set a digit */
/*
int mp_init_set(mp_int *a, mp_digit b);
*/

/* initialize and set 32-bit value */
/*
int mp_init_set_int(mp_int *a, unsigned long b);
*/

/* copy, b = a */
/*
int mp_copy(const mp_int *a, mp_int *b);
*/

/* inits and copies, a = b */
/*
int mp_init_copy(mp_int *a, const mp_int *b);
*/

/* trim unused digits */
/*
void mp_clamp(mp_int *a);
*/

/* import binary data */
/*
int mp_import(mp_int *rop, size_t count, int order, size_t size, int endian, size_t nails, const void *op);
*/

/* export binary data */
/*
int mp_export(void *rop, size_t *countp, int order, size_t size, int endian, size_t nails, const mp_int *op);
*/

/* ---> digit manipulation <--- */

/* right shift by "b" digits */
/*
void mp_rshd(mp_int *a, int b);
*/

/* left shift by "b" digits */
/*
int mp_lshd(mp_int *a, int b);
*/

/* c = a / 2**b, implemented as c = a >> b */
/*
int mp_div_2d(const mp_int *a, int b, mp_int *c, mp_int *d);
*/

/* b = a/2 */
/*
int mp_div_2(const mp_int *a, mp_int *b);
*/

/* c = a * 2**b, implemented as c = a << b */
/*
int mp_mul_2d(const mp_int *a, int b, mp_int *c);
*/

/* b = a*2 */
/*
int mp_mul_2(const mp_int *a, mp_int *b);
*/

/* c = a mod 2**b */
/*
int mp_mod_2d(const mp_int *a, int b, mp_int *c);
*/

/* computes a = 2**b */
/*
int mp_2expt(mp_int *a, int b);
*/

/* Counts the number of lsbs which are zero before the first zero bit */
/*
int mp_cnt_lsb(const mp_int *a);
*/

/* I Love Earth! */

/* makes a pseudo-random int of a given size */
/*
int mp_rand(mp_int *a, int digits);
*/

#ifdef MP_PRNG_ENABLE_LTM_RNG
/* as last resort we will fall back to libtomcrypt's rng_get_bytes()
 * in case you don't use libtomcrypt or use it w/o rng_get_bytes()
 * you have to implement it somewhere else, as it's required */
extern unsigned long (*ltm_rng)(unsigned char *out, unsigned long outlen, void (*callback)(void));
extern void (*ltm_rng_callback)(void);
#endif

/* ---> binary operations <--- */
/* c = a XOR b  */
/*
int mp_xor(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a OR b */
/*
int mp_or(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a AND b */
/*
int mp_and(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a XOR b (two complement) */
/*
int mp_tc_xor(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a OR b (two complement) */
/*
int mp_tc_or(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a AND b (two complement) */
/*
int mp_tc_and(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* right shift (two complement) */
/*
int mp_tc_div_2d(const mp_int *a, int b, mp_int *c);
*/

/* ---> Basic arithmetic <--- */

/* b = ~a */
/*
int mp_complement(const mp_int *a, mp_int *b);
*/

/* b = -a */
/*
int mp_neg(const mp_int *a, mp_int *b);
*/

/* b = |a| */
/*
int mp_abs(const mp_int *a, mp_int *b);
*/

/* compare a to b */
/*
int mp_cmp(const mp_int *a, const mp_int *b);
*/

/* compare |a| to |b| */
/*
int mp_cmp_mag(const mp_int *a, const mp_int *b);
*/

/* c = a + b */
/*
int mp_add(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a - b */
/*
int mp_sub(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = a * b */
/*
int mp_mul(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* b = a*a  */
/*
int mp_sqr(const mp_int *a, mp_int *b);
*/

/* a/b => cb + d == a */
/*
int mp_div(const mp_int *a, const mp_int *b, mp_int *c, mp_int *d);
*/

/* c = a mod b, 0 <= c < b  */
/*
int mp_mod(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* ---> single digit functions <--- */

/* compare against a single digit */
/*
int mp_cmp_d(const mp_int *a, mp_digit b);
*/

/* c = a + b */
/*
int mp_add_d(const mp_int *a, mp_digit b, mp_int *c);
*/

/* c = a - b */
/*
int mp_sub_d(const mp_int *a, mp_digit b, mp_int *c);
*/

/* c = a * b */
/*
int mp_mul_d(const mp_int *a, mp_digit b, mp_int *c);
*/

/* a/b => cb + d == a */
/*
int mp_div_d(const mp_int *a, mp_digit b, mp_int *c, mp_digit *d);
*/

/* a/3 => 3c + d == a */
/*
int mp_div_3(const mp_int *a, mp_int *c, mp_digit *d);
*/

/* c = a**b */
/*
int mp_expt_d(const mp_int *a, mp_digit b, mp_int *c);
*/
/*
int mp_expt_d_ex(const mp_int *a, mp_digit b, mp_int *c, int fast);
*/

/* c = a mod b, 0 <= c < b  */
/*
int mp_mod_d(const mp_int *a, mp_digit b, mp_digit *c);
*/

/* ---> number theory <--- */

/* d = a + b (mod c) */
/*
int mp_addmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d);
*/

/* d = a - b (mod c) */
/*
int mp_submod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d);
*/

/* d = a * b (mod c) */
/*
int mp_mulmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d);
*/

/* c = a * a (mod b) */
/*
int mp_sqrmod(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = 1/a (mod b) */
/*
int mp_invmod(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* c = (a, b) */
/*
int mp_gcd(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* produces value such that U1*a + U2*b = U3 */
/*
int mp_exteuclid(const mp_int *a, const mp_int *b, mp_int *U1, mp_int *U2, mp_int *U3);
*/

/* c = [a, b] or (a*b)/(a, b) */
/*
int mp_lcm(const mp_int *a, const mp_int *b, mp_int *c);
*/

/* finds one of the b'th root of a, such that |c|**b <= |a|
 *
 * returns error if a < 0 and b is even
 */
/*
int mp_n_root(const mp_int *a, mp_digit b, mp_int *c);
*/
/*
int mp_n_root_ex(const mp_int *a, mp_digit b, mp_int *c, int fast);
*/

/* special sqrt algo */
/*
int mp_sqrt(const mp_int *arg, mp_int *ret);
*/

/* special sqrt (mod prime) */
/*
int mp_sqrtmod_prime(const mp_int *n, const mp_int *prime, mp_int *ret);
*/

/* is number a square? */
/*
int mp_is_square(const mp_int *arg, int *ret);
*/

/* computes the jacobi c = (a | n) (or Legendre if b is prime)  */
/*
int mp_jacobi(const mp_int *a, const mp_int *n, int *c);
*/

/* used to setup the Barrett reduction for a given modulus b */
/*
int mp_reduce_setup(mp_int *a, const mp_int *b);
*/

/* Barrett Reduction, computes a (mod b) with a precomputed value c
 *
 * Assumes that 0 < x <= m*m, note if 0 > x > -(m*m) then you can merely
 * compute the reduction as -1 * mp_reduce(mp_abs(x)) [pseudo code].
 */
/*
int mp_reduce(mp_int *x, const mp_int *m, const mp_int *mu);
*/

/* setups the montgomery reduction */
/*
int mp_montgomery_setup(const mp_int *n, mp_digit *rho);
*/

/* computes a = B**n mod b without division or multiplication useful for
 * normalizing numbers in a Montgomery system.
 */
/*
int mp_montgomery_calc_normalization(mp_int *a, const mp_int *b);
*/

/* computes x/R == x (mod N) via Montgomery Reduction */
/*
int mp_montgomery_reduce(mp_int *x, const mp_int *n, mp_digit rho);
*/

/* returns 1 if a is a valid DR modulus */
/*
int mp_dr_is_modulus(const mp_int *a);
*/

/* sets the value of "d" required for mp_dr_reduce */
/*
void mp_dr_setup(const mp_int *a, mp_digit *d);
*/

/* reduces a modulo n using the Diminished Radix method */
/*
int mp_dr_reduce(mp_int *x, const mp_int *n, mp_digit k);
*/

/* returns true if a can be reduced with mp_reduce_2k */
/*
int mp_reduce_is_2k(const mp_int *a);
*/

/* determines k value for 2k reduction */
/*
int mp_reduce_2k_setup(const mp_int *a, mp_digit *d);
*/

/* reduces a modulo b where b is of the form 2**p - k [0 <= a] */
/*
int mp_reduce_2k(mp_int *a, const mp_int *n, mp_digit d);
*/

/* returns true if a can be reduced with mp_reduce_2k_l */
/*
int mp_reduce_is_2k_l(const mp_int *a);
*/

/* determines k value for 2k reduction */
/*
int mp_reduce_2k_setup_l(const mp_int *a, mp_int *d);
*/

/* reduces a modulo b where b is of the form 2**p - k [0 <= a] */
/*
int mp_reduce_2k_l(mp_int *a, const mp_int *n, const mp_int *d);
*/

/* Y = G**X (mod P) */
/*
int mp_exptmod(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y);
*/

/* ---> Primes <--- */

/* number of primes */
#ifdef MP_8BIT
#  define PRIME_SIZE 31
#else
#  define PRIME_SIZE 256
#endif

/* table of first PRIME_SIZE primes */
#if defined(BUILD_tcl) || !defined(_WIN32)
MODULE_SCOPE const mp_digit ltm_prime_tab[PRIME_SIZE];
#endif

/* result=1 if a is divisible by one of the first PRIME_SIZE primes */
/*
int mp_prime_is_divisible(const mp_int *a, int *result);
*/

/* performs one Fermat test of "a" using base "b".
 * Sets result to 0 if composite or 1 if probable prime
 */
/*
int mp_prime_fermat(const mp_int *a, const mp_int *b, int *result);
*/

/* performs one Miller-Rabin test of "a" using base "b".
 * Sets result to 0 if composite or 1 if probable prime
 */
/*
int mp_prime_miller_rabin(const mp_int *a, const mp_int *b, int *result);
*/

/* This gives [for a given bit size] the number of trials required
 * such that Miller-Rabin gives a prob of failure lower than 2^-96
 */
/*
int mp_prime_rabin_miller_trials(int size);
*/

/* performs t rounds of Miller-Rabin on "a" using the first
 * t prime bases.  Also performs an initial sieve of trial
 * division.  Determines if "a" is prime with probability
 * of error no more than (1/4)**t.
 *
 * Sets result to 1 if probably prime, 0 otherwise
 */
/*
int mp_prime_is_prime(const mp_int *a, int t, int *result);
*/

/* finds the next prime after the number "a" using "t" trials
 * of Miller-Rabin.
 *
 * bbs_style = 1 means the prime must be congruent to 3 mod 4
 */
/*
int mp_prime_next_prime(mp_int *a, int t, int bbs_style);
*/

/* makes a truly random prime of a given size (bytes),
 * call with bbs = 1 if you want it to be congruent to 3 mod 4
 *
 * You have to supply a callback which fills in a buffer with random bytes.  "dat" is a parameter you can
 * have passed to the callback (e.g. a state or something).  This function doesn't use "dat" itself
 * so it can be NULL
 *
 * The prime generated will be larger than 2^(8*size).
 */
#define mp_prime_random(a, t, size, bbs, cb, dat) mp_prime_random_ex(a, t, ((size) * 8) + 1, (bbs==1)?LTM_PRIME_BBS:0, cb, dat)

/* makes a truly random prime of a given size (bits),
 *
 * Flags are as follows:
 *
 *   LTM_PRIME_BBS      - make prime congruent to 3 mod 4
 *   LTM_PRIME_SAFE     - make sure (p-1)/2 is prime as well (implies LTM_PRIME_BBS)
 *   LTM_PRIME_2MSB_ON  - make the 2nd highest bit one
 *
 * You have to supply a callback which fills in a buffer with random bytes.  "dat" is a parameter you can
 * have passed to the callback (e.g. a state or something).  This function doesn't use "dat" itself
 * so it can be NULL
 *
 */
/*
int mp_prime_random_ex(mp_int *a, int t, int size, int flags, ltm_prime_callback cb, void *dat);
*/

/* ---> radix conversion <--- */
/*
int mp_count_bits(const mp_int *a);
*/

/*
int mp_unsigned_bin_size(const mp_int *a);
*/
/*
int mp_read_unsigned_bin(mp_int *a, const unsigned char *b, int c);
*/
/*
int mp_to_unsigned_bin(const mp_int *a, unsigned char *b);
*/
/*
int mp_to_unsigned_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen);
*/

/*
int mp_signed_bin_size(const mp_int *a);
*/
/*
int mp_read_signed_bin(mp_int *a, const unsigned char *b, int c);
*/
/*
int mp_to_signed_bin(const mp_int *a,  unsigned char *b);
*/
/*
int mp_to_signed_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen);
*/

/*
int mp_read_radix(mp_int *a, const char *str, int radix);
*/
/*
int mp_toradix(const mp_int *a, char *str, int radix);
*/
/*
int mp_toradix_n(const mp_int *a, char *str, int radix, int maxlen);
*/
/*
int mp_radix_size(const mp_int *a, int radix, int *size);
*/

#ifndef LTM_NO_FILE
/*
int mp_fread(mp_int *a, int radix, FILE *stream);
*/
/*
int mp_fwrite(const mp_int *a, int radix, FILE *stream);
*/
#endif

#define mp_read_raw(mp, str, len) mp_read_signed_bin((mp), (str), (len))
#define mp_raw_size(mp)           mp_signed_bin_size(mp)
#define mp_toraw(mp, str)         mp_to_signed_bin((mp), (str))
#define mp_read_mag(mp, str, len) mp_read_unsigned_bin((mp), (str), (len))
#define mp_mag_size(mp)           mp_unsigned_bin_size(mp)
#define mp_tomag(mp, str)         mp_to_unsigned_bin((mp), (str))

#define mp_tobinary(M, S)  mp_toradix((M), (S), 2)
#define mp_tooctal(M, S)   mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S)     mp_toradix((M), (S), 16)

#ifdef __cplusplus
}
#endif

#endif


/* ref:         $Format:%D$ */
/* git commit:  $Format:%H$ */
/* commit time: $Format:%ai$ */

