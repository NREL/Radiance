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
#ifndef DIRSEP
#define DIRSEP		'/'
#endif
#ifndef PATHSEP
#define PATHSEP		':'
#endif

char  *libpath = NULL;		/* library search path */


FILE *
fropen(fname)			/* find file and open for reading */
register char  *fname;
{
	extern char  *strcpy(), *getenv();
	FILE  *fp;
	char  pname[256];
	register char  *sp, *cp;

	if (fname == NULL)
		return(NULL);

	if (fname[0] == DIRSEP || fname[0] == '.')	/* absolute path */
		return(fopen(fname, "r"));
		
	if (libpath == NULL) {			/* get search path */
		libpath = getenv(ULIBVAR);
		if (libpath == NULL)
			libpath = DEFPATH;
	}
						/* check search path */
	sp = libpath;
	do {
		cp = pname;
		while (*sp && (*cp = *sp++) != PATHSEP)
			cp++;
		if (cp > pname && cp[-1] != DIRSEP)
			*cp++ = DIRSEP;
		strcpy(cp, fname);
		if ((fp = fopen(pname, "r")) != NULL)
			return(fp);			/* got it! */
	} while (*sp);
						/* not found */
	return(NULL);
}
