/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * fgetline.c - read line with escaped newlines.
 *
 *	10/4/89
 */

#include  <stdio.h>


char *
fgetline(s, n, fp)	/* read in line with escapes, elide final newline */
char  *s;
int  n;
register FILE  *fp;
{
	int  escape = 0;
	register char  *cp = s;
	register int  c = EOF;

	while (--n > 0 && (c = getc(fp)) != EOF) {
		if (c == '\n' && (cp == s || cp[-1] != '\\'))
			break;
		*cp++ = c;
	}
	if (cp == s && c == EOF)
		return(NULL);
	*cp = '\0';
	return(s);
}
