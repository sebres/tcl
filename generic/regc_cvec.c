/*
 * Utility functions for handling cvecs
 * This file is #included by regcomp.c.
 */

/*
 - newcvec - allocate a new cvec
 ^ static struct cvec *newcvec(int, int);
 */
static struct cvec *
newcvec(nchrs, nmcces)
int nchrs;			/* to hold this many chrs... */
int nmcces;			/* ... and this many MCCEs */
{
	size_t n;
	size_t nc;
	struct cvec *cv;

	nc = (size_t)nchrs + (size_t)nmcces*(MAXMCCE+1);
	n = sizeof(struct cvec) + (size_t)(nmcces-1)*sizeof(chr *) +
								nc*sizeof(chr);
	cv = (struct cvec *)MALLOC(n);
	if (cv == NULL)
		return NULL;
	cv->chrspace = nc;
	cv->chrs = (chr *)&cv->mcces[nmcces];	/* chrs just after MCCE ptrs */
	cv->mccespace = nmcces;
	return clearcvec(cv);
}

/*
 - clearcvec - clear a possibly-new cvec
 * Returns pointer as convenience.
 ^ static struct cvec *clearcvec(struct cvec *);
 */
static struct cvec *
clearcvec(cv)
struct cvec *cv;
{
	int i;

	assert(cv != NULL);
	cv->nchrs = 0;
	assert(cv->chrs == (chr *)&cv->mcces[cv->mccespace]);
	cv->nmcces = 0;
	cv->nmccechrs = 0;
	for (i = 0; i < cv->mccespace; i++)
		cv->mcces[i] = NULL;

	return cv;
}

/*
 - addchr - add a chr to a cvec
 ^ static VOID addchr(struct cvec *, pchr);
 */
static VOID
addchr(cv, c)
struct cvec *cv;
pchr c;
{
	assert(cv->nchrs < cv->chrspace - cv->nmccechrs);
	cv->chrs[cv->nchrs++] = (chr)c;
}

/*
 - addmcce - add an MCCE to a cvec
 ^ static VOID addmcce(struct cvec *, chr *, chr *);
 */
static VOID
addmcce(cv, startp, endp)
struct cvec *cv;
chr *startp;			/* beginning of text */
chr *endp;			/* just past end of text */
{
	int n = endp - startp;
	int i;
	chr *s;
	chr *d;

	assert(n > 0);
	assert(cv->nchrs + n < cv->chrspace - cv->nmccechrs);
	assert(cv->nmcces < cv->mccespace);
	d = &cv->chrs[cv->chrspace - cv->nmccechrs - n - 1];
	cv->mcces[cv->nmcces++] = d;
	for (s = startp, i = n; i > 0; s++, i--)
		*d++ = *s;
	*d++ = 0;		/* endmarker */
	assert(d == &cv->chrs[cv->chrspace - cv->nmccechrs]);
	cv->nmccechrs += n + 1;
}

/*
 - haschr - does a cvec contain this chr?
 ^ static int haschr(struct cvec *, pchr);
 */
static int			/* predicate */
haschr(cv, c)
struct cvec *cv;
pchr c;
{
	int i;
	chr *p;

	for (p = cv->chrs, i = cv->nchrs; i > 0; p++, i--)
		if (*p == c)
			return 1;
	return 0;
}

/*
 - getcvec - get a cvec, remembering it as v->cv
 ^ static struct cvec *getcvec(struct vars *, int, int);
 */
static struct cvec *
getcvec(v, nchrs, nmcces)
struct vars *v;
int nchrs;			/* to hold this many chrs... */
int nmcces;			/* ... and this many MCCEs */
{
	if (v->cv != NULL && nchrs <= v->cv->chrspace &&
						nmcces <= v->cv->mccespace)
		return clearcvec(v->cv);

	if (v->cv != NULL)
		freecvec(v->cv);
	v->cv = newcvec(nchrs, nmcces);
	if (v->cv == NULL)
		ERR(REG_ESPACE);

	return v->cv;
}

/*
 - freecvec - free a cvec
 ^ static VOID freecvec(struct cvec *);
 */
static VOID
freecvec(cv)
struct cvec *cv;
{
	FREE(cv);
}
