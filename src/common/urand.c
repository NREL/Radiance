/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Uncorrelated (anticorrelated) random function
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
