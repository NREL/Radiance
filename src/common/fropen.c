#ifndef lint
static const char	RCSid[] = "$Id: fropen.c,v 2.13 2003/07/17 09:21:29 schorsch Exp $";
#endif
/*
 * Find and open a Radiance library file.
 *
 *  External symbols declared in standard.h
 */

#include "copyright.h"

#include <stdio.h>

#include "rtio.h"
#include "paths.h"


FILE *
frlibopen(fname)		/* find file and open for reading */
register char  *fname;
{
	extern char  *getrlibpath();
	FILE  *fp;
	char  pname[PATH_MAX];
	register char  *sp, *cp;

	if (fname == NULL)
		return(NULL);

	if (ISDIRSEP(fname[0]) || fname[0] == '.')	/* absolute path */
		return(fopen(fname, "r"));
						/* check search path */
	sp = getrlibpath();
	do {
		cp = pname;
		while (*sp && (*cp = *sp++) != PATHSEP)
			cp++;
		if (cp > pname && !ISDIRSEP(cp[-1]))
			*cp++ = DIRSEP;
		strcpy(cp, fname);
		if ((fp = fopen(pname, "r")) != NULL)
			return(fp);			/* got it! */
	} while (*sp);
						/* not found */
	return(NULL);
}
