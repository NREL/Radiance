/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Basic memory allocation w/o overhead.
 * Calls malloc(3) for big requests.
 *
 */

#define  NULL		0

#ifndef  MBLKSIZ
#define  MBLKSIZ	16376		/* size of memory allocation block */
#endif
#define  WASTEFRAC	4		/* don't waste more than this */
#ifndef  ALIGN
#define  ALIGN		int		/* type for alignment */
#endif
#define  BYTES_WORD	sizeof(ALIGN)


char *
bmalloc(n)		/* allocate a block of n bytes, no refunds */
register unsigned  n;
{
	extern char  *malloc();
	static char  *bpos = NULL;
	static unsigned  nrem = 0;

	if (n > nrem && (n > MBLKSIZ || nrem > MBLKSIZ/WASTEFRAC))
		return(malloc(n));			/* too big */

	n = (n+(BYTES_WORD-1))&~(BYTES_WORD-1);		/* word align */

	if (n > nrem) {
		if ((bpos = malloc((unsigned)MBLKSIZ)) == NULL) {
			nrem = 0;
			return(NULL);
		}
		nrem = MBLKSIZ;
	}
	bpos += n;
	nrem -= n;
	return(bpos - n);
}


bfree(p, n)			/* free random memory */
char	*p;
unsigned	n;
{
	/* not implemented */
}
