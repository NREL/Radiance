/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Find and open a Radiance library file.
 */

#include <stdio.h>

#ifndef	DEFPATH
#define	DEFPATH		":/usr/local/lib/ray"
#endif
#ifndef ULIBVAR
#define ULIBVAR		"RAYPATH"
#endif


FILE *
fropen(fname)			/* find file and open for reading */
register char  *fname;
{
	extern char  *strcpy(), *getenv();
	static char  *searchpath;
	FILE  *fp;
	char  pname[256];
	register char  *sp, *cp;

	if (fname == NULL)
		return(NULL);

	if (fname[0] == '/' || fname[0] == '.')	/* absolute path */
		return(fopen(fname, "r"));
		
	if (searchpath == NULL) {		/* get search path */
		searchpath = getenv(ULIBVAR);
		if (searchpath == NULL)
			searchpath = DEFPATH;
	}
						/* check search path */
	sp = searchpath;
	do {
		cp = pname;
		while (*sp && (*cp = *sp++) != ':')
			cp++;
		if (cp > pname && cp[-1] != '/')
			*cp++ = '/';
		strcpy(cp, fname);
		if ((fp = fopen(pname, "r")) != NULL)
			return(fp);			/* got it! */
	} while (*sp);
						/* not found */
	return(NULL);
}
