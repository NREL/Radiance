/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Fast malloc for memory hogs and VM environments.
 * Performs a minimum of searching through free lists.
 * bmalloc() doesn't keep track of free lists -- it's basically
 *	just a buffered call to sbrk().
 * bfree() puts memory from any source into malloc() free lists.
 * malloc(), realloc() and free() use buckets
 *	containing blocks in powers of two, similar to CIT routines.
 *
 *	Greg Ward	Lawrence Berkeley Laboratory
 */

#define  NULL		0

#ifndef ALIGN
#define  ALIGN		int			/* align type */
#endif
#define  BYTES_WORD	sizeof(ALIGN)

#ifdef  NOVMEM
#define  getpagesize()	BYTES_WORD
#endif

					/* malloc free lists */
typedef union m_head {
	union m_head	*next;
	int		bucket;
	ALIGN		dummy;
} M_HEAD;

#define FIRSTBUCKET	3
#define NBUCKETS	30

static M_HEAD	*free_list[NBUCKETS];


char *
bmalloc(n)		/* allocate a block of n bytes, sans header */
register unsigned  n;
{
	extern char  *sbrk();
	static int  pagesz = 0;
	static int  amnt;
	static char  *bpos;
	static int  nrem;
	register char	*p;

	if (pagesz == 0) {				/* initialize */
		amnt = pagesz = getpagesize();
		nrem = (int)sbrk(0);			/* page align break */
		nrem = pagesz - (nrem&(pagesz-1));
		bpos = sbrk(nrem);			/* word aligned! */
		if ((int)bpos == -1)
			return(NULL);
	}

	n = (n+(BYTES_WORD-1))&~(BYTES_WORD-1);		/* word align rqst. */

	if (n > nrem) {					/* need more core */
		if (n > amnt)			/* increase amount */
			amnt = (n+(pagesz-1))&~(pagesz-1);
		p = sbrk(amnt);
		if ((int)p == -1)
			return(NULL);
		if (p != bpos+nrem) {		/* someone called sbrk()! */
			bfree(bpos, nrem);
			bpos = p;
			nrem = amnt;
		} else
			nrem += amnt;
	}
	p = bpos;
	bpos += n;					/* advance */
	nrem -= n;
	return(p);
}


bfree(p, n)			/* free n bytes of random memory */
char  *p;
unsigned  n;
{
	register M_HEAD	*mp;
	register int	bucket;
	register unsigned	bsiz;
					/* find largest bucket */
	bucket = 0;
	for (bsiz = 1; bsiz+sizeof(M_HEAD) <= n; bsiz <<= 1)
		bucket++;
	mp = (M_HEAD *)p;
	while (bucket > FIRSTBUCKET) {
		bsiz >>= 1;
		bucket--;
		if (n < bsiz+sizeof(M_HEAD))	/* nothing for this bucket */
			continue;
		mp->next = free_list[bucket];
		free_list[bucket] = mp;
		(char *)mp += bsiz+sizeof(M_HEAD);
		n -= bsiz+sizeof(M_HEAD);
	}
}


char *
malloc(n)			/* allocate n bytes of memory */
unsigned	n;
{
	register M_HEAD	*mp;
	register int	bucket;
	register unsigned	bsiz;

	bucket = FIRSTBUCKET;
	for (bsiz = 1<<FIRSTBUCKET; bsiz < n; bsiz <<= 1)
		bucket++;
	if (free_list[bucket] == NULL) {
		mp = (M_HEAD *)bmalloc(bsiz+sizeof(M_HEAD));
		if (mp == NULL)
			return(NULL);
	} else {
		mp = free_list[bucket];
		free_list[bucket] = mp->next;
	}
	mp->bucket = bucket;
	return((char *)(mp+1));
}


char *
realloc(op, n)			/* reallocate memory using malloc() */
char	*op;
unsigned	n;
{
	register char	*p;
	register unsigned	on;

	if (op != NULL)
		on = 1 << ((M_HEAD *)op-1)->bucket;
	else
		on = 0;
	if (n <= on && n > on>>1)
		return(op);		/* same bucket */
	p = malloc(n);
	if (p == NULL)
		return(NULL);
#ifdef  BSD
	bcopy(op, p, n>on ? on : n);
#else
	(void)memcpy(p, op, n>on ? on : n);
#endif
	free(op);
	return(p);
}


free(p)			/* free memory allocated by malloc */
char	*p;
{
	register M_HEAD	*mp;
	register int	bucket;

	mp = (M_HEAD *)p - 1;
	bucket = mp->bucket;
	mp->next = free_list[bucket];
	free_list[bucket] = mp;
}


#ifndef NOVMEM
#ifndef BSD
#include <sys/var.h>
int
getpagesize()			/* use SYSV var structure to get page size */
{
	static int  pagesz = 0;
	struct var  v;

	if (pagesz == 0) {
		uvar(&v);
		pagesz = 1 << v.v_pageshift;
	}
	return(pagesz);
}
#endif
#endif
