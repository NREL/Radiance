/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  image.c - routines for image generation.
 *
 *     10/17/85
 */

#include  "standard.h"

#include  "color.h"

#include  "view.h"

VIEW  stdview = STDVIEW(512);		/* default view parameters */


bigdiff(c1, c2, md)			/* c1 delta c2 > md? */
register COLOR  c1, c2;
double  md;
{
	register int  i;

	for (i = 0; i < 3; i++)
		if (colval(c1,i)-colval(c2,i) > md*colval(c2,i) ||
			colval(c2,i)-colval(c1,i) > md*colval(c1,i))
			return(1);
	return(0);
}


char *
setview(v)		/* set vhinc and vvinc, return message on error */
register VIEW  *v;
{
	double  tan(), dt;
	
	if (normalize(v->vdir) == 0.0)		/* normalize direction */
		return("zero view direction");
		
	fcross(v->vhinc, v->vdir, v->vup);	/* compute horiz dir */

	if (normalize(v->vhinc) == 0.0)
		return("illegal view up vector");
		
	fcross(v->vvinc, v->vhinc, v->vdir);	/* compute vert dir */

	if (v->type == VT_PAR)
		dt =  v->horiz;
	else if (v->type == VT_PER)
		dt = 2.0 * tan(v->horiz*(PI/180.0/2.0));
	else
		return("unknown view type");

	if (dt <= FTINY || dt >= FHUGE)
		return("illegal horizontal view size");

	if (v->hresolu <= 0)
		return("illegal horizontal resolution");

	dt /= (double)v->hresolu;
	v->vhinc[0] *= dt;
	v->vhinc[1] *= dt;
	v->vhinc[2] *= dt;

	if (v->type == VT_PAR)
		dt = v->vert;
	else
		dt = 2.0 * tan(v->vert*(PI/180.0/2.0));

	if (dt <= FTINY || dt >= FHUGE)
		return("illegal vertical view size");

	if (v->vresolu <= 0)
		return("illegal vertical resolution");
	
	dt /= (double)v->vresolu;
	v->vvinc[0] *= dt;
	v->vvinc[1] *= dt;
	v->vvinc[2] *= dt;

	return(NULL);
}


rayview(orig, direc, v, x, y)		/* compute ray origin and direction */
FVECT  orig, direc;
register VIEW  *v;
double  x, y;
{
	register int  i;

	x -= 0.5 * v->hresolu;
	y -= 0.5 * v->vresolu;

	if (v->type == VT_PAR) {	/* parallel view */
		for (i = 0; i < 3; i++)
			orig[i] = v->vp[i] + x*v->vhinc[i] + y*v->vvinc[i];
		VCOPY(direc, v->vdir);
	} else {			/* perspective view */
		VCOPY(orig, v->vp);
		for (i = 0; i < 3; i++)
			direc[i] = v->vdir[i] + x*v->vhinc[i] + y*v->vvinc[i];
		normalize(direc);
	}
}


sscanview(vp, s)			/* get view parameters */
register VIEW  *vp;
register char  *s;
{
	for ( ; ; )
		switch (*s++) {
		case '\0':
		case '\n':
			return(0);
		case '-':
			switch (*s++) {
			case '\0':
				return(-1);
			case 'x':
				if (sscanf(s, "%d", &vp->hresolu) != 1)
					return(-1);
				s = sskip(s);
				continue;
			case 'y':
				if (sscanf(s, "%d", &vp->vresolu) != 1)
					return(-1);
				s = sskip(s);
				continue;
			case 'v':
				switch (*s++) {
				case '\0':
					return(-1);
				case 't':
					vp->type = *s++;
					continue;
				case 'p':
					if (sscanf(s, "%lf %lf %lf",
							&vp->vp[0],
							&vp->vp[1],
							&vp->vp[2]) != 3)
						return(-1);
					s = sskip(sskip(sskip(s)));
					continue;
				case 'd':
					if (sscanf(s, "%lf %lf %lf",
							&vp->vdir[0],
							&vp->vdir[1],
							&vp->vdir[2]) != 3)
						return(-1);
					s = sskip(sskip(sskip(s)));
					continue;
				case 'u':
					if (sscanf(s, "%lf %lf %lf",
							&vp->vup[0],
							&vp->vup[1],
							&vp->vup[2]) != 3)
						return(-1);
					s = sskip(sskip(sskip(s)));
					continue;
				case 'h':
					if (sscanf(s, "%lf",
							&vp->horiz) != 1)
						return(-1);
					s = sskip(s);
					continue;
				case 'v':
					if (sscanf(s, "%lf",
							&vp->vert) != 1)
						return(-1);
					s = sskip(s);
					continue;
				}
				continue;
			}
			continue;
		}
}


fprintview(vp, fp)			/* write out view parameters */
register VIEW  *vp;
FILE  *fp;
{
	fprintf(fp, " -vt%c", vp->type);
	fprintf(fp, " -vp %.6g %.6g %.6g", vp->vp[0], vp->vp[1], vp->vp[2]);
	fprintf(fp, " -vd %.6g %.6g %.6g", vp->vdir[0], vp->vdir[1], vp->vdir[2]);
	fprintf(fp, " -vu %.6g %.6g %.6g", vp->vup[0], vp->vup[1], vp->vup[2]);
	fprintf(fp, " -vh %.6g -vv %.6g", vp->horiz, vp->vert);
	fprintf(fp, " -x %d -y %d", vp->hresolu, vp->vresolu);
}


static VIEW  *hview;			/* view from header */
static int  gothview;			/* success indicator */
static char  *altname[] = {NULL,"rpict","rview",VIEWSTR,NULL};


static
gethview(s)				/* get view from header */
char  *s;
{
	register char  **an;

	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, s, strlen(*an))) {
			if (sscanview(hview, s+strlen(*an)) == 0)
				gothview++;
			return;
		}
}


int
viewfile(fname, vp)			/* get view from file */
char  *fname;
VIEW  *vp;
{
	extern char  *progname;
	FILE  *fp;

	if ((fp = fopen(fname, "r")) == NULL)
		return(-1);

	altname[0] = progname;
	hview = vp;
	gothview = 0;

	getheader(fp, gethview);

	fclose(fp);

	return(gothview);
}
