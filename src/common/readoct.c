/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  readoct.c - routines to read octree information.
 *
 *     7/30/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "otypes.h"

extern double  atof();
static double  getflt();
static long  getint();
static char  *getstr();
static int  getobj(), octerror();
static OCTREE  getfullnode(), gettree();

static char  *infn;			/* input file name */
static FILE  *infp;			/* input file stream */
static int  objsize;			/* size of stored OBJECT's */
static OBJECT  objorig;			/* zeroeth object */
static short  otypmap[NUMOTYPE+8];	/* object type map */


int
readoct(fname, load, scene, ofn)	/* read in octree from file */
char  *fname;
int  load;
CUBE  *scene;
char  *ofn[];
{
	extern int  fputs();
	char  sbuf[512];
	int  nf;
	OBJECT  fnobjects;
	register int  i;
	long  m;
	
	if (fname == NULL) {
		infn = "standard input";
		infp = stdin;
	} else {
		infn = fname;
		if ((infp = fopen(fname, "r")) == NULL) {
			sprintf(errmsg, "cannot open octree file \"%s\"",
					fname);
			error(SYSTEM, errmsg);
		}
	}
					/* get header */
	if (checkheader(infp, OCTFMT, load&IO_INFO ? stdout : NULL) < 0)
		octerror(USER, "not an octree");
					/* check format */
	if ((objsize = getint(2)-OCTMAGIC) <= 0 ||
			objsize > MAXOBJSIZ || objsize > sizeof(long))
		octerror(USER, "incompatible octree format");
					/* get boundaries */
	if (load & IO_BOUNDS) {
		for (i = 0; i < 3; i++)
			scene->cuorg[i] = atof(getstr(sbuf));
		scene->cusize = atof(getstr(sbuf));
	} else {
		for (i = 0; i < 4; i++)
			getstr(sbuf);
	}
	objorig = nobjects;		/* set object offset */
	nf = 0;				/* get object files */
	while (*getstr(sbuf)) {
		if (load & IO_SCENE)
			readobj(sbuf);
		if (load & IO_FILES)
			ofn[nf] = savqstr(sbuf);
		nf++;
	}
	if (load & IO_FILES)
		ofn[nf] = NULL;
					/* get number of objects */
	fnobjects = m = getint(objsize);
	if (fnobjects != m)
		octerror(USER, "too many objects");

	if (load & IO_TREE) {
						/* get the octree */
		scene->cutree = gettree();
						/* get the scene */
		if (nf == 0 && load & IO_SCENE) {
			for (i = 0; *getstr(sbuf); i++)
				if ((otypmap[i] = otype(sbuf)) < 0) {
					sprintf(errmsg, "unknown type \"%s\"",
							sbuf);
					octerror(WARNING, errmsg);
				}
			while (getobj() != OVOID)
				;
		} else if (load & IO_SCENE) {		/* consistency checks */
						/* check object count */
			if (nobjects != objorig+fnobjects)
				octerror(USER, "bad object count; octree stale?");
						/* check for non-surfaces */
			if (nonsurfinset(objorig, fnobjects))
				octerror(USER, "non-surface in set; octree stale?");
		}
	}
	fclose(infp);
	return(nf);
}


static char *
getstr(s)			/* get null-terminated string */
char  *s;
{
	register char  *cp;
	register int  c;

	cp = s;
	while ((c = getc(infp)) != EOF)
		if ((*cp++ = c) == '\0')
			return(s);
		
	octerror(USER, "truncated octree");
}


static OCTREE
getfullnode()			/* get a set, return fullnode */
{
	OBJECT  set[MAXSET+1];
	register int  i;
	register long  m;

	if ((set[0] = getint(objsize)) > MAXSET)
		octerror(USER, "bad set in getfullnode");
	for (i = 1; i <= set[0]; i++) {
		m = getint(objsize) + objorig;
		if ((set[i] = m) != m)
			octerror(USER, "too many objects");
	}
	return(fullnode(set));
}	


static long
getint(siz)			/* get a siz-byte integer */
register int  siz;
{
	register int  c;
	register long  r;

	if ((c = getc(infp)) == EOF)
		goto end_file;
	r = 0x80&c ? -1<<8|c : c;		/* sign extend */
	while (--siz > 0) {
		if ((c = getc(infp)) == EOF)
			goto end_file;
		r <<= 8;
		r |= c;
	}
	return(r);
end_file:
	octerror(USER, "truncated octree");
}


static double
getflt()			/* get a floating point number */
{
	extern double  ldexp();
	double  d;

	d = (double)getint(4)/0x7fffffff;
	return(ldexp(d, (int)getint(1)));
}
	

static OCTREE
gettree()			/* get a pre-ordered octree */
{
	register OCTREE  ot;
	register int  i;
	
	switch (getc(infp)) {
	case OT_EMPTY:
		return(EMPTY);
	case OT_FULL:
		return(getfullnode());
	case OT_TREE:
		if ((ot = octalloc()) == EMPTY)
			octerror(SYSTEM, "out of tree space in gettree");
		for (i = 0; i < 8; i++)
			octkid(ot, i) = gettree();
		return(ot);
	case EOF:
		octerror(USER, "truncated octree");
	default:
		octerror(USER, "damaged octree");
	}
}


static
getobj()				/* get next object */
{
	char  sbuf[MAXSTR];
	int  obj;
	register int  i;
	register long  m;
	register OBJREC  *objp;

	i = getint(1);
	if (i == -1)
		return(OVOID);		/* terminator */
	if ((obj = newobject()) == OVOID)
		error(SYSTEM, "out of object space");
	objp = objptr(obj);
	if ((objp->otype = otypmap[i]) < 0)
		octerror(USER, "reference to unknown type");
	if ((m = getint(objsize)) != OVOID) {
		m += objorig;
		if ((OBJECT)m != m)
			octerror(USER, "too many objects");
	}
	objp->omod = m;
	objp->oname = savqstr(getstr(sbuf));
	if (objp->oargs.nsargs = getint(2)) {
		objp->oargs.sarg = (char **)bmalloc
				(objp->oargs.nsargs*sizeof(char *));
		if (objp->oargs.sarg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.nsargs; i++)
			objp->oargs.sarg[i] = savestr(getstr(sbuf));
	} else
		objp->oargs.sarg = NULL;
#ifdef  IARGS
	if (objp->oargs.niargs = getint(2)) {
		objp->oargs.iarg = (long *)bmalloc
				(objp->oargs.niargs*sizeof(long));
		if (objp->oargs.iarg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.niargs; i++)
			objp->oargs.iarg[i] = getint(4);
	} else
		objp->oargs.iarg = NULL;
#endif
	if (objp->oargs.nfargs = getint(2)) {
		objp->oargs.farg = (FLOAT *)bmalloc
				(objp->oargs.nfargs*sizeof(FLOAT));
		if (objp->oargs.farg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.nfargs; i++)
			objp->oargs.farg[i] = getflt();
	} else
		objp->oargs.farg = NULL;
						/* initialize */
	objp->os = NULL;
	objp->lastrno = -1;
						/* insert */
	insertobject(obj);
	return(obj);
memerr:
	error(SYSTEM, "out of memory in getobj");
}


static
octerror(etyp, msg)			/* octree error */
int  etyp;
char  *msg;
{
	char  msgbuf[128];

	sprintf(msgbuf, "(%s): %s", infn, msg);
	error(etyp, msgbuf);
}
