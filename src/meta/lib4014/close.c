#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
#ifndef lint
static char sccsid[] = "@(#)close.c	4.1 (Berkeley) 6/27/83";
#endif

#include <stdio.h>
closevt(){
	putch(037);
	putch(030);
	fflush(stdout);
}
closepl(){
	putch(037);
	putch(030);
	fflush(stdout);
}
