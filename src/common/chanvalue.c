#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Dummy definition of chanvalue() for calcomp routines.
 */

double
chanvalue(int n)
{
	eputs("Call to unsupported chanvalue routine\n");
	quit(1);
}
