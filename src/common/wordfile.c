/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Load whitespace separated words from a file into an array.
 * Assume the passed pointer array is big enough to hold them all.
 */

#include <ctype.h>

#define NULL		0

#define MAXFLEN		10240		/* file must be smaller than this */

extern char	*bmalloc();


int
wordfile(words, fname)		/* get words from fname, put in words */
char	**words;
char	*fname;
{
	char	**wp = words;
	int	fd;
	char	*buf;
	register int	n;
	register char	*cp;
					/* allocate memory */
	if ((cp = buf = bmalloc(MAXFLEN)) == NULL)
		return(-1);
	if ((fd = open(fname, 0)) < 0)
		goto err;
	n = read(fd, buf, MAXFLEN);
	close(fd);
	if (n < 0)
		goto err;
	if (n == MAXFLEN)		/* file too big, take what we can */
		while (!isspace(buf[--n]))
			if (n <= 0)
				goto err;
	bfree(buf+n+1, MAXFLEN-n-1);	/* return unneeded memory */
	while (n > 0) {			/* break buffer into words */
		while (isspace(*cp)) {
			cp++;
			if (--n <= 0)
				return(wp - words);
		}
		*wp++ = cp;
		while (!isspace(*cp)) {
			cp++;
			if (--n <= 0)
				break;
		}
		*cp++ = '\0'; n--;
	}
	*wp = NULL;			/* null terminator */
	return(wp - words);
err:
	bfree(buf, MAXFLEN);
	return(-1);
}
