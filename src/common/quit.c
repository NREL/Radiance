#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Default program quit routine.
 */

#include "copyright.h"

void
quit(code)			/* quit program */
int  code;
{
	exit(code);
}
