/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  instance.c - routines for octree objects.
 *
 *	11/10/88
 */

#include  "standard.h"

#include  "object.h"

#include  "instance.h"

static SCENE  *slist = NULL;		/* list of loaded octrees */


SCENE *
getscene(sname, flags)			/* load octree sname */
char  *sname;
int  flags;
{
	extern char  *libpath;
	char  *pathname;
	register SCENE  *sc;

	flags &= ~(IO_FILES|IO_INFO);		/* not allowed */
	for (sc = slist; sc != NULL; sc = sc->next)
		if (!strcmp(sname, sc->name)) {
			if ((sc->ldflags & flags) == flags)
				return(sc);		/* loaded */
			break;			/* load the rest */
		}
	if (sc == NULL) {
		sc = (SCENE *)malloc(sizeof(SCENE));
		if (sc == NULL)
			error(SYSTEM, "out of memory in getscene");
		sc->name = savestr(sname);
		sc->ldflags = 0;
		sc->next = slist;
		slist = sc;
	}
	if ((pathname = getpath(sname, libpath, R_OK)) == NULL) {
		sprintf(errmsg, "cannot find octree file \"%s\"", sname);
		error(USER, errmsg);
	}
	readoct(pathname, flags & ~sc->ldflags, &sc->scube, NULL);
	sc->ldflags |= flags;
	return(sc);
}


INSTANCE *
getinstance(o, flags)			/* get instance structure */
register OBJREC  *o;
int  flags;
{
	register INSTANCE  *in;

	if ((in = (INSTANCE *)o->os) == NULL) {
		if ((in = (INSTANCE *)malloc(sizeof(INSTANCE))) == NULL)
			error(SYSTEM, "out of memory in getinstance");
		if (o->oargs.nsargs < 1)
			objerror(o, USER, "bad # of arguments");
		if (fullxf(&in->x, o->oargs.nsargs-1,
				o->oargs.sarg+1) != o->oargs.nsargs-1)
			objerror(o, USER, "bad transform");
		if (in->x.f.sca < 0.0)
			in->x.f.sca = -in->x.f.sca;
		if (in->x.b.sca < 0.0)
			in->x.b.sca = -in->x.b.sca;
		in->obj = NULL;
		o->os = (char *)in;
	}
	if (in->obj == NULL || (in->obj->ldflags & flags) != flags)
		in->obj = getscene(o->oargs.sarg[0], flags);
	return(in);
}


freeinstance(o)		/* free memory associated with instance */
OBJREC  *o;
{
	if (o->os == NULL)
		return;
	free(o->os);
	o->os = NULL;
}
