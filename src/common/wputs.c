#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Default warning output function.
 */

#include "copyright.h"

#include "standard.h"

int	nowarn = 0;		/* don't print warnings? */

void
wputs(char *s)
{
	if (!nowarn)
		eputs(s);
}
