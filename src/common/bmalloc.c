/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Bmalloc provides basic memory allocation without overhead (no free lists).
 * Use only to take the load off of malloc for all those
 * piddling little requests that you never expect to free.
 * Bmalloc defers to malloc for big requests.
 * Bfree should hand memory to bmalloc, but it usually fails here.
 */

#define  NULL		0

#ifndef  MBLKSIZ
#define  MBLKSIZ	16376		/* size of memory allocation block */
#endif
#define  WASTEFRAC	12		/* don't waste more than a fraction */
#ifndef  ALIGN
#define  ALIGN		int		/* type for alignment */
#endif
#define  BYTES_WORD	sizeof(ALIGN)

extern char  *malloc();

static char  *bposition = NULL;
static unsigned  nremain = 0;


char *
bmalloc(n)		/* allocate a block of n bytes, no refunds */
register unsigned  n;
{
	if (n > nremain && (n > MBLKSIZ || nremain > MBLKSIZ/WASTEFRAC))
		return(malloc(n));			/* too big */

	n = (n+(BYTES_WORD-1))&~(BYTES_WORD-1);		/* word align */

	if (n > nremain) {
		if ((bposition = malloc((unsigned)MBLKSIZ)) == NULL) {
			nremain = 0;
			return(NULL);
		}
		nremain = MBLKSIZ;
	}
	bposition += n;
	nremain -= n;
	return(bposition - n);
}


bfree(p, n)			/* free random memory */
register char	*p;
register unsigned	n;
{
	register unsigned	bsiz;
					/* check alignment */
	bsiz = BYTES_WORD - ((unsigned)p&(BYTES_WORD-1));
	if (bsiz < BYTES_WORD) {
		p += bsiz;
		n -= bsiz;
	}
	if (p + n == bposition) {	/* just allocated? */
		bposition = p;
		nremain += n;
		return;
	}
	if (n > nremain) {		/* better than what we've got? */
		bposition = p;
		nremain = n;
		return;
	}
				/* just throw it away, then */
}
