/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Default warning output function.
 */

int	nowarn = 0;		/* don't print warnings? */

wputs(s)
char	*s;
{
	if (!nowarn)
		eputs(s);
}
