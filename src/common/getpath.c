/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  getpath.c - function to search for file in a list of directories
 */

#include  "standard.h"

#include  "paths.h"

#ifndef	 NIX
#include  <pwd.h>
extern struct passwd  *getpwnam();
#endif


char *
getpath(fname, searchpath, mode)	/* expand fname, return full path */
register char  *fname;
register char  *searchpath;
int  mode;
{
#ifndef	 NIX
	struct passwd  *pwent;
#endif
	static char  pname[MAXPATH];
	register char  *cp;

	if (fname == NULL)
		return(NULL);

	switch (*fname) {
	CASEDIRSEP:				/* relative to root */
	case '.':				/* relative to cwd */
		strcpy(pname, fname);
		return(pname);
#ifndef NIX
	case '~':				/* relative to home directory */
		fname++;
		if (*fname == '\0' || ISDIRSEP(*fname)) {	/* ours */
			if ((cp = getenv("HOME")) == NULL)
				return(NULL);
			strcpy(pname, cp);
			strcat(pname, fname);
			return(pname);
		}
		cp = pname;					/* user */
		do
			*cp++ = *fname++;
		while (*fname && !ISDIRSEP(*fname));
		*cp = '\0';
		if ((pwent = getpwnam(pname)) == NULL)
			return(NULL);
		strcpy(pname, pwent->pw_dir);
		strcat(pname, fname);
		return(pname);
#endif
	}

	if (searchpath == NULL) {			/* don't search */
		strcpy(pname, fname);
		return(pname);
	}
							/* check search path */
	do {
		cp = pname;
		while (*searchpath && (*cp = *searchpath++) != PATHSEP)
			cp++;
		if (cp > pname && !ISDIRSEP(cp[-1]))
			*cp++ = DIRSEP;
		strcpy(cp, fname);
		if (access(pname, mode) == 0)		/* file accessable? */
			return(pname);
	} while (*searchpath);
							/* not found */
	return(NULL);
}
