/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  getpath.c - function to search for file in a list of directories
 */

#define  NULL		0

#ifndef  NIX
#include  <pwd.h>
#endif

#ifndef DIRSEP
#define DIRSEP		'/'
#endif
#ifndef PATHSEP
#define PATHSEP		':'
#endif

extern char  *strcpy(), *strcat(), *getenv();
extern struct passwd  *getpwnam();


char *
getpath(fname, searchpath, mode)	/* expand fname, return full path */
register char  *fname;
register char  *searchpath;
int  mode;
{
#ifndef  NIX
	struct passwd  *pwent;
#endif
	static char  pname[256];
	register char  *cp;

	if (fname == NULL)
		return(NULL);

	switch (*fname) {
	case DIRSEP:				/* relative to root */
	case '.':				/* relative to cwd */
		strcpy(pname, fname);
		return(pname);
#ifndef NIX
	case '~':				/* relative to home directory */
		fname++;
		if (*fname == '\0' || *fname == DIRSEP) {	/* ours */
			if ((cp = getenv("HOME")) == NULL)
				return(NULL);
			strcpy(pname, cp);
			strcat(pname, fname);
			return(pname);
		}
		cp = pname;					/* user */
		do
			*cp++ = *fname++;
		while (*fname && *fname != DIRSEP);
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
		if (cp > pname && cp[-1] != DIRSEP)
			*cp++ = DIRSEP;
		strcpy(cp, fname);
		if (access(pname, mode) == 0)		/* file accessable? */
			return(pname);
	} while (*searchpath);
							/* not found */
	return(NULL);
}
