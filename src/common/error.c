/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  error.c - standard error reporting function
 */

#include  "standard.h"

#ifndef linux
extern char  *sys_errlist[];	/* system error list */
extern int  sys_nerr;		/* number of system errors */
#endif
				/* global list of error actions */
struct erract	erract[NERRS] = ERRACT_INIT;

char  errmsg[512];		/* global error message buffer */


error(etype, emsg)		/* report error, quit if necessary */
int  etype;
char  *emsg;
{
	register struct erract	*ep;

	if (etype < 0 | etype >= NERRS)
		return;
	ep = erract + etype;
	if (ep->pf != NULL) {
		(*ep->pf)(ep->pre);
		(*ep->pf)(emsg);
		if (etype == SYSTEM && errno > 0) {
			(*ep->pf)(": ");
			if (errno <= sys_nerr)
				(*ep->pf)(sys_errlist[errno]);
			else
				(*ep->pf)("Unknown error");
		}
		(*ep->pf)("\n");
	}
	if (!ep->ec)		/* non-fatal */
		return;
	if (ep->ec < 0)		/* dump core */
		abort();
	quit(ep->ec);		/* quit calls exit after cleanup */
}
