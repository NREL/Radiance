/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Check argument list against format string.
 */

#define NULL		0

int
badarg(ac, av, fl)		/* check argument list */
int	ac;
register char	**av;
register char	*fl;
{
	register int  i;

	if (fl == NULL)
		fl = "";		/* no arguments? */
	for (i = 1; *fl; i++,av++,fl++) {
		if (i > ac || *av == NULL)
			return(-1);
		switch (*fl) {
		case 's':		/* string */
			if (**av == '\0' || **av == ' ' || **av == '\t')
				return(i);
			break;
		case 'i':		/* integer */
			if (!isintd(*av, " \t"))
				return(i);
			break;
		case 'f':		/* float */
			if (!isfltd(*av, " \t"))
				return(i);
			break;
		default:		/* bad call! */
			return(-1);
		}
	}
	return(0);		/* all's well */
}
