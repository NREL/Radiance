/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * fgetline.c - read line with escaped newlines
 *
 *	10/4/89
 */

#include  <stdio.h>


char *
fgetline(s, n, fp)	/* read in line with escapes, elide final newline */
char  *s;
register int  n;
FILE  *fp;
{
	register char  *cp;
	register int  c;

	cp = s;
	while (--n > 0 && (c = getc(fp)) != EOF && c != '\n') {
		if (c == '\\') {
			if ((c = getc(fp)) == EOF)
				break;
		}
		*cp++ = c;
	}
	if (cp == s)
		return(NULL);
	*cp++ = '\0';
	return(s);
}
