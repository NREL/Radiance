#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Default program quit routine.
 */

#include "copyright.h"

#include <stdlib.h>

void
quit(code)			/* quit program */
int  code;
{
	exit(code);
}
