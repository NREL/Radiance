/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

#include <stdio.h>

eputs(s)			/* error message */
char  *s;
{
	fputs(s, stderr);
}
