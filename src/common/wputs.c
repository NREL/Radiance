#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Default warning output function.
 */

#include "copyright.h"

int	nowarn = 0;		/* don't print warnings? */

void
wputs(s)
char	*s;
{
	if (!nowarn)
		eputs(s);
}
