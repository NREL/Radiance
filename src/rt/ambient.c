/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ambient.c - routines dealing with ambient (inter-reflected) component.
 *
 *  The macro AMBFLUSH (if defined) is the number of ambient values
 *	to wait before flushing to the ambient file.
 *
 *     5/9/86
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "ambient.h"

#include  "random.h"

#define  OCTSCALE	0.5	/* ceil((valid rad.)/(cube size)) */

typedef struct ambtree {
	AMBVAL  *alist;		/* ambient value list */
	struct ambtree  *kid;	/* 8 child nodes */
}  AMBTREE;			/* ambient octree */

extern CUBE  thescene;		/* contains space boundaries */

#define  MAXASET	511	/* maximum number of elements in ambient set */
OBJECT  ambset[MAXASET+1]={0};	/* ambient include/exclude set */

double  maxarad;		/* maximum ambient radius */
double  minarad;		/* minimum ambient radius */

static AMBTREE  atrunk;		/* our ambient trunk node */

static FILE  *ambfp = NULL;	/* ambient file pointer */

#define  newambval()	(AMBVAL *)bmalloc(sizeof(AMBVAL))

#define  newambtree()	(AMBTREE *)calloc(8, sizeof(AMBTREE))


setambient(afile)			/* initialize calculation */
char  *afile;
{
	long  ftell();
	AMBVAL  amb;

	maxarad = thescene.cusize / 2.0;		/* maximum radius */
							/* minimum radius */
	minarad = ambres > 0 ? thescene.cusize/ambres : 0.0;

					/* open ambient file */
	if (afile != NULL)
		if ((ambfp = fopen(afile, "r+")) != NULL) {
			while (fread((char *)&amb,sizeof(AMBVAL),1,ambfp) == 1)
				avinsert(&amb, &atrunk, thescene.cuorg,
						thescene.cusize);
							/* align */
			fseek(ambfp, -(ftell(ambfp)%sizeof(AMBVAL)), 1);
		} else if ((ambfp = fopen(afile, "w")) == NULL) {
			sprintf(errmsg, "cannot open ambient file \"%s\"",
					afile);
			error(SYSTEM, errmsg);
		}
}


ambnotify(obj)			/* record new modifier */
OBJECT  obj;
{
	static int  hitlimit = 0;
	register OBJREC  *o = objptr(obj);
	register char  **amblp;

	if (hitlimit || !ismodifier(o->otype))
		return;
	for (amblp = amblist; *amblp != NULL; amblp++)
		if (!strcmp(o->oname, *amblp)) {
			if (ambset[0] >= MAXASET) {
				error(WARNING, "too many modifiers in ambient list");
				hitlimit++;
				return;		/* should this be fatal? */
			}
			insertelem(ambset, obj);
			return;
		}
}


ambient(acol, r)		/* compute ambient component for ray */
COLOR  acol;
register RAY  *r;
{
	static int  rdepth = 0;			/* ambient recursion */
	double  d;

	if (ambdiv <= 0)			/* no ambient calculation */
		goto dumbamb;
						/* check number of bounces */
	if (rdepth >= ambounce)
		goto dumbamb;
						/* check ambient list */
	if (ambincl != -1 && r->ro != NULL &&
			ambincl != inset(ambset, r->ro->omod))
		goto dumbamb;

	if (ambacc <= FTINY) {			/* no ambient storage */
		rdepth++;
		d = doambient(acol, r, r->rweight, NULL, NULL);
		rdepth--;
		if (d == 0.0)
			goto dumbamb;
		return;
	}
						/* get ambient value */
	setcolor(acol, 0.0, 0.0, 0.0);
	d = sumambient(acol, r, rdepth,
			&atrunk, thescene.cuorg, thescene.cusize);
	if (d > FTINY)
		scalecolor(acol, 1.0/d);
	else {
		d = makeambient(acol, r, rdepth++);
		rdepth--;
	}
	if (d > FTINY)
		return;
dumbamb:					/* return global value */
	copycolor(acol, ambval);
}


double
sumambient(acol, r, al, at, c0, s)	/* get interpolated ambient value */
COLOR  acol;
register RAY  *r;
int  al;
AMBTREE  *at;
FVECT  c0;
double  s;
{
	extern double  sqrt();
	double  d, e1, e2, wt, wsum;
	COLOR  ct;
	FVECT  ck0;
	int  i;
	register int  j;
	register AMBVAL  *av;
					/* do this node */
	wsum = 0.0;
	for (av = at->alist; av != NULL; av = av->next) {
		/*
		 *  Ambient level test.
		 */
		if (av->lvl > al || av->weight < r->rweight-FTINY)
			continue;
		/*
		 *  Ambient radius test.
		 */
		e1 = 0.0;
		for (j = 0; j < 3; j++) {
			d = av->pos[j] - r->rop[j];
			e1 += d * d;
		}
		e1 /= av->rad * av->rad;
		if (e1 > ambacc*ambacc*1.21)
			continue;
		/*
		 *  Normal direction test.
		 */
		e2 = (1.0 - DOT(av->dir, r->ron)) * r->rweight;
		if (e2 < 0.0) e2 = 0.0;
		if (e1 + e2 > ambacc*ambacc*1.21)
			continue;
		/*
		 *  Ray behind test.
		 */
		d = 0.0;
		for (j = 0; j < 3; j++)
			d += (r->rop[j] - av->pos[j]) *
					(av->dir[j] + r->ron[j]);
		if (d*0.5 < -minarad*ambacc)
			continue;
		/*
		 *  Jittering final test reduces image artifacts.
		 */
		wt = sqrt(e1) + sqrt(e2);
		wt *= .9 + .2*frandom();
		if (wt > ambacc)
			continue;
		if (wt <= 1e-3)
			wt = 1e3;
		else
			wt = 1.0 / wt;
		wsum += wt;
		extambient(ct, av, r->rop, r->ron);
		scalecolor(ct, wt);
		addcolor(acol, ct);
	}
	if (at->kid == NULL)
		return(wsum);
					/* do children */
	s *= 0.5;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 3; j++) {
			ck0[j] = c0[j];
			if (1<<j & i)
				ck0[j] += s;
			if (r->rop[j] < ck0[j] - OCTSCALE*s)
				break;
			if (r->rop[j] > ck0[j] + (1.0+OCTSCALE)*s)
				break;
		}
		if (j == 3)
			wsum += sumambient(acol, r, al, at->kid+i, ck0, s);
	}
	return(wsum);
}


double
makeambient(acol, r, al)	/* make a new ambient value */
COLOR  acol;
register RAY  *r;
int  al;
{
	AMBVAL  amb;
	FVECT	gp, gd;
						/* compute weight */
	amb.weight = pow(AVGREFL, (double)al);
	if (r->rweight < 0.1*amb.weight)	/* heuristic */
		amb.weight = r->rweight;
						/* compute ambient */
	amb.rad = doambient(acol, r, amb.weight, gp, gd);
	if (amb.rad == 0.0)
		return(0.0);
						/* store it */
	VCOPY(amb.pos, r->rop);
	VCOPY(amb.dir, r->ron);
	amb.lvl = al;
	copycolor(amb.val, acol);
	VCOPY(amb.gpos, gp);
	VCOPY(amb.gdir, gd);
						/* insert into tree */
	avinsert(&amb, &atrunk, thescene.cuorg, thescene.cusize);
	avsave(&amb);				/* write to file */
	return(amb.rad);
}


extambient(cr, ap, pv, nv)		/* extrapolate value at pv, nv */
COLOR  cr;
register AMBVAL  *ap;
FVECT  pv, nv;
{
	FVECT  v1, v2;
	register int  i;
	double  d;

	d = 1.0;			/* zeroeth order */
					/* gradient due to translation */
	for (i = 0; i < 3; i++)
		d += ap->gpos[i]*(pv[i]-ap->pos[i]);
					/* gradient due to rotation */
	VCOPY(v1, ap->dir);
	fcross(v2, v1, nv);
	d += DOT(ap->gdir, v2);
	if (d <= 0.0) {
		setcolor(cr, 0.0, 0.0, 0.0);
		return;
	}
	copycolor(cr, ap->val);
	scalecolor(cr, d);
}


static
avsave(av)				/* save an ambient value */
AMBVAL  *av;
{
#ifdef  AMBFLUSH
	static int  nunflshed = 0;
#endif
	if (ambfp == NULL)
		return;
	if (fwrite((char *)av, sizeof(AMBVAL), 1, ambfp) != 1)
		goto writerr;
#ifdef  AMBFLUSH
	if (++nunflshed >= AMBFLUSH) {
		if (fflush(ambfp) == EOF)
			goto writerr;
		nunflshed = 0;
	}
#endif
	return;
writerr:
	error(SYSTEM, "error writing ambient file");
}


static
avinsert(aval, at, c0, s)		/* insert ambient value in a tree */
AMBVAL  *aval;
register AMBTREE  *at;
FVECT  c0;
double  s;
{
	FVECT  ck0;
	int  branch;
	register AMBVAL  *av;
	register int  i;

	if ((av = newambval()) == NULL)
		goto memerr;
	copystruct(av, aval);
	VCOPY(ck0, c0);
	while (s*(OCTSCALE/2) > av->rad*ambacc) {
		if (at->kid == NULL)
			if ((at->kid = newambtree()) == NULL)
				goto memerr;
		s *= 0.5;
		branch = 0;
		for (i = 0; i < 3; i++)
			if (av->pos[i] > ck0[i] + s) {
				ck0[i] += s;
				branch |= 1 << i;
			}
		at = at->kid + branch;
	}
	av->next = at->alist;
	at->alist = av;
	return;
memerr:
	error(SYSTEM, "out of memory in avinsert");
}
