/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ambient.c - routines dealing with ambient (inter-reflected) component.
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "ambient.h"

#include  "random.h"

#ifndef  OCTSCALE
#define	 OCTSCALE	1.0	/* ceil((valid rad.)/(cube size)) */
#endif

typedef struct ambtree {
	AMBVAL	*alist;		/* ambient value list */
	struct ambtree	*kid;	/* 8 child nodes */
}  AMBTREE;			/* ambient octree */

extern CUBE  thescene;		/* contains space boundaries */

extern char  *shm_boundary;	/* memory sharing boundary */

#define	 MAXASET	511	/* maximum number of elements in ambient set */
OBJECT	ambset[MAXASET+1]={0};	/* ambient include/exclude set */

double	maxarad;		/* maximum ambient radius */
double	minarad;		/* minimum ambient radius */

static AMBTREE	atrunk;		/* our ambient trunk node */

static FILE  *ambfp = NULL;	/* ambient file pointer */
static int  nunflshed = 0;	/* number of unflushed ambient values */

#ifndef SORT_THRESH
#ifdef BIGMEM
#define SORT_THRESH	((9L<<20)/sizeof(AMBVAL))
#else
#define SORT_THRESH	((3L<<20)/sizeof(AMBVAL))
#endif
#endif
#ifndef SORT_INTVL
#define SORT_INTVL	(SORT_THRESH*256)
#endif
#ifndef MAX_SORT_INTVL
#define MAX_SORT_INTVL	(SORT_INTVL<<4)
#endif

static COLOR  avsum = BLKCOLOR;		/* computed ambient value sum */
static unsigned int  nambvals = 0;	/* total number of indirect values */
static unsigned int  nambshare = 0;	/* number of values from file */
static unsigned long  ambclock = 0;	/* ambient access clock */
static unsigned long  lastsort = 0;	/* time of last value sort */
static long  sortintvl = SORT_INTVL;	/* time until next sort */

#define MAXACLOCK	(1L<<30)	/* clock turnover value */
	/*
	 * Track access times unless we are sharing ambient values
	 * through memory on a multiprocessor, when we want to avoid
	 * claiming our own memory (copy on write).  Go ahead anyway
	 * if more than two thirds of our values are unshared.
	 */
#define tracktime	(shm_boundary == NULL || nambvals > 3*nambshare)

#define	 AMBFLUSH	(BUFSIZ/AMBVALSIZ)

#define	 newambval()	(AMBVAL *)bmalloc(sizeof(AMBVAL))

extern long  ftell(), lseek();
static int  initambfile(), avsave(), avinsert(), sortambvals();
static AMBVAL  *avstore();
#ifdef  F_SETLKW
static  aflock();
#endif


setambres(ar)				/* set ambient resolution */
int  ar;
{
	ambres = ar < 0 ? 0 : ar;		/* may be done already */
						/* set min & max radii */
	if (ar <= 0) {
		minarad = 0;
		maxarad = thescene.cusize / 2.0;
	} else {
		minarad = thescene.cusize / ar;
		maxarad = 64 * minarad;			/* heuristic */
		if (maxarad > thescene.cusize / 2.0)
			maxarad = thescene.cusize / 2.0;
	}
	if (minarad <= FTINY)
		minarad = 10*FTINY;
	if (maxarad <= minarad)
		maxarad = 64 * minarad;
}


setambacc(newa)				/* set ambient accuracy */
double  newa;
{
	double  ambdiff;

	if (newa < 0.0)
		newa = 0.0;
	ambdiff = fabs(newa - ambacc);
	if (ambdiff >= .01 && (ambacc = newa) > FTINY && nambvals > 0)
		sortambvals(1);			/* rebuild tree */
}


setambient(afile)			/* initialize calculation */
char  *afile;
{
	long  pos, flen;
	AMBVAL	amb;
						/* init ambient limits */
	setambres(ambres);
	setambacc(ambacc);
	if (afile == NULL)
		return;
	if (ambacc <= FTINY) {
		sprintf(errmsg, "zero ambient accuracy so \"%s\" not opened",
				afile);
		error(WARNING, errmsg);
		return;
	}
						/* open ambient file */
	if ((ambfp = fopen(afile, "r+")) != NULL) {
		initambfile(0);
		pos = ftell(ambfp);
		while (readambval(&amb, ambfp))
			avinsert(avstore(&amb));
						/* align */
		pos += (long)nambvals*AMBVALSIZ;
		flen = lseek(fileno(ambfp), 0L, 2);
		if (flen != pos) {
			error(WARNING,
			"ignoring last %ld values in ambient file (corrupted)",
					(flen - pos)/AMBVALSIZ);
			fseek(ambfp, pos, 0);
			ftruncate(fileno(ambfp), pos);
		}
		nambshare = nambvals;
	} else if ((ambfp = fopen(afile, "w+")) != NULL)
		initambfile(1);
	else {
		sprintf(errmsg, "cannot open ambient file \"%s\"", afile);
		error(SYSTEM, errmsg);
	}
	nunflshed++;	/* lie */
	ambsync();
}


ambnotify(obj)			/* record new modifier */
OBJECT	obj;
{
	static int  hitlimit = 0;
	register OBJREC	 *o = objptr(obj);
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


ambient(acol, r, nrm)		/* compute ambient component for ray */
COLOR  acol;
register RAY  *r;
FVECT  nrm;
{
	static int  rdepth = 0;			/* ambient recursion */
	double	d;

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
		if (d <= FTINY)
			goto dumbamb;
		return;
	}
						/* resort memory? */
	sortambvals(0);
						/* get ambient value */
	setcolor(acol, 0.0, 0.0, 0.0);
	d = sumambient(acol, r, nrm, rdepth,
			&atrunk, thescene.cuorg, thescene.cusize);
	if (d > FTINY) {
		scalecolor(acol, 1.0/d);
		return;
	}
	rdepth++;				/* need to cache new value */
	d = makeambient(acol, r, nrm, rdepth-1);
	rdepth--;
	if (d > FTINY)
		return;
dumbamb:					/* return global value */
	copycolor(acol, ambval);
	if (ambvwt <= 0 | nambvals == 0)
		return;
	scalecolor(acol, (double)ambvwt);
	addcolor(acol, avsum);			/* average in computations */
	d = 1.0/(ambvwt+nambvals);
	scalecolor(acol, d);
}


double
sumambient(acol, r, rn, al, at, c0, s)	/* get interpolated ambient value */
COLOR  acol;
register RAY  *r;
FVECT  rn;
int  al;
AMBTREE	 *at;
FVECT  c0;
double	s;
{
	double	d, e1, e2, wt, wsum;
	COLOR  ct;
	FVECT  ck0;
	int  i;
	register int  j;
	register AMBVAL	 *av;

	wsum = 0.0;
					/* do this node */
	for (av = at->alist; av != NULL; av = av->next) {
		if (tracktime)
			av->latick = ambclock++;
		/*
		 *  Ambient level test.
		 */
		if (av->lvl > al)	/* list sorted, so this works */
			break;
		if (av->weight < r->rweight-FTINY)
			continue;
		/*
		 *  Ambient radius test.
		 */
		d = av->pos[0] - r->rop[0];
		e1 = d * d;
		d = av->pos[1] - r->rop[1];
		e1 += d * d;
		d = av->pos[2] - r->rop[2];
		e1 += d * d;
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
		if (d*0.5 < -minarad*ambacc-.001)
			continue;
		/*
		 *  Jittering final test reduces image artifacts.
		 */
		wt = sqrt(e1) + sqrt(e2);
		if (wt > ambacc*(.9+.2*urand(9015+samplendx)))
			continue;
		if (wt <= 1e-3)
			wt = 1e3;
		else
			wt = 1.0 / wt;
		wsum += wt;
		extambient(ct, av, r->rop, rn);
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
			wsum += sumambient(acol, r, rn, al, at->kid+i, ck0, s);
	}
	return(wsum);
}


double
makeambient(acol, r, rn, al)	/* make a new ambient value */
COLOR  acol;
register RAY  *r;
FVECT  rn;
int  al;
{
	AMBVAL	amb;
	FVECT	gp, gd;
						/* compute weight */
	amb.weight = pow(AVGREFL, (double)al);
	if (r->rweight < 0.1*amb.weight)	/* heuristic */
		amb.weight = r->rweight;
						/* compute ambient */
	amb.rad = doambient(acol, r, amb.weight, gp, gd);
	if (amb.rad <= FTINY)
		return(0.0);
						/* store it */
	VCOPY(amb.pos, r->rop);
	VCOPY(amb.dir, r->ron);
	amb.lvl = al;
	copycolor(amb.val, acol);
	VCOPY(amb.gpos, gp);
	VCOPY(amb.gdir, gd);
						/* insert into tree */
	avsave(&amb);				/* and save to file */
	if (rn != r->ron)
		extambient(acol, &amb, r->rop, rn);	/* texture */
	return(amb.rad);
}


extambient(cr, ap, pv, nv)		/* extrapolate value at pv, nv */
COLOR  cr;
register AMBVAL	 *ap;
FVECT  pv, nv;
{
	FVECT  v1, v2;
	register int  i;
	double	d;

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
initambfile(creat)		/* initialize ambient file */
int  creat;
{
	extern char  *progname, *octname, VersionID[];

#ifdef	F_SETLKW
	aflock(creat ? F_WRLCK : F_RDLCK);
#endif
#ifdef MSDOS
	setmode(fileno(ambfp), O_BINARY);
#endif
	setbuf(ambfp, bmalloc(BUFSIZ+8));
	if (creat) {			/* new file */
		newheader("RADIANCE", ambfp);
		fprintf(ambfp, "%s -av %g %g %g -ab %d -aa %g ",
				progname, colval(ambval,RED),
				colval(ambval,GRN), colval(ambval,BLU),
				ambounce, ambacc);
		fprintf(ambfp, "-ad %d -as %d -ar %d %s\n",
				ambdiv, ambssamp, ambres,
				octname==NULL ? "" : octname);
		fprintf(ambfp, "SOFTWARE= %s\n", VersionID);
		fputformat(AMBFMT, ambfp);
		putc('\n', ambfp);
		putambmagic(ambfp);
	} else if (checkheader(ambfp, AMBFMT, NULL) < 0 || !hasambmagic(ambfp))
		error(USER, "bad ambient file");
}


static
avsave(av)				/* insert and save an ambient value */
AMBVAL	*av;
{
	avinsert(avstore(av));
	if (ambfp == NULL)
		return;
	if (writambval(av, ambfp) < 0)
		goto writerr;
	if (++nunflshed >= AMBFLUSH)
		if (ambsync() == EOF)
			goto writerr;
	return;
writerr:
	error(SYSTEM, "error writing ambient file");
}


static AMBVAL *
avstore(aval)				/* allocate memory and store aval */
register AMBVAL  *aval;
{
	register AMBVAL  *av;

	if ((av = newambval()) == NULL)
		error(SYSTEM, "out of memory in avstore");
	copystruct(av, aval);
	av->latick = ambclock;
	av->next = NULL;
	addcolor(avsum, av->val);	/* add to sum for averaging */
	nambvals++;
	return(av);
}


#define ATALLOCSZ	512		/* #/8 trees to allocate at once */

static AMBTREE  *atfreelist = NULL;	/* free ambient tree structures */


static
AMBTREE *
newambtree()				/* allocate 8 ambient tree structs */
{
	register AMBTREE  *atp, *upperlim;

	if (atfreelist == NULL) {	/* get more nodes */
		atfreelist = (AMBTREE *)bmalloc(ATALLOCSZ*8*sizeof(AMBTREE));
		if (atfreelist == NULL)
			return(NULL);
					/* link new free list */
		upperlim = atfreelist + 8*(ATALLOCSZ-1);
		for (atp = atfreelist; atp < upperlim; atp += 8)
			atp->kid = atp + 8;
		atp->kid = NULL;
	}
	atp = atfreelist;
	atfreelist = atp->kid;
	bzero((char *)atp, 8*sizeof(AMBTREE));
	return(atp);
}


static
freeambtree(atp)			/* free 8 ambient tree structs */
AMBTREE  *atp;
{
	atp->kid = atfreelist;
	atfreelist = atp;
}


static
avinsert(av)				/* insert ambient value in our tree */
register AMBVAL	 *av;
{
	register AMBTREE  *at;
	register AMBVAL  *ap;
	AMBVAL  avh;
	FVECT  ck0;
	double	s;
	int  branch;
	register int  i;

	if (av->rad <= FTINY)
		error(CONSISTENCY, "zero ambient radius in avinsert");
	at = &atrunk;
	VCOPY(ck0, thescene.cuorg);
	s = thescene.cusize;
	while (s*(OCTSCALE/2) > av->rad*ambacc) {
		if (at->kid == NULL)
			if ((at->kid = newambtree()) == NULL)
				error(SYSTEM, "out of memory in avinsert");
		s *= 0.5;
		branch = 0;
		for (i = 0; i < 3; i++)
			if (av->pos[i] > ck0[i] + s) {
				ck0[i] += s;
				branch |= 1 << i;
			}
		at = at->kid + branch;
	}
	avh.next = at->alist;		/* order by increasing level */
	for (ap = &avh; ap->next != NULL; ap = ap->next)
		if (ap->next->lvl >= av->lvl)
			break;
	av->next = ap->next;
	ap->next = av;
	at->alist = avh.next;
}


static
unloadatree(at, f)			/* unload an ambient value tree */
register AMBTREE  *at;
int	(*f)();
{
	register AMBVAL  *av;
	register int  i;
					/* transfer values at this node */
	for (av = at->alist; av != NULL; av = at->alist) {
		at->alist = av->next;
		(*f)(av);
	}
	if (at->kid == NULL)
		return;
	for (i = 0; i < 8; i++)		/* transfer and free children */
		unloadatree(at->kid+i, f);
	freeambtree(at->kid);
	at->kid = NULL;
}


static AMBVAL	**avlist1, **avlist2;	/* ambient value lists for sorting */
static int	i_avlist;		/* index for lists */


static
av2list(av)
AMBVAL	*av;
{
#ifdef DEBUG
	if (i_avlist >= nambvals)
		error(CONSISTENCY, "too many ambient values in av2list1");
#endif
	avlist1[i_avlist] = avlist2[i_avlist] = av;
	i_avlist++;
}


static int
alatcmp(avp1, avp2)			/* compare ambient values for MRA */
AMBVAL	**avp1, **avp2;
{
	return((**avp2).latick - (**avp1).latick);
}


static int
aposcmp(avp1, avp2)			/* compare ambient value positions */
AMBVAL	**avp1, **avp2;
{
	return(*avp1 - *avp2);
}


#ifdef DEBUG
static int
avlmemi(avaddr)				/* find list position from address */
AMBVAL	*avaddr;
{
	register AMBVAL  **avlpp;

	avlpp = (AMBVAL **)bsearch((char *)&avaddr, (char *)avlist2,
			nambvals, sizeof(AMBVAL *), aposcmp);
	if (avlpp == NULL)
		error(CONSISTENCY, "address not found in avlmemi");
	return(avlpp - avlist2);
}
#else
#define avlmemi(avaddr)	((AMBVAL **)bsearch((char *)&avaddr,(char *)avlist2, \
				nambvals,sizeof(AMBVAL *),aposcmp) - avlist2)
#endif


static
sortambvals(always)			/* resort ambient values */
int	always;
{
	AMBTREE  oldatrunk;
	AMBVAL	tav, *tap, *pnext;
	register int	i, j;
					/* see if it's time yet */
	if (!always && (ambclock < lastsort+sortintvl ||
			nambvals < SORT_THRESH))
		return;
	/*
	 * The idea here is to minimize memory thrashing
	 * in VM systems by improving reference locality.
	 * We do this by periodically sorting our stored ambient
	 * values in memory in order of most recently to least
	 * recently accessed.  This ordering was chosen so that new
	 * ambient values (which tend to be less important) go into
	 * higher memory with the infrequently accessed values.
	 *	Since we expect our values to need sorting less
	 * frequently as the process continues, we double our
	 * waiting interval after each call.
	 * 	This routine is also called by setambacc() with
	 * the "always" parameter set to 1 so that the ambient
	 * tree will be rebuilt with the new accuracy parameter.
	 */
	if (tracktime) {		/* allocate pointer arrays to sort */
		avlist1 = (AMBVAL **)malloc(nambvals*sizeof(AMBVAL *));
		avlist2 = (AMBVAL **)malloc(nambvals*sizeof(AMBVAL *));
	} else
		avlist1 = avlist2 = NULL;
	if (avlist2 == NULL) {		/* no time tracking -- rebuild tree? */
		if (avlist1 != NULL)
			free((char *)avlist1);
		if (always) {		/* rebuild without sorting */
			copystruct(&oldatrunk, &atrunk);
			atrunk.alist = NULL;
			atrunk.kid = NULL;
			unloadatree(&oldatrunk, avinsert);
		}
	} else {			/* sort memory by last access time */
		/*
		 * Sorting memory is tricky because it isn't contiguous.
		 * We have to sort an array of pointers by MRA and also
		 * by memory position.  We then copy values in "loops"
		 * to minimize memory hits.  Nevertheless, we will visit
		 * everyone at least twice, and this is an expensive process
		 * when we're thrashing, which is when we need to do it.
		 */
#ifdef DEBUG
		sprintf(errmsg, "sorting %u ambient values at ambclock=%lu...",
				nambvals, ambclock);
		eputs(errmsg);
#endif
		i_avlist = 0;
		unloadatree(&atrunk, av2list);	/* empty current tree */
#ifdef DEBUG
		if (i_avlist < nambvals)
			error(CONSISTENCY, "missing ambient values in sortambvals");
#endif
		qsort((char *)avlist1, nambvals, sizeof(AMBVAL *), alatcmp);
		qsort((char *)avlist2, nambvals, sizeof(AMBVAL *), aposcmp);
		for (i = 0; i < nambvals; i++) {
			if (avlist1[i] == NULL)
				continue;
			tap = avlist2[i];
			copystruct(&tav, tap);
			for (j = i; (pnext = avlist1[j]) != tap;
					j = avlmemi(pnext)) {
				copystruct(avlist2[j], pnext);
				avinsert(avlist2[j]);
				avlist1[j] = NULL;
			}
			copystruct(avlist2[j], &tav);
			avinsert(avlist2[j]);
			avlist1[j] = NULL;
		}
		free((char *)avlist1);
		free((char *)avlist2);
						/* compute new sort interval */
		sortintvl = ambclock - lastsort;
		if (sortintvl >= MAX_SORT_INTVL/2)
			sortintvl = MAX_SORT_INTVL;
		else
			sortintvl <<= 1;	/* wait twice as long next */
#ifdef DEBUG
		eputs("done\n");
#endif
	}
	if (ambclock >= MAXACLOCK)
		ambclock = MAXACLOCK/2;
	lastsort = ambclock;
}


#ifdef	F_SETLKW

static
aflock(typ)			/* lock/unlock ambient file */
int  typ;
{
	static struct flock  fls;	/* static so initialized to zeroes */

	fls.l_type = typ;
	if (fcntl(fileno(ambfp), F_SETLKW, &fls) < 0)
		error(SYSTEM, "cannot (un)lock ambient file");
}


int
ambsync()			/* synchronize ambient file */
{
	static FILE  *ambinp = NULL;
	static long  lastpos = -1;
	long  flen;
	AMBVAL	avs;
	register int  n;

	if (nunflshed == 0)
		return(0);
	if (lastpos < 0)	/* initializing (locked in initambfile) */
		goto syncend;
				/* gain exclusive access */
	aflock(F_WRLCK);
				/* see if file has grown */
	if ((flen = lseek(fileno(ambfp), 0L, 2)) < 0)
		goto seekerr;
	if (n = flen - lastpos) {		/* file has grown */
		if (ambinp == NULL) {		/* use duplicate filedes */
			ambinp = fdopen(dup(fileno(ambfp)), "r");
			if (ambinp == NULL)
				error(SYSTEM, "fdopen failed in ambsync");
		}
		if (fseek(ambinp, lastpos, 0) < 0)
			goto seekerr;
		while (n >= AMBVALSIZ) {	/* load contributed values */
			if (!readambval(&avs, ambinp)) {
				sprintf(errmsg,
				"ambient file corrupted near character %ld",
						flen - n);
				error(WARNING, errmsg);
				break;
			}
			avinsert(avstore(&avs));
			n -= AMBVALSIZ;
		}
		/*** seek always as safety measure
		if (n) ***/			/* alignment */
			if (lseek(fileno(ambfp), flen-n, 0) < 0)
				goto seekerr;
	}
#ifdef  DEBUG
	if (ambfp->_ptr - ambfp->_base != nunflshed*AMBVALSIZ) {
		sprintf(errmsg, "ambient file buffer at %d rather than %d",
				ambfp->_ptr - ambfp->_base,
				nunflshed*AMBVALSIZ);
		error(CONSISTENCY, errmsg);
	}
#endif
syncend:
	n = fflush(ambfp);			/* calls write() at last */
	if ((lastpos = lseek(fileno(ambfp), 0L, 1)) < 0)
		goto seekerr;
	aflock(F_UNLCK);			/* release file */
	nunflshed = 0;
	return(n);
seekerr:
	error(SYSTEM, "seek failed in ambsync");
}

#else

int
ambsync()			/* flush ambient file */
{
	if (nunflshed == 0)
		return(0);
	nunflshed = 0;
	return(fflush(ambfp));
}

#endif
