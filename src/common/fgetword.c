/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Read white space separated words from stream
 */

#include  <stdio.h>

#include  <ctype.h>


char *
fgetword(s, n, fp)		/* get a word, maximum n-1 characters */
char  *s;
int  n;
register FILE  *fp;
{
	register char  *cp;
	register int  c;
					/* skip initial white space */
	do
		c = getc(fp);
	while (isspace(c));
					/* check for end of file */
	if (c == EOF)
		return(NULL);
					/* get actual word */
	cp = s;
	do {
		if (--n <= 0)			/* check length limit */
			break;
		*cp++ = c;
		c = getc(fp);
	} while (c != EOF && !isspace(c));
	*cp = '\0';
	if (c != EOF)			/* replace delimiter */
		ungetc(c, fp);
	return(s);
}


isint(s)			/* check integer format */
char  *s;
{
	register char  *cp = s;

	while (isspace(*cp))
		cp++;
	if (*cp == '-' || *cp == '+')
		cp++;
	do
		if (!isdigit(*cp))
			return(0);
	while (*++cp);
	return(1);
}


isflt(s)			/* check float format */
char  *s;
{
	register char  *cp = s;

	while (isspace(*cp))
		cp++;
	if (*cp == '-' || *cp == '+')
		cp++;
	s = cp;
	while (isdigit(*cp))
		cp++;
	if (*cp == '.') {
		cp++; s++;
		while (isdigit(*cp))
			cp++;
	}
	if (cp == s)
		return(0);
	if (*cp == 'e' || *cp == 'E')
		return(isint(cp+1));
	return(1);
}
