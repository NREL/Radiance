/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Fix argv[0] for DOS environments
 */


char *
fixargv0(av0)			/* extract command name from full path */
char  *av0;
{
	register char  *cp = av0;

	while (*cp) cp++;		/* start from end */
	while (cp-- > av0)
		switch (*cp) {		/* fix up command name */
		case '.':			/* remove extension */
			*cp = '\0';
			continue;
		case '\\':			/* remove directory */
			return(cp+1);
		default:			/* convert to lower case */
			*cp = tolower(*cp);
			continue;
		}
	return(av0);
}


