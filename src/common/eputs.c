#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Default error output function.
 */

#include "copyright.h"

#include <stdio.h>

void
eputs(s)			/* error message */
char  *s;
{
	fputs(s, stderr);
}
