/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Quick and dirty string saver.
 */

#define  NULL		0

extern char  *strcpy(), *strcat(), *bmalloc();


char *
savqstr(s)			/* save a private string */
char  *s;
{
	register char  *cp;

	if ((cp = bmalloc(strlen(s)+1)) == NULL) {
		eputs("out of memory in savqstr");
		quit(1);
	}
	(void)strcpy(cp, s);
	return(cp);
}


freeqstr(s)			/* free a private string */
char  *s;
{
	bfree(s, strlen(s)+1);
}
