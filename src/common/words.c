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
atos(rs, nb, s)			/* get next word from string */
char  *rs;
register int  nb;
register char  *s;
{
	register char  *cp = rs;

	while (isspace(*s))
		s++;
	while (--nb > 0 && *s && !isspace(*s))
		*cp++ = *s++;
	*cp = '\0';
	return(rs);
}


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
register char  *s;
{
	while (isspace(*s))
		s++;
	if (*s == '-' || *s == '+')
		s++;
	if (!isdigit(*s))
		return(NULL);
	do
		s++;
	while (isdigit(*s));
	return(s);
}


char *
fskip(s)			/* skip float in string */
register char  *s;
{
	register char  *cp;

	while (isspace(*s))
		s++;
	if (*s == '-' || *s == '+')
		s++;
	cp = s;
	while (isdigit(*cp))
		cp++;
	if (*cp == '.') {
		cp++; s++;
		while (isdigit(*cp))
			cp++;
	}
	if (cp == s)
		return(NULL);
	if (*cp == 'e' || *cp == 'E')
		return(iskip(cp+1));
	return(cp);
}


isint(s)			/* check integer format */
char  *s;
{
	register char  *cp;

	cp = iskip(s);
	return(cp != NULL && *cp == '\0');
}


isintd(s, ds)			/* check integer format with delimiter set */
char  *s, *ds;
{
	register char  *cp;

	cp = iskip(s);
	return(cp != NULL && strchr(ds, *cp) != NULL);
}


isflt(s)			/* check float format */
char  *s;
{
	register char  *cp;

	cp = fskip(s);
	return(cp != NULL && *cp == '\0');
}


isfltd(s, ds)			/* check integer format with delimiter set */
char  *s, *ds;
{
	register char  *cp;

	cp = fskip(s);
	return(cp != NULL && strchr(ds, *cp) != NULL);
}
