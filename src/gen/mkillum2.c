/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines to do the actual calcultion and output for mkillum
 */

#include  "mkillum.h"

#include  "face.h"

#include  "cone.h"


printobj(mod, obj)		/* print out an object */
char  *mod;
register OBJREC  *obj;
{
	register int  i;

	printf("\n%s %s %s", mod, ofun[obj->otype].funame, obj->oname);
	printf("\n%d", obj->oargs.nsargs);
	for (i = 0; i < obj->oargs.nsargs; i++)
		printf(" %s", obj->oargs.sarg[i]);
#ifdef  IARGS
	printf("\n%d", obj->oargs.niargs);
	for (i = 0; i < obj->oargs.niargs; i++)
		printf(" %d", obj->oargs.iarg[i]);
#else
	printf("\n0");
#endif
	printf("\n%d", obj->oargs.nfargs);
	for (i = 0; i < obj->oargs.nfargs; i++) {
		if (i%3 == 0)
			putchar('\n');
		printf(" %18.12g", obj->oargs.farg[i]);
	}
	putchar('\n');
}


mkillum(ob, il, rt)		/* make an illum object */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
{
}
