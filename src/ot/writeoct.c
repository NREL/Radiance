/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  writeoct.c - routines for writing octree information to stdout.
 *
 *     7/30/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "otypes.h"


writeoct(store, scene, ofn)		/* write octree structures to stdout */
int  store;
CUBE  *scene;
char  *ofn[];
{
	char  sbuf[64];
	register int  i;
					/* write format number */
	putint((long)OCTMAGIC, 2);

	if (!(store & IO_BOUNDS))
		return;
					/* write boundaries */
	for (i = 0; i < 3; i++) {
		sprintf(sbuf, "%.12g", scene->cuorg[i]);
		putstr(sbuf);
	}
	sprintf(sbuf, "%.12g", scene->cusize);
	putstr(sbuf);
					/* write object file names */
	if (store & IO_FILES)
		for (i = 0; ofn[i] != NULL; i++)
			putstr(ofn[i]);
	putstr("");

	if (!(store & IO_TREE))
		return;
					/* write the octree */
	puttree(scene->cutree);

	if (store & IO_FILES || !(store & IO_SCENE))
		return;
					/* write the scene */
	for (i = 0; i < NUMOTYPE; i++)
		putstr(ofun[i].funame);
	putstr("");
	for (i = 0; i < nobjects; i++)
		putobj(objptr(i));
	putobj(NULL);
}


static
putstr(s)			/* write null-terminated string to stdout */
register char  *s;
{
	do
		putc(*s, stdout);
	while (*s++);
	if (ferror(stdout))
		error(SYSTEM, "write error in putstr");
}


static
putfullnode(fn)			/* write out a full node */
OCTREE  fn;
{
	OBJECT  oset[MAXSET+1];
	register int  i;

	objset(oset, fn);
	for (i = 0; i <= oset[0]; i++)
		putint((long)oset[i], sizeof(OBJECT));
}


static
putint(i, siz)			/* write a siz-byte integer to stdout */
register long  i;
register int  siz;
{
	while (siz--)
		putc(i>>(siz<<3) & 0377, stdout);
	if (ferror(stdout))
		error(SYSTEM, "write error in putint");
}


static
putflt(f)			/* put out floating point number */
double  f;
{
	extern double  frexp();
	int  e;

	putint((long)(frexp(f,&e)*0x7fffffff), sizeof(long));
	putint(e, 1);
}


static
puttree(ot)			/* write octree to stdout in pre-order form */
register OCTREE  ot;
{
	register int  i;
	
	if (istree(ot)) {
		putc(OT_TREE, stdout);		/* indicate tree */
		for (i = 0; i < 8; i++)		/* write tree */
			puttree(octkid(ot, i));
	} else if (isfull(ot)) {
		putc(OT_FULL, stdout);		/* indicate fullnode */
		putfullnode(ot);		/* write fullnode */
	} else
		putc(OT_EMPTY, stdout);		/* indicate empty */
}


static
putobj(o)			/* write out object */
register OBJREC  *o;
{
	register int  i;

	if (o == NULL) {		/* terminator */
		putint(-1L, 1);
		return;
	}
	putint((long)o->otype, 1);
	putint((long)o->omod, sizeof(OBJECT));
	putstr(o->oname);
	putint((long)o->oargs.nsargs, 2);
	for (i = 0; i < o->oargs.nsargs; i++)
		putstr(o->oargs.sarg[i]);
#ifdef  IARGS
	putint(o->oargs.niargs, 2);
	for (i = 0; i < o->oargs.niargs; i++)
		putint(o->oargs.iarg[i], sizeof(long));
#endif
	putint((long)o->oargs.nfargs, 2);
	for (i = 0; i < o->oargs.nfargs; i++)
		putflt(o->oargs.farg[i]);
}
