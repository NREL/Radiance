/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for recognizing and moving about words in strings.
 */

#include  <ctype.h>

#ifdef  BSD
#define  strchr		index
#endif

#define  NULL		0

extern char  *strchr();


char *
sskip(s)			/* skip word in string */
register char  *s;
{
	while (isspace(*s))
		s++;
	while (*s && !isspace(*s))
		s++;
	return(s);
}


char *
iskip(s)			/* skip integer in string */
char  *s;
{
	register char  *cp = s;

	while (isspace(*cp))
		cp++;
	if (*cp == '-' || *cp == '+')
		cp++;
	while (isdigit(*cp))
		cp++;
	return(cp);
}


char *
fskip(s)			/* skip float in string */
char  *s;
{
	register char  *cp = s;

	while (isspace(*cp))
		cp++;
	if (*cp == '-' || *cp == '+')
		cp++;
	while (isdigit(*cp))
		cp++;
	if (*cp == '.') {
		cp++;
		while (isdigit(*cp))
			cp++;
	}
	if (*cp == 'e' || *cp == 'E')
		return(iskip(cp+1));
	return(cp);
}


isint(s)			/* check integer format */
char  *s;
{
	register char  *cp;

	cp = iskip(s);
	return(cp > s && *cp == '\0');
}


isintd(s, ds)			/* check integer format with delimiter set */
char  *s, *ds;
{
	register char  *cp;

	cp = iskip(s);
	return(cp > s && strchr(*cp, ds) != NULL);
}


isflt(s)			/* check float format */
char  *s;
{
	register char  *cp;

	cp = fskip(s);
	return(cp > s && *cp == '\0');
}


isfltd(s, ds)			/* check integer format with delimiter set */
char  *s, *ds;
{
	register char  *cp;

	cp = fskip(s);
	return(cp > s && strchr(*cp, ds) != NULL);
}
