/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * String comparison routine optimized for use with savestr.c
 */


int
strcmp(s1, s2)				/* check for s1==s2 */
register char  *s1, *s2;
{
	if (s1 == s2)
		return(0);

	while (*s1 == *s2++)
		if (!*s1++)
			return(0);

	return(*s1 - *--s2);
}
