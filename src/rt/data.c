/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  data.c - routines dealing with interpolated data.
 *
 *     6/4/86
 */

#include  "standard.h"

#include  "color.h"

#include  "resolu.h"

#include  "data.h"

				/* picture memory usage before warning */
#ifndef PSIZWARN
#ifdef BIGMEM
#define PSIZWARN	3000000
#else
#define PSIZWARN	1000000
#endif
#endif

#ifndef TABSIZ
#define TABSIZ		97		/* table size (prime) */
#endif

#define hash(s)		(shash(s)%TABSIZ)


extern char  *getlibpath();		/* library search path */

static DATARRAY	 *dtab[TABSIZ];		/* data array list */

static DATARRAY	 *ptab[TABSIZ];		/* picture list */


DATARRAY *
getdata(dname)				/* get data array dname */
char  *dname;
{
	char  *dfname;
	FILE  *fp;
	int  asize;
	register int  i, j;
	register DATARRAY  *dp;
						/* look for array in list */
	for (dp = dtab[hash(dname)]; dp != NULL; dp = dp->next)
		if (!strcmp(dname, dp->name))
			return(dp);		/* found! */

	/*
	 *	If we haven't loaded the data already, we will look
	 *  for it in the directories specified by the library path.
	 *
	 *	The file has the following format:
	 *
	 *		N
	 *		beg0	end0	n0
	 *		beg1	end1	n1
	 *		. . .
	 *		begN	endN	nN
	 *		data, later dimensions changing faster
	 *		. . .
	 *
	 *	For irregularly spaced points, the following can be
	 *  substituted for begi endi ni:
	 *
	 *		0 0 ni p0i p1i .. pni
	 */

	if ((dfname = getpath(dname, getlibpath(), R_OK)) == NULL) {
		sprintf(errmsg, "cannot find data file \"%s\"", dname);
		error(USER, errmsg);
	}
	if ((dp = (DATARRAY *)malloc(sizeof(DATARRAY))) == NULL)
		goto memerr;

	dp->name = savestr(dname);

	if ((fp = fopen(dfname, "r")) == NULL) {
		sprintf(errmsg, "cannot open data file \"%s\"", dfname);
		error(SYSTEM, errmsg);
	}
							/* get dimensions */
	if (fgetval(fp, 'i', &dp->nd) <= 0)
		goto scanerr;
	if (dp->nd <= 0 || dp->nd > MAXDDIM) {
		sprintf(errmsg, "bad number of dimensions for \"%s\"", dname);
		error(USER, errmsg);
	}
	asize = 1;
	for (i = 0; i < dp->nd; i++) {
		if (fgetval(fp, 'd', &dp->dim[i].org) <= 0)
			goto scanerr;
		if (fgetval(fp, 'd', &dp->dim[i].siz) <= 0)
			goto scanerr;
		if (fgetval(fp, 'i', &dp->dim[i].ne) <= 0)
			goto scanerr;
		if (dp->dim[i].ne < 2)
			goto scanerr;
		asize *= dp->dim[i].ne;
		if ((dp->dim[i].siz -= dp->dim[i].org) == 0) {
			dp->dim[i].p = (double *)malloc(dp->dim[i].ne*sizeof(double));
			if (dp->dim[i].p == NULL)
				goto memerr;
			for (j = 0; j < dp->dim[i].ne; j++)
				if (fgetval(fp, 'd', &dp->dim[i].p[i]) <= 0)
					goto scanerr;
			for (j = 1; j < dp->dim[i].ne-1; j++)
				if ((dp->dim[i].p[j-1] < dp->dim[i].p[j]) !=
					(dp->dim[i].p[j] < dp->dim[i].p[j+1]))
					goto scanerr;
			dp->dim[i].org = dp->dim[i].p[0];
			dp->dim[i].siz = dp->dim[i].p[dp->dim[i].ne-1]
						- dp->dim[i].p[0];
		} else
			dp->dim[i].p = NULL;
	}
	if ((dp->arr = (DATATYPE *)malloc(asize*sizeof(DATATYPE))) == NULL)
		goto memerr;
	
	for (i = 0; i < asize; i++)
		if (fgetval(fp, DATATY, &dp->arr[i]) <= 0)
			goto scanerr;
	fclose(fp);
	i = hash(dname);
	dp->next = dtab[i];
	return(dtab[i] = dp);

memerr:
	error(SYSTEM, "out of memory in getdata");
scanerr:
	sprintf(errmsg, "%s in data file \"%s\"",
			feof(fp) ? "unexpected EOF" : "bad format", dfname);
	error(USER, errmsg);
}


static
headaspect(s, iap)			/* check string for aspect ratio */
char  *s;
double  *iap;
{
	if (isaspect(s))
		*iap *= aspectval(s);
}


DATARRAY *
getpict(pname)				/* get picture pname */
char  *pname;
{
	double  inpaspect;
	char  *pfname;
	FILE  *fp;
	COLOR  *scanin;
	int  sl, ns;
	RESOLU	inpres;
	FLOAT  loc[2];
	int  y;
	register int  x, i;
	register DATARRAY  *pp;
						/* look for array in list */
	for (pp = ptab[hash(pname)]; pp != NULL; pp = pp->next)
		if (!strcmp(pname, pp->name))
			return(pp);		/* found! */

	if ((pfname = getpath(pname, getlibpath(), R_OK)) == NULL) {
		sprintf(errmsg, "cannot find picture file \"%s\"", pname);
		error(USER, errmsg);
	}
	if ((pp = (DATARRAY *)calloc(3, sizeof(DATARRAY))) == NULL)
		goto memerr;

	pp[0].name = 
	pp[1].name =
	pp[2].name = savestr(pname);

	if ((fp = fopen(pfname, "r")) == NULL) {
		sprintf(errmsg, "cannot open picture file \"%s\"", pfname);
		error(SYSTEM, errmsg);
	}
#ifdef MSDOS
	setmode(fileno(fp), O_BINARY);
#endif
						/* get dimensions */
	inpaspect = 1.0;
	getheader(fp, headaspect, &inpaspect);
	if (!fgetsresolu(&inpres, fp))
		goto readerr;
#if PSIZWARN
						/* check memory usage */
	i = 3*sizeof(DATATYPE)*inpres.xr*inpres.yr;
	if (i > PSIZWARN) {
		sprintf(errmsg, "picture file \"%s\" using %d bytes of memory",
				pname, i);
		error(WARNING, errmsg);
	}
#endif
	for (i = 0; i < 3; i++) {
		pp[i].nd = 2;
		pp[i].dim[0].ne = inpres.yr;
		pp[i].dim[1].ne = inpres.xr;
		pp[i].dim[0].org =
		pp[i].dim[1].org = 0.0;
		if (inpres.xr <= inpres.yr*inpaspect) {
			pp[i].dim[0].siz = inpaspect *
						(double)inpres.yr/inpres.xr;
			pp[i].dim[1].siz = 1.0;
		} else {
			pp[i].dim[0].siz = 1.0;
			pp[i].dim[1].siz = (double)inpres.xr/inpres.yr /
						inpaspect;
		}
		pp[i].dim[0].p = pp[i].dim[1].p = NULL;
		pp[i].arr = (DATATYPE *)
				malloc(inpres.xr*inpres.yr*sizeof(DATATYPE));
		if (pp[i].arr == NULL)
			goto memerr;
	}
							/* load picture */
	sl = scanlen(&inpres);
	ns = numscans(&inpres);
	if ((scanin = (COLOR *)malloc(sl*sizeof(COLOR))) == NULL)
		goto memerr;
	for (y = 0; y < ns; y++) {
		if (freadscan(scanin, sl, fp) < 0)
			goto readerr;
		for (x = 0; x < sl; x++) {
			pix2loc(loc, &inpres, x, y);
			i = (int)(loc[1]*inpres.yr)*inpres.xr +
					(int)(loc[0]*inpres.xr);
			pp[0].arr[i] = colval(scanin[x],RED);
			pp[1].arr[i] = colval(scanin[x],GRN);
			pp[2].arr[i] = colval(scanin[x],BLU);
		}
	}
	free((char *)scanin);
	fclose(fp);
	i = hash(pname);
	pp[0].next =
	pp[1].next =
	pp[2].next = ptab[i];
	return(ptab[i] = pp);

memerr:
	error(SYSTEM, "out of memory in getpict");
readerr:
	sprintf(errmsg, "bad picture file \"%s\"", pfname);
	error(USER, errmsg);
}


freedata(dname)			/* free memory associated with dname */
char  *dname;
{
	DATARRAY  head;
	int  hval, nents;
	register DATARRAY  *dp, *dpl;
	register int  i;

	if (dname == NULL) {			/* free all if NULL */
		hval = 0; nents = TABSIZ;
	} else {
		hval = hash(dname); nents = 1;
	}
	while (nents--) {
		head.next = dtab[hval];
		dpl = &head;
		while ((dp = dpl->next) != NULL)
			if (dname == NULL || !strcmp(dname, dp->name)) {
				dpl->next = dp->next;
				free((char *)dp->arr);
				for (i = 0; i < dp->nd; i++)
					if (dp->dim[i].p != NULL)
						free((char *)dp->dim[i].p);
				freestr(dp->name);
				free((char *)dp);
			} else
				dpl = dp;
		dtab[hval++] = head.next;
	}
}


freepict(pname)			/* free memory associated with pname */
char  *pname;
{
	DATARRAY  head;
	int  hval, nents;
	register DATARRAY  *pp, *ppl;

	if (pname == NULL) {			/* free all if NULL */
		hval = 0; nents = TABSIZ;
	} else {
		hval = hash(pname); nents = 1;
	}
	while (nents--) {
		head.next = ptab[hval];
		ppl = &head;
		while ((pp = ppl->next) != NULL)
			if (pname == NULL || !strcmp(pname, pp->name)) {
				ppl->next = pp->next;
				free((char *)pp[0].arr);
				free((char *)pp[1].arr);
				free((char *)pp[2].arr);
				freestr(pp[0].name);
				free((char *)pp);
			} else
				ppl = pp;
		ptab[hval++] = head.next;
	}
}


double
datavalue(dp, pt)		/* interpolate data value at a point */
register DATARRAY  *dp;
double	*pt;
{
	DATARRAY  sd;
	int  asize;
	int  lower, upper;
	register int  i;
	double	x, y0, y1;
					/* set up dimensions for recursion */
	sd.nd = dp->nd - 1;
	asize = 1;
	for (i = 0; i < sd.nd; i++) {
		sd.dim[i].org = dp->dim[i+1].org;
		sd.dim[i].siz = dp->dim[i+1].siz;
		sd.dim[i].p = dp->dim[i+1].p;
		asize *= sd.dim[i].ne = dp->dim[i+1].ne;
	}
					/* get independent variable */
	if (dp->dim[0].p == NULL) {		/* evenly spaced points */
		x = (pt[0] - dp->dim[0].org)/dp->dim[0].siz;
		x *= (double)(dp->dim[0].ne - 1);
		i = x;
		if (i < 0)
			i = 0;
		else if (i > dp->dim[0].ne - 2)
			i = dp->dim[0].ne - 2;
	} else {				/* unevenly spaced points */
		if (dp->dim[0].siz > 0.0) {
			lower = 0;
			upper = dp->dim[0].ne;
		} else {
			lower = dp->dim[0].ne;
			upper = 0;
		}
		do {
			i = (lower + upper) >> 1;
			if (pt[0] >= dp->dim[0].p[i])
				lower = i;
			else
				upper = i;
		} while (i != (lower + upper) >> 1);
		if (i > dp->dim[0].ne - 2)
			i = dp->dim[0].ne - 2;
		x = i + (pt[0] - dp->dim[0].p[i]) /
				(dp->dim[0].p[i+1] - dp->dim[0].p[i]);
	}
					/* get dependent variable */
	if (dp->nd == 1) {
		y0 = dp->arr[i];
		y1 = dp->arr[i+1];
	} else {
		sd.arr = &dp->arr[i*asize];
		y0 = datavalue(&sd, pt+1);
		sd.arr = &dp->arr[(i+1)*asize];
		y1 = datavalue(&sd, pt+1);
	}
	/*
	 * Extrapolate as far as one division, then
	 * taper off harmonically to zero.
	 */
	if (x > i+2)
		return( (2*y1-y0)/(x-(i-1)) );

	if (x < i-1)
		return( (2*y0-y1)/(i-x) );

	return( y0*((i+1)-x) + y1*(x-i) );
}
