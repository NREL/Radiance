/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  image.c - routines for image generation.
 *
 *     10/17/85
 */

#include  <ctype.h>

#include  "standard.h"

#include  "view.h"

VIEW  stdview = STDVIEW(512);		/* default view parameters */


char *
sskip(s)		/* skip a word */
register char  *s;
{
	while (isspace(*s)) s++;
	while (*s && !isspace(*s)) s++;
	return(s);
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

	v->vhs2 = 1.0/DOT(v->vhinc,v->vhinc);
	v->vvs2 = 1.0/DOT(v->vvinc,v->vvinc);

	return(NULL);
}


rayview(orig, direc, v, x, y)		/* compute ray origin and direction */
FVECT  orig, direc;
register VIEW  *v;
double  x, y;
{
	x -= 0.5 * v->hresolu;
	y -= 0.5 * v->vresolu;

	if (v->type == VT_PAR) {	/* parallel view */
		orig[0] = v->vp[0] + x*v->vhinc[0] + y*v->vvinc[0];
		orig[1] = v->vp[1] + x*v->vhinc[1] + y*v->vvinc[1];
		orig[2] = v->vp[2] + x*v->vhinc[2] + y*v->vvinc[2];
		VCOPY(direc, v->vdir);
	} else {			/* perspective view */
		VCOPY(orig, v->vp);
		direc[0] = v->vdir[0] + x*v->vhinc[0] + y*v->vvinc[0];
		direc[1] = v->vdir[1] + x*v->vhinc[1] + y*v->vvinc[1];
		direc[2] = v->vdir[2] + x*v->vhinc[2] + y*v->vvinc[2];
		normalize(direc);
	}
}


pixelview(xp, yp, zp, v, p)		/* find image location for point */
double  *xp, *yp, *zp;
register VIEW  *v;
FVECT  p;
{
	extern double  sqrt();
	double  d;
	FVECT  disp;
	
	disp[0] = p[0] - v->vp[0];
	disp[1] = p[1] - v->vp[1];
	disp[2] = p[2] - v->vp[2];

	if (v->type == VT_PAR) {	/* parallel view */
		if (zp != NULL)
			*zp = DOT(disp,v->vdir);
	} else {			/* perspective view */
		d = 1.0/DOT(disp,v->vdir);
		if (zp != NULL) {
			*zp = sqrt(DOT(disp,disp));
			if (d < 0.0)
				*zp = -*zp;
		}
		disp[0] *= d;
		disp[1] *= d;
		disp[2] *= d;
	}
	*xp = DOT(disp,v->vhinc)*v->vhs2 + 0.5*v->hresolu;
	*yp = DOT(disp,v->vvinc)*v->vvs2 + 0.5*v->vresolu;
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
static char  *altname[] = {NULL,"rpict","rview","pinterp",VIEWSTR,NULL};


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
