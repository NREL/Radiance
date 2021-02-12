#ifndef lint
static const char	RCSid[] = "$Id: image.c,v 2.52 2021/02/12 00:47:08 greg Exp $";
#endif
/*
 *  image.c - routines for image generation.
 *
 *  External symbols declared in view.h
 */

#include "copyright.h"

#include  <ctype.h>
#include  "rtio.h"
#include  "rtmath.h"
#include  "paths.h"
#include  "view.h"

VIEW  stdview = STDVIEW;		/* default view parameters */

static gethfunc gethview;


char *
setview(		/* set hvec and vvec, return message on error */
VIEW  *v
)
{
	static char  ill_horiz[] = "illegal horizontal view size";
	static char  ill_vert[] = "illegal vertical view size";
	
	if ((v->vfore < -FTINY) | (v->vaft < -FTINY) ||
			(v->vaft > FTINY) & (v->vaft <= v->vfore))
		return("illegal fore/aft clipping plane");

	if (v->vdist <= FTINY)
		return("illegal view distance");
	v->vdist *= normalize(v->vdir);		/* normalize direction */
	if (v->vdist == 0.0)
		return("zero view direction");

	if (normalize(v->vup) == 0.0)		/* normalize view up */
		return("zero view up vector");

	fcross(v->hvec, v->vdir, v->vup);	/* compute horiz dir */

	if (normalize(v->hvec) == 0.0)
		return("view up parallel to view direction");

	fcross(v->vvec, v->hvec, v->vdir);	/* compute vert dir */

	if (v->horiz <= FTINY)
		return(ill_horiz);
	if (v->vert <= FTINY)
		return(ill_vert);

	switch (v->type) {
	case VT_PAR:				/* parallel view */
		v->hn2 = v->horiz;
		v->vn2 = v->vert;
		break;
	case VT_PER:				/* perspective view */
		if (v->horiz >= 180.0-FTINY)
			return(ill_horiz);
		if (v->vert >= 180.0-FTINY)
			return(ill_vert);
		v->hn2 = 2.0 * tan(v->horiz*(PI/180.0/2.0));
		v->vn2 = 2.0 * tan(v->vert*(PI/180.0/2.0));
		break;
	case VT_CYL:				/* cylindrical panorama */
		if (v->horiz > 360.0+FTINY)
			return(ill_horiz);
		if (v->vert >= 180.0-FTINY)
			return(ill_vert);
		v->hn2 = v->horiz * (PI/180.0);
		v->vn2 = 2.0 * tan(v->vert*(PI/180.0/2.0));
		break;
	case VT_ANG:				/* angular fisheye */
		if (v->horiz > 360.0+FTINY)
			return(ill_horiz);
		if (v->vert > 360.0+FTINY)
			return(ill_vert);
		v->hn2 = v->horiz * (PI/180.0);
		v->vn2 = v->vert * (PI/180.0);
		break;
	case VT_HEM:				/* hemispherical fisheye */
		if (v->horiz > 180.0+FTINY)
			return(ill_horiz);
		if (v->vert > 180.0+FTINY)
			return(ill_vert);
		v->hn2 = 2.0 * sin(v->horiz*(PI/180.0/2.0));
		v->vn2 = 2.0 * sin(v->vert*(PI/180.0/2.0));
		break;
	case VT_PLS:				/* planispheric fisheye */
		if (v->horiz >= 360.0-FTINY)
			return(ill_horiz);
		if (v->vert >= 360.0-FTINY)
			return(ill_vert);
		v->hn2 = 2.*sin(v->horiz*(PI/180.0/2.0)) /
				(1.0 + cos(v->horiz*(PI/180.0/2.0)));
		v->vn2 = 2.*sin(v->vert*(PI/180.0/2.0)) /
				(1.0 + cos(v->vert*(PI/180.0/2.0)));
		break;
	default:
		return("unknown view type");
	}
	if (v->type != VT_ANG && v->type != VT_PLS) {
		if (v->type != VT_CYL) {
			v->hvec[0] *= v->hn2;
			v->hvec[1] *= v->hn2;
			v->hvec[2] *= v->hn2;
		}
		v->vvec[0] *= v->vn2;
		v->vvec[1] *= v->vn2;
		v->vvec[2] *= v->vn2;
	}
	v->hn2 *= v->hn2;
	v->vn2 *= v->vn2;

	return(NULL);
}


void
normaspect(				/* fix pixel aspect or resolution */
double  va,			/* view aspect ratio */
double  *ap,			/* pixel aspect in (or out if 0) */
int  *xp,
int  *yp			/* x and y resolution in (or out if *ap!=0) */
)
{
	if (*ap <= FTINY)
		*ap = va * *xp / *yp;		/* compute pixel aspect */
	else if (va * *xp > *ap * *yp)
		*xp = *yp / va * *ap + .5;	/* reduce x resolution */
	else
		*yp = *xp * va / *ap + .5;	/* reduce y resolution */
}


double
viewray(				/* compute ray origin and direction */
FVECT  orig,
FVECT  direc,
VIEW  *v,
double  x,
double  y
)
{
	double	d, z;
	
	x += v->hoff - 0.5;
	y += v->voff - 0.5;

	switch(v->type) {
	case VT_PAR:			/* parallel view */
		orig[0] = v->vp[0] + v->vfore*v->vdir[0]
				+ x*v->hvec[0] + y*v->vvec[0];
		orig[1] = v->vp[1] + v->vfore*v->vdir[1]
				+ x*v->hvec[1] + y*v->vvec[1];
		orig[2] = v->vp[2] + v->vfore*v->vdir[2]
				+ x*v->hvec[2] + y*v->vvec[2];
		VCOPY(direc, v->vdir);
		return(v->vaft > FTINY ? v->vaft - v->vfore : 0.0);
	case VT_PER:			/* perspective view */
		direc[0] = v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		VSUM(orig, v->vp, direc, v->vfore);
		d = normalize(direc);
		return(v->vaft > FTINY ? (v->vaft - v->vfore)*d : 0.0);
	case VT_HEM:			/* hemispherical fisheye */
		z = 1.0 - x*x*v->hn2 - y*y*v->vn2;
		if (z < 0.0)
			return(-1.0);
		z = sqrt(z);
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		VSUM(orig, v->vp, direc, v->vfore);
		return(v->vaft > FTINY ? v->vaft - v->vfore : 0.0);
	case VT_CYL:			/* cylindrical panorama */
		d = x * v->horiz * (PI/180.0);
		z = cos(d);
		x = sin(d);
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		VSUM(orig, v->vp, direc, v->vfore);
		d = normalize(direc);
		return(v->vaft > FTINY ? (v->vaft - v->vfore)*d : 0.0);
	case VT_ANG:			/* angular fisheye */
		x *= (1.0/180.0)*v->horiz;
		y *= (1.0/180.0)*v->vert;
		d = x*x + y*y;
		if (d > 1.0)
			return(-1.0);
		d = sqrt(d);
		z = cos(PI*d);
		d = d <= FTINY ? PI : sqrt(1.0 - z*z)/d;
		x *= d;
		y *= d;
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		VSUM(orig, v->vp, direc, v->vfore);
		return(v->vaft > FTINY ? v->vaft - v->vfore : 0.0);
	case VT_PLS:			/* planispheric fisheye */
		x *= sqrt(v->hn2);
		y *= sqrt(v->vn2);
		d = x*x + y*y;
		z = (1. - d)/(1. + d);
		x *= (1. + z);
		y *= (1. + z);
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		VSUM(orig, v->vp, direc, v->vfore);
		return(v->vaft > FTINY ? v->vaft - v->vfore : 0.0);
	}
	return(-1.0);
}


int
viewloc(			/* find image location for point */
FVECT  ip,
VIEW  *v,
FVECT  p
)	/* Use VL_* flags to interpret return value */
{
	int	rflags = VL_GOOD;
	double  d, d2;
	FVECT  disp;

	VSUB(disp, p, v->vp);

	switch (v->type) {
	case VT_PAR:			/* parallel view */
		ip[2] = DOT(disp,v->vdir) - v->vfore;
		break;
	case VT_PER:			/* perspective view */
		d = DOT(disp,v->vdir);
		if ((v->vaft > FTINY) & (d >= v->vaft))
			rflags |= VL_BEYOND;
		ip[2] = VLEN(disp);
		if (d < -FTINY) {	/* fold pyramid */
			ip[2] = -ip[2];
			d = -d;
		} else if (d <= FTINY)
			return(VL_BAD);	/* at infinite edge */
		d = 1.0/d;
		disp[0] *= d;
		disp[1] *= d;
		disp[2] *= d;
		if (ip[2] < 0.0) d = -d;
		ip[2] *= (1.0 - v->vfore*d);
		break;
	case VT_HEM:			/* hemispherical fisheye */
		d = normalize(disp);
		if (DOT(disp,v->vdir) < 0.0)
			ip[2] = -d;
		else
			ip[2] = d;
		ip[2] -= v->vfore;
		break;
	case VT_CYL:			/* cylindrical panorama */
		d = DOT(disp,v->hvec);
		d2 = DOT(disp,v->vdir);
		ip[0] = 180.0/PI * atan2(d,d2) / v->horiz + 0.5 - v->hoff;
		d2 = d*d + d2*d2;
		if (d2 <= FTINY*FTINY)
			return(VL_BAD);	/* at pole */
		if ((v->vaft > FTINY) & (d2 >= v->vaft*v->vaft))
			rflags |= VL_BEYOND;
		d = 1.0/sqrt(d2);
		ip[1] = DOT(disp,v->vvec)*d/v->vn2 + 0.5 - v->voff;
		ip[2] = VLEN(disp);
		ip[2] *= (1.0 - v->vfore*d);
		goto gotall;
	case VT_ANG:			/* angular fisheye */
		ip[0] = 0.5 - v->hoff;
		ip[1] = 0.5 - v->voff;
		ip[2] = normalize(disp) - v->vfore;
		d = DOT(disp,v->vdir);
		if (d >= 1.0-FTINY)
			goto gotall;
		if (d <= -(1.0-FTINY)) {
			ip[0] += 180.0/v->horiz;
			goto gotall;
		}
		d = (180.0/PI)*acos(d) / sqrt(1.0 - d*d);
		ip[0] += DOT(disp,v->hvec)*d/v->horiz;
		ip[1] += DOT(disp,v->vvec)*d/v->vert;
		goto gotall;
	case VT_PLS:			/* planispheric fisheye */
		ip[0] = 0.5 - v->hoff;
		ip[1] = 0.5 - v->voff;
		ip[2] = normalize(disp) - v->vfore;
		d = DOT(disp,v->vdir);
		if (d >= 1.0-FTINY)
			goto gotall;
		if (d <= -(1.0-FTINY))
			return(VL_BAD);
		ip[0] += DOT(disp,v->hvec)/((1. + d)*sqrt(v->hn2));
		ip[1] += DOT(disp,v->vvec)/((1. + d)*sqrt(v->vn2));
		goto gotall;
	default:
		return(VL_BAD);
	}
	ip[0] = DOT(disp,v->hvec)/v->hn2 + 0.5 - v->hoff;
	ip[1] = DOT(disp,v->vvec)/v->vn2 + 0.5 - v->voff;
gotall:					/* add appropriate return flags */
	if (ip[2] <= 0.0)
		rflags |= VL_BEHIND;
	else if ((v->type != VT_PER) & (v->type != VT_CYL))
		rflags |= VL_BEYOND*((v->vaft > FTINY) &
					(ip[2] >= v->vaft - v->vfore));
	rflags |= VL_OUTSIDE*((0.0 >= ip[0]) | (ip[0] >= 1.0) |
				(0.0 >= ip[1]) | (ip[1] >= 1.0));
	return(rflags);
}


void
pix2loc(		/* compute image location from pixel pos. */
RREAL  loc[2],
RESOLU  *rp,
int  px,
int  py
)
{
	int  x, y;

	if (rp->rt & YMAJOR) {
		x = px;
		y = py;
	} else {
		x = py;
		y = px;
	}
	if (rp->rt & XDECR)
		x = rp->xr-1 - x;
	if (rp->rt & YDECR)
		y = rp->yr-1 - y;
	loc[0] = (x+.5)/rp->xr;
	loc[1] = (y+.5)/rp->yr;
}


void
loc2pix(			/* compute pixel pos. from image location */
int  pp[2],
RESOLU  *rp,
double  lx,
double  ly
)
{
	int  x, y;

	x = (int)(lx*rp->xr + .5 - (lx < 0.0));
	y = (int)(ly*rp->yr + .5 - (ly < 0.0));

	if (rp->rt & XDECR)
		x = rp->xr-1 - x;
	if (rp->rt & YDECR)
		y = rp->yr-1 - y;
	if (rp->rt & YMAJOR) {
		pp[0] = x;
		pp[1] = y;
	} else {
		pp[0] = y;
		pp[1] = x;
	}
}


int
getviewopt(				/* process view argument */
VIEW  *v,
int  ac,
char  *av[]
)
{
#define check(c,l)	if ((av[0][c]&&!isspace(av[0][c])) || \
			badarg(ac-1,av+1,l)) return(-1)

	if (ac <= 0 || av[0][0] != '-' || av[0][1] != 'v')
		return(-1);
	switch (av[0][2]) {
	case 't':			/* type */
		if (!av[0][3] || isspace(av[0][3]))
			return(-1);
		check(4,"");
		v->type = av[0][3];
		return(0);
	case 'p':			/* point */
		check(3,"fff");
		v->vp[0] = atof(av[1]);
		v->vp[1] = atof(av[2]);
		v->vp[2] = atof(av[3]);
		return(3);
	case 'd':			/* direction */
		check(3,"fff");
		v->vdir[0] = atof(av[1]);
		v->vdir[1] = atof(av[2]);
		v->vdir[2] = atof(av[3]);
		v->vdist = 1.;
		return(3);
	case 'u':			/* up */
		check(3,"fff");
		v->vup[0] = atof(av[1]);
		v->vup[1] = atof(av[2]);
		v->vup[2] = atof(av[3]);
		return(3);
	case 'h':			/* horizontal size */
		check(3,"f");
		v->horiz = atof(av[1]);
		return(1);
	case 'v':			/* vertical size */
		check(3,"f");
		v->vert = atof(av[1]);
		return(1);
	case 'o':			/* fore clipping plane */
		check(3,"f");
		v->vfore = atof(av[1]);
		return(1);
	case 'a':			/* aft clipping plane */
		check(3,"f");
		v->vaft = atof(av[1]);
		return(1);
	case 's':			/* shift */
		check(3,"f");
		v->hoff = atof(av[1]);
		return(1);
	case 'l':			/* lift */
		check(3,"f");
		v->voff = atof(av[1]);
		return(1);
	default:
		return(-1);
	}
#undef check
}


int
sscanview(				/* get view parameters from string */
VIEW  *vp,
char  *s
)
{
	int  ac;
	char  *av[4];
	int  na;
	int  nvopts = 0;

	while (isspace(*s))
		if (!*s++)
			return(0);
	while (*s) {
		ac = 0;
		do {
			if (ac || *s == '-')
				av[ac++] = s;
			while (*s && !isspace(*s))
				s++;
			while (isspace(*s))
				s++;
		} while (*s && ac < 4);
		if ((na = getviewopt(vp, ac, av)) >= 0) {
			if (na+1 < ac)
				s = av[na+1];
			nvopts++;
		} else if (ac > 1)
			s = av[1];
	}
	return(nvopts);
}


void
fprintview(				/* write out view parameters */
VIEW  *vp,
FILE  *fp
)
{
	fprintf(fp, " -vt%c", vp->type);
	fprintf(fp, " -vp %.6g %.6g %.6g", vp->vp[0], vp->vp[1], vp->vp[2]);
	fprintf(fp, " -vd %.6g %.6g %.6g", vp->vdir[0]*vp->vdist,
						vp->vdir[1]*vp->vdist,
						vp->vdir[2]*vp->vdist);
	fprintf(fp, " -vu %.6g %.6g %.6g", vp->vup[0], vp->vup[1], vp->vup[2]);
	fprintf(fp, " -vh %.6g -vv %.6g", vp->horiz, vp->vert);
	fprintf(fp, " -vo %.6g -va %.6g", vp->vfore, vp->vaft);
	fprintf(fp, " -vs %.6g -vl %.6g", vp->hoff, vp->voff);
}


char *
viewopt(				/* translate to minimal view string */
VIEW  *vp
)
{
	static char  vwstr[128];
	char  *cp = vwstr;

	*cp = '\0';
	if (vp->type != stdview.type) {
		sprintf(cp, " -vt%c", vp->type);
		cp += strlen(cp);
	}
	if (!VABSEQ(vp->vp,stdview.vp)) {
		sprintf(cp, " -vp %.6g %.6g %.6g",
				vp->vp[0], vp->vp[1], vp->vp[2]);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->vdist,stdview.vdist) || !VABSEQ(vp->vdir,stdview.vdir)) {
		sprintf(cp, " -vd %.6g %.6g %.6g",
				vp->vdir[0]*vp->vdist,
				vp->vdir[1]*vp->vdist,
				vp->vdir[2]*vp->vdist);
		cp += strlen(cp);
	}
	if (!VABSEQ(vp->vup,stdview.vup)) {
		sprintf(cp, " -vu %.6g %.6g %.6g",
				vp->vup[0], vp->vup[1], vp->vup[2]);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->horiz,stdview.horiz)) {
		sprintf(cp, " -vh %.6g", vp->horiz);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->vert,stdview.vert)) {
		sprintf(cp, " -vv %.6g", vp->vert);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->vfore,stdview.vfore)) {
		sprintf(cp, " -vo %.6g", vp->vfore);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->vaft,stdview.vaft)) {
		sprintf(cp, " -va %.6g", vp->vaft);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->hoff,stdview.hoff)) {
		sprintf(cp, " -vs %.6g", vp->hoff);
		cp += strlen(cp);
	}
	if (!FABSEQ(vp->voff,stdview.voff)) {
		sprintf(cp, " -vl %.6g", vp->voff);
		cp += strlen(cp);
	}
	return(vwstr);
}


int
isview(					/* is this a view string? */
char  *s
)
{
	static char  *altname[]={NULL,VIEWSTR,"rpict","rview","rvu","rpiece","pinterp",NULL};
	extern char  *progname;
	char  *cp;
	char  **an;
					/* add program name to list */
	if (altname[0] == NULL) {
		for (cp = progname; *cp; cp++)
			;
		while (cp > progname && !ISDIRSEP(cp[-1]))
			cp--;
		altname[0] = cp;
	}
					/* skip leading path */
	cp = s;
	while (*cp && !isspace(*cp))
		cp++;
	while (cp > s && !ISDIRSEP(cp[-1]))
		cp--;
	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, cp, strlen(*an)))
			return(1);
	return(0);
}


struct myview {
	VIEW	*hv;
	int	ok;
};


static int
gethview(				/* get view from header */
	char  *s,
	void  *v
)
{
	if (isview(s) && sscanview(((struct myview*)v)->hv, s) > 0)
		((struct myview*)v)->ok++;
	return(0);
}


int
viewfile(				/* get view from file */
char  *fname,
VIEW  *vp,
RESOLU  *rp
)
{
	struct myview	mvs;
	FILE  *fp;

	if (fname == NULL || !strcmp(fname, "-"))
		fp = stdin;
	else if ((fp = fopen(fname, "r")) == NULL)
		return(-1);

	mvs.hv = vp;
	mvs.ok = 0;

	getheader(fp, gethview, &mvs);

	if (rp != NULL && !fgetsresolu(rp, fp))
		mvs.ok = 0;

	if (fp != stdin)
		fclose(fp);

	return(mvs.ok);
}
