/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Anticorrelated random function due to Christophe Schlick
 */

#include  "random.h"

#define  NULL		0

extern char  *malloc();

short  *urperm;		/* urand() permutation */
int  urmask;		/* bits used in permutation */


initurand(size)		/* initialize urand() for size entries */
int  size;
{
	int  order, n;
	register int  i, offset;

	size--;
	for (i = 1; size >>= 1; i++)
		;
	order = i;
	urmask = (1<<i) - 1;
	urperm = (short *)malloc((urmask+1)*sizeof(short));
	if (urperm == NULL) {
		eputs("out of memory in initurand\n");
		quit(1);
	}
	urperm[0] = 0;
	for (n = 1, offset = 1; n <= order; n++, offset <<= 1)
		for (i = offset; i--; ) {
			urperm[i] =
			urperm[i+offset] = 2*urperm[i];
			if (random() & 0x4000)
				urperm[i]++;
			else
				urperm[i+offset]++;
		}
}


int
ilhash(d, n)			/* hash a set of integer values */
register int  *d;
register int  n;
{
	static int  tab[8] = {13623,353,1637,5831,2314,3887,5832,8737};
	register int  hval;

	hval = 0;
	while (n-- > 0)
		hval += *d++ * tab[n&7];
	return(hval);
}


static char  bctab[256] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
	};


#define NBITS		32

#if NBITS==32
#define  bitcount(i)	(bctab[(i)>>24&0xff]+bctab[(i)>>16&0xff]+ \
				bctab[(i)>>8&0xff]+bctab[(i)&0xff])
#endif
#if  NBITS==16
#define  bitcount(i)	(bctab[(i)>>8&0xff]+bctab[(i)&0xff])
#endif


int
urind(s, i)			/* compute i'th index from seed s */
int	s, i;
{
	register int  ss, k;
	int  left;

	ss = s*1103515245 + 12345;
	left = 0;
	for (k = i/NBITS; k--; ) {
		left += bitcount(ss);
		ss = ss*1103515245 + 12345;
	}
	for (k = i&(NBITS-1); k--; ss >>= 1)
		left += ss & 1;
	if (ss & 1)
		return(s-left-1);
	return(s-left+i);
}
