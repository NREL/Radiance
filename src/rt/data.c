/* Copyright (c) 1986 Regents of the University of California */

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

#include  "data.h"


extern char  *libpath;			/* library search path */

static DATARRAY  *dlist = NULL;		/* data array list */

static DATARRAY  *plist = NULL;		/* picture list */


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
	for (dp = dlist; dp != NULL; dp = dp->next)
		if (!strcmp(dname, dp->name))
			return(dp);		/* found! */

	/*
	 *	If we haven't loaded the data already, we will look
	 *  for it in the directorys specified by the library path.
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

	if ((dfname = getpath(dname, libpath, R_OK)) == NULL) {
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
	if (fscanf(fp, "%d", &dp->nd) != 1)
		goto scanerr;
	if (dp->nd <= 0 || dp->nd > MAXDIM) {
		sprintf(errmsg, "bad number of dimensions for \"%s\"", dname);
		error(USER, errmsg);
	}
	asize = 1;
	for (i = 0; i < dp->nd; i++) {
		if (fscanf(fp, "%lf %lf %d",
				&dp->dim[i].org, &dp->dim[i].siz,
				&dp->dim[i].ne) != 3)
			goto scanerr;
		if (dp->dim[i].ne < 2)
			goto scanerr;
		asize *= dp->dim[i].ne;
		if ((dp->dim[i].siz -= dp->dim[i].org) == 0) {
			dp->dim[i].p = (double *)malloc(dp->dim[i].ne*sizeof(double));
			if (dp->dim[i].p == NULL)
				goto memerr;
			for (j = 0; j < dp->dim[i].ne; j++)
				if (fscanf(fp, "%lf", &dp->dim[i].p[j]) != 1)
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
		if (fscanf(fp, DSCANF, &dp->arr[i]) != 1)
			goto scanerr;
	
	fclose(fp);
	dp->next = dlist;
	return(dlist = dp);

memerr:
	error(SYSTEM, "out of memory in getdata");
scanerr:
	sprintf(errmsg, "%s in data file \"%s\"",
			feof(fp) ? "unexpected EOF" : "bad format", dfname);
	error(USER, errmsg);
}


static double  inpaspect;		/* aspect ratio of input picture */

static
headaspect(s)				/* check string for aspect ratio */
char  *s;
{
	if (isaspect(s))
		inpaspect *= aspectval(s);
}


DATARRAY *
getpict(pname)				/* get picture pname */
char  *pname;
{
	extern char  *libpath;
	char  *pfname;
	FILE  *fp;
	COLOR  *scanin;
	int  width, height;
	int  x, y;
	register int  i;
	register DATARRAY  *pp;
						/* look for array in list */
	for (pp = plist; pp != NULL; pp = pp->next)
		if (!strcmp(pname, pp->name))
			return(pp);		/* found! */

	if ((pfname = getpath(pname, libpath, R_OK)) == NULL) {
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
						/* get dimensions */
	inpaspect = 1.0;
	getheader(fp, headaspect);
	if (fgetresolu(&width, &height, fp) != (YMAJOR|YDECR))
		goto readerr;
	for (i = 0; i < 3; i++) {
		pp[i].nd = 2;
		pp[i].dim[0].ne = width;
		pp[i].dim[1].ne = height;
		pp[i].dim[0].org =
		pp[i].dim[1].org = 0.0;
		if (width <= height*inpaspect) {
			pp[i].dim[0].siz = 1.0;
			pp[i].dim[1].siz = inpaspect*(double)height/width;
		} else {
			pp[i].dim[0].siz = (double)width/height/inpaspect;
			pp[i].dim[1].siz = 1.0;
		}
		pp[i].arr = (DATATYPE *)malloc(width*height*sizeof(DATATYPE));
		if (pp[i].arr == NULL)
			goto memerr;
	}
							/* load picture */
	if ((scanin = (COLOR *)malloc(width*sizeof(COLOR))) == NULL)
		goto memerr;
	for (y = height-1; y >= 0; y--) {
		if (freadscan(scanin, width, fp) < 0)
			goto readerr;
		for (x = 0; x < width; x++)
			for (i = 0; i < 3; i++)
				pp[i].arr[x*height+y] = colval(scanin[x],i);
	}
	free((char *)scanin);
	fclose(fp);
	pp[0].next =
	pp[1].next =
	pp[2].next = plist;
	return(plist = pp);

memerr:
	error(SYSTEM, "out of memory in getpict");
readerr:
	sprintf(errmsg, "bad picture file \"%s\"", pfname);
	error(USER, errmsg);
}


freedata(dname)			/* free memory associated with dname */
char  *dname;
{
	register DATARRAY  *dp, *dpl;
	register int  i;

	for (dpl = NULL, dp = dlist; dp != NULL; dpl = dp, dp = dp->next)
		if (!strcmp(dname, dp->name)) {
			if (dpl == NULL)
				dlist = dp->next;
			else
				dpl->next = dp->next;
			free((char *)dp->arr);
			for (i = 0; i < dp->nd; i++)
				if (dp->dim[i].p != NULL)
					free((char *)dp->dim[i].p);
			freestr(dp->name);
			free((char *)dp);
			return;
		}
}


freepict(pname)			/* free memory associated with pname */
char  *pname;
{
	register DATARRAY  *pp, *ppl;

	for (ppl = NULL, pp = plist; pp != NULL; ppl = pp, pp = pp->next)
		if (!strcmp(pname, pp->name)) {
			if (ppl == NULL)
				plist = pp->next;
			else
				ppl->next = pp->next;
			free((char *)pp[0].arr);
			free((char *)pp[1].arr);
			free((char *)pp[2].arr);
			freestr(pp[0].name);
			free((char *)pp);
			return;
		}
}


double
datavalue(dp, pt)		/* interpolate data value at a point */
register DATARRAY  *dp;
double  *pt;
{
	DATARRAY  sd;
	int  asize;
	int  lower, upper;
	register int  i;
	double  x, y, y0, y1;
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
		x = x * (dp->dim[0].ne - 1);
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
			else if (pt[0] < dp->dim[0].p[i])
				upper = i;
		} while (i != (lower + upper) >> 1);
		if (i < 0)
			i = 0;
		else if (i > dp->dim[0].ne - 2)
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
		y = (2*y1-y0)/(x-i-1);
	else if (x < i-1)
		y = (2*y0-y1)/(i-x);
	else
		y = y0*((i+1)-x) + y1*(x-i);

	return(y);
}
