#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * String comparison routine optimized for use with savestr.c
 */

#include "copyright.h"


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
