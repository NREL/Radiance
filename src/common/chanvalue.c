#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Dummy definition of chanvalue() for calcomp routines.
 */

#include "rterror.h"
#include "calcomp.h"

double
chanvalue(int n)
{
	eputs("Call to unsupported chanvalue routine\n");
	quit(1);
	return 0.0; /* pro forma return */
}
