/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * bcopy.c - substitutes for library routines.
 *
 *	5/24/88
 */


bcopy(src, dest, nbytes)
register char	*src, *dest;
register int	nbytes;
{
	while (nbytes-- > 0)
		*dest++ = *src++;
}


bzero(b, nbytes)
register char	*b;
register int	nbytes;
{
	while (nbytes-- > 0)
		*b++ = 0;
}


int
bcmp(b1, b2, nbytes)
register unsigned char	*b1, *b2;
register int	nbytes;
{
	while (nbytes-- > 0)
		if (*b1++ - *b2++)
			return(*--b1 - *--b2);
	return(0);
}
