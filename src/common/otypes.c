/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Object type lookup and error reporting
 */

#include  "standard.h"

#include  "object.h"

#include  "otypes.h"


int
otype(ofname)			/* get object function number from its name */
register char  *ofname;
{
	register int  i;

	for (i = 0; i < NUMOTYPE; i++)
		if (!strcmp(ofun[i].funame, ofname))
			return(i);

	return(-1);		/* not found */
}


objerror(o, etyp, msg)		/* report error related to object */
OBJREC  *o;
int  etyp;
char  *msg;
{
	char  msgbuf[128];

	sprintf(msgbuf, "%s for %s \"%s\"",
			msg, ofun[o->otype].funame, o->oname);
	error(etyp, msgbuf);
}
