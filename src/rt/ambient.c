/* Copyright (c) 1986 Regents of the University of California */

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

#include  "random.h"

#define  OCTSCALE	0.5	/* ceil((valid rad.)/(cube size)) */

extern CUBE  thescene;		/* contains space boundaries */

extern COLOR  ambval;		/* global ambient component */
extern double  ambacc;		/* ambient accuracy */
extern int  ambres;		/* ambient resolution */
extern int  ambdiv;		/* number of divisions for calculation */
extern int  ambssamp;		/* number of super-samples */
extern int  ambounce;		/* number of ambient bounces */
extern char  *amblist[];	/* ambient include/exclude list */
extern int  ambincl;		/* include == 1, exclude == 0 */

#define  MAXASET	511	/* maximum number of elements in ambient set */
OBJECT  ambset[MAXASET+1]={0};	/* ambient include/exclude set */

double  maxarad;		/* maximum ambient radius */
double  minarad;		/* minimum ambient radius */

typedef struct ambval {
	FVECT  pos;		/* position in space */
	FVECT  dir;		/* normal direction */
	int  lvl;		/* recursion level of parent ray */
	float  weight;		/* weight of parent ray */
	COLOR  val;		/* computed ambient value */
	float  rad;		/* validity radius */
	struct ambval  *next;	/* next in list */
}  AMBVAL;			/* ambient value */

typedef struct ambtree {
	AMBVAL  *alist;		/* ambient value list */
	struct ambtree  *kid;	/* 8 child nodes */
}  AMBTREE;			/* ambient octree */

typedef struct {
	float  k;		/* error contribution per sample */
	COLOR  v;		/* ray sum */
	int  n;			/* number of samples */
	short  t, p;		/* theta, phi indices */
}  AMBSAMP;			/* ambient sample */

static AMBTREE  atrunk;		/* our ambient trunk node */

static FILE  *ambfp = NULL;	/* ambient file pointer */

#define  newambval()	(AMBVAL *)bmalloc(sizeof(AMBVAL))

#define  newambtree()	(AMBTREE *)calloc(8, sizeof(AMBTREE))

double  sumambient(), doambient(), makeambient();


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
	double  wsum;

	rdepth++;				/* increment level */

	if (ambdiv <= 0)			/* no ambient calculation */
		goto dumbamb;
						/* check number of bounces */
	if (rdepth > ambounce)
		goto dumbamb;
						/* check ambient list */
	if (ambincl != -1 && r->ro != NULL &&
			ambincl != inset(ambset, r->ro->omod))
		goto dumbamb;

	if (ambacc <= FTINY) {			/* no ambient storage */
		if (doambient(acol, r) == 0.0)
			goto dumbamb;
		goto done;
	}
						/* get ambient value */
	setcolor(acol, 0.0, 0.0, 0.0);
	wsum = sumambient(acol, r, &atrunk, thescene.cuorg, thescene.cusize);
	if (wsum > FTINY)
		scalecolor(acol, 1.0/wsum);
	else if (makeambient(acol, r) == 0.0)
		goto dumbamb;
	goto done;

dumbamb:					/* return global value */
	copycolor(acol, ambval);
done:						/* must finish here! */
	rdepth--;
}


double
sumambient(acol, r, at, c0, s)		/* get interpolated ambient value */
COLOR  acol;
register RAY  *r;
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
		 *  Ray strength test.
		 */
		if (av->lvl > r->rlvl || av->weight < r->rweight-FTINY)
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
		if (d < -minarad)
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
		copycolor(ct, av->val);
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
			wsum += sumambient(acol, r, at->kid+i, ck0, s);
	}
	return(wsum);
}


double
makeambient(acol, r)		/* make a new ambient value */
COLOR  acol;
register RAY  *r;
{
	AMBVAL  amb;

	amb.rad = doambient(acol, r);		/* compute ambient */
	if (amb.rad == 0.0)
		return(0.0);
						/* store it */
	VCOPY(amb.pos, r->rop);
	VCOPY(amb.dir, r->ron);
	amb.lvl = r->rlvl;
	amb.weight = r->rweight;
	copycolor(amb.val, acol);
						/* insert into tree */
	avinsert(&amb, &atrunk, thescene.cuorg, thescene.cusize);
	avsave(&amb);				/* write to file */
	return(amb.rad);
}


double
doambient(acol, r)			/* compute ambient component */
COLOR  acol;
register RAY  *r;
{
	extern int  ambcmp();
	extern double  sin(), cos(), sqrt();
	double  phi, xd, yd, zd;
	double  b, b2;
	register AMBSAMP  *div;
	AMBSAMP  dnew;
	RAY  ar;
	FVECT  ux, uy;
	double  arad;
	int  ndivs, nt, np, ns, ne, i, j;
	register int  k;

	setcolor(acol, 0.0, 0.0, 0.0);
					/* set number of divisions */
	nt = sqrt(ambdiv * r->rweight * 0.5) + 0.5;
	np = 2 * nt;
	ndivs = nt * np;
					/* check first */
	if (ndivs == 0 || rayorigin(&ar, r, AMBIENT, 0.5) < 0)
		return(0.0);
					/* set number of super-samples */
	ns = ambssamp * r->rweight + 0.5;
	if (ns > 0) {
		div = (AMBSAMP *)malloc(ndivs*sizeof(AMBSAMP));
		if (div == NULL)
			error(SYSTEM, "out of memory in doambient");
	}
					/* make axes */
	uy[0] = uy[1] = uy[2] = 0.0;
	for (k = 0; k < 3; k++)
		if (r->ron[k] < 0.6 && r->ron[k] > -0.6)
			break;
	uy[k] = 1.0;
	fcross(ux, r->ron, uy);
	normalize(ux);
	fcross(uy, ux, r->ron);
						/* sample divisions */
	arad = 0.0;
	ne = 0;
	for (i = 0; i < nt; i++)
		for (j = 0; j < np; j++) {
			rayorigin(&ar, r, AMBIENT, 0.5);	/* pretested */
			zd = sqrt((i+frandom())/nt);
			phi = 2.0*PI * (j+frandom())/np;
			xd = cos(phi) * zd;
			yd = sin(phi) * zd;
			zd = sqrt(1.0 - zd*zd);
			for (k = 0; k < 3; k++)
				ar.rdir[k] = xd*ux[k]+yd*uy[k]+zd*r->ron[k];
			rayvalue(&ar);
			if (ar.rot < FHUGE)
				arad += 1.0 / ar.rot;
			if (ns > 0) {			/* save division */
				div[ne].k = 0.0;
				copycolor(div[ne].v, ar.rcol);
				div[ne].n = 0;
				div[ne].t = i; div[ne].p = j;
							/* sum errors */
				b = bright(ar.rcol);
				if (i > 0) {		/* from above */
					b2 = bright(div[ne-np].v) - b;
					b2 *= b2 * 0.25;
					div[ne].k += b2;
					div[ne].n++;
					div[ne-np].k += b2;
					div[ne-np].n++;
				}
				if (j > 0) {		/* from behind */
					b2 = bright(div[ne-1].v) - b;
					b2 *= b2 * 0.25;
					div[ne].k += b2;
					div[ne].n++;
					div[ne-1].k += b2;
					div[ne-1].n++;
				}
				if (j == np-1) {	/* around */
					b2 = bright(div[ne-(np-1)].v) - b;
					b2 *= b2 * 0.25;
					div[ne].k += b2;
					div[ne].n++;
					div[ne-(np-1)].k += b2;
					div[ne-(np-1)].n++;
				}
				ne++;
			} else
				addcolor(acol, ar.rcol);
		}
	for (k = 0; k < ne; k++) {		/* compute errors */
		if (div[k].n > 1)
			div[k].k /= div[k].n;
		div[k].n = 1;
	}
						/* sort the divisions */
	qsort(div, ne, sizeof(AMBSAMP), ambcmp);
						/* skim excess */
	while (ne > ns) {
		ne--;
		addcolor(acol, div[ne].v);
	}
						/* super-sample */
	for (i = ns; i > 0; i--) {
		rayorigin(&ar, r, AMBIENT, 0.5);	/* pretested */
		zd = sqrt((div[0].t+frandom())/nt);
		phi = 2.0*PI * (div[0].p+frandom())/np;
		xd = cos(phi) * zd;
		yd = sin(phi) * zd;
		zd = sqrt(1.0 - zd*zd);
		for (k = 0; k < 3; k++)
			ar.rdir[k] = xd*ux[k]+yd*uy[k]+zd*r->ron[k];
		rayvalue(&ar);
		if (ar.rot < FHUGE)
			arad += 1.0 / ar.rot;
						/* recompute error */
		copycolor(dnew.v, div[0].v);
		addcolor(dnew.v, ar.rcol);
		dnew.n = div[0].n + 1;
		dnew.t = div[0].t; dnew.p = div[0].p;
		b2 = bright(dnew.v)/dnew.n - bright(ar.rcol);
		b2 = b2*b2 + div[0].k*(div[0].n*div[0].n);
		dnew.k = b2/(dnew.n*dnew.n);
						/* reinsert */
		for (k = 0; k < ne-1 && dnew.k < div[k+1].k; k++)
			copystruct(&div[k], &div[k+1]);
		copystruct(&div[k], &dnew);

		if (ne >= i) {		/* extract darkest division */
			ne--;
			if (div[ne].n > 1)
				scalecolor(div[ne].v, 1.0/div[ne].n);
			addcolor(acol, div[ne].v);
		}
	}
	scalecolor(acol, 1.0/ndivs);
	if (arad <= FTINY)
		arad = FHUGE;
	else
		arad = (ndivs+ns) / arad / sqrt(r->rweight);
	if (arad > maxarad)
		arad = maxarad;
	else if (arad < minarad)
		arad = minarad;
	if (ns > 0)
		free((char *)div);
	return(arad);
}


static int
ambcmp(d1, d2)				/* decreasing order */
AMBSAMP  *d1, *d2;
{
	if (d1->k < d2->k)
		return(1);
	if (d1->k > d2->k)
		return(-1);
	return(0);
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
