/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for veiling glare and loss of acuity.
 */

#include "pcond.h"

/************** VEILING STUFF *****************/

#define	VADAPT		0.08		/* fraction of adaptation from veil */

extern COLOR	*fovimg;		/* foveal (1 degree) averaged image */
extern short	fvxr, fvyr;		/* foveal image resolution */

#define fovscan(y)	(fovimg+(y)*fvxr)

static COLOR	*veilimg;		/* veiling image */

#define veilscan(y)	(veilimg+(y)*fvxr)

static float	(*raydir)[3] = NULL;	/* ray direction for each pixel */

#define rdirscan(y)	(raydir+(y)*fvxr)


compraydir()				/* compute ray directions */
{
	FVECT	rorg, rdir;
	double	h, v;
	register int	x, y;

	if (raydir != NULL)		/* already done? */
		return;
	raydir = (float (*)[3])malloc(fvxr*fvyr*3*sizeof(float));
	if (raydir == NULL)
		syserror("malloc");

	for (y = 0; y < fvyr; y++) {
		switch (inpres.or) {
		case YMAJOR: case YMAJOR|XDECR:
			v = (y+.5)/fvyr; break;
		case YMAJOR|YDECR: case YMAJOR|YDECR|XDECR:
			v = 1. - (y+.5)/fvyr; break;
		case 0: case YDECR:
			h = (y+.5)/fvyr; break;
		case XDECR: case XDECR|YDECR:
			h = 1. - (y+.5)/fvyr; break;
		}
		for (x = 0; x < fvxr; x++) {
			switch (inpres.or) {
			case YMAJOR: case YMAJOR|YDECR:
				h = (x+.5)/fvxr; break;
			case YMAJOR|XDECR: case YMAJOR|XDECR|YDECR:
				h = 1. - (x+.5)/fvxr; break;
			case 0: case XDECR:
				v = (x+.5)/fvxr; break;
			case YDECR: case YDECR|XDECR:
				v = 1. - (x+.5)/fvxr; break;
			}
			if (viewray(rorg, rdir, &ourview, h, v)
					>= -FTINY) {
				rdirscan(y)[x][0] = rdir[0];
				rdirscan(y)[x][1] = rdir[1];
				rdirscan(y)[x][2] = rdir[2];
			} else {
				rdirscan(y)[x][0] =
				rdirscan(y)[x][1] =
				rdirscan(y)[x][2] = 0.0;
			}
		}
	}
}


compveil()				/* compute veiling image */
{
	double	t2, t2sum;
	COLOR	ctmp, vsum;
	int	px, py;
	register int	x, y;
					/* compute ray directions */
	compraydir();
					/* compute veil image */
	veilimg = (COLOR *)malloc(fvxr*fvyr*sizeof(COLOR));
	if (veilimg == NULL)
		syserror("malloc");
	for (py = 0; py < fvyr; py++)
		for (px = 0; px < fvxr; px++) {
			t2sum = 0.;
			setcolor(vsum, 0., 0., 0.);
			for (y = 0; y < fvyr; y++)
				for (x = 0; x < fvxr; x++) {
					if (x == px && y == py) continue;
					t2 = DOT(rdirscan(py)[px],
							rdirscan(y)[x]);
					if (t2 <= FTINY) continue;
					/*	use approximation instead
					t2 = acos(t2);
					t2 = 1./(t2*t2);
					*/
					t2 = .5 / (1. - t2);
					copycolor(ctmp, fovscan(y)[x]);
					scalecolor(ctmp, t2);
					addcolor(vsum, ctmp);
					t2sum += t2;
				}
			/* VADAPT of original is subtracted in addveil() */
			scalecolor(vsum, VADAPT/t2sum);
			copycolor(veilscan(py)[px], vsum);
		}
}


addveil(sl, y)				/* add veil to scanline */
COLOR	*sl;
int	y;
{
	int	vx, vy;
	double	dx, dy;
	double	lv, uv;
	register int	x, i;

	vy = dy = (y+.5)/numscans(&inpres)*fvyr - .5;
	while (vy >= fvyr-1) vy--;
	dy -= (double)vy;
	for (x = 0; x < scanlen(&inpres); x++) {
		vx = dx = (x+.5)/scanlen(&inpres)*fvxr - .5;
		while (vx >= fvxr-1) vx--;
		dx -= (double)vx;
		for (i = 0; i < 3; i++) {
			lv = (1.-dy)*colval(veilscan(vy)[vx],i) +
					dy*colval(veilscan(vy+1)[vx],i);
			uv = (1.-dy)*colval(veilscan(vy)[vx+1],i) +
					dy*colval(veilscan(vy+1)[vx+1],i);
			colval(sl[x],i) = (1.-VADAPT)*colval(sl[x],i) +
					(1.-dx)*lv + dx*uv;
		}
	}
}


/****************** ACUITY STUFF *******************/

typedef struct {
	short	sampe;		/* sample area size (exponent of 2) */
	short	nscans;		/* number of scanlines in this bar */
	int	len;		/* individual scanline length */
	int	nread;		/* number of scanlines loaded */
	COLOR	*sdata;		/* scanbar data */
} SCANBAR;

#define bscan(sb,y)	((COLOR *)(sb)->sdata+((y)%(sb)->nscans)*(sb)->len)

SCANBAR	*rootbar;		/* root scan bar (lowest resolution) */

float	*inpacuD;		/* input acuity data (cycles/degree) */

#define tsampr(x,y)	inpacuD[(y)*fvxr+(x)]


double
hacuity(La)			/* return visual acuity in cycles/degree */
double	La;
{				/* data due to S. Shaler (we should fit it!) */
#define NPOINTS	20
	static float	l10lum[NPOINTS] = {
		-3.10503,-2.66403,-2.37703,-2.09303,-1.64403,-1.35803,
		-1.07403,-0.67203,-0.38503,-0.10103,0.29397,0.58097,0.86497,
		1.25697,1.54397,1.82797,2.27597,2.56297,2.84697,3.24897
	};
	static float	resfreq[NPOINTS] = {
		2.09,3.28,3.79,4.39,6.11,8.83,10.94,18.66,23.88,31.05,37.42,
		37.68,41.60,43.16,45.30,47.00,48.43,48.32,51.06,51.09
	};
	double	l10La;
	register int	i;
					/* check limits */
	if (La <= 7.85e-4)
		return(resfreq[0]);
	if (La >= 1.78e3)
		return(resfreq[NPOINTS-1]);
					/* interpolate data */
	l10La = log10(La);
	for (i = 0; i < NPOINTS-2 && l10lum[i+1] <= l10La; i++)
		;
	return( ( (l10lum[i+1] - l10La)*resfreq[i] +
			(l10La - l10lum[i])*resfreq[i+1] ) /
			(l10lum[i+1] - l10lum[i]) );
#undef NPOINTS
}


COLOR *
getascan(sb, y)			/* find/read scanline y for scanbar sb */
register SCANBAR	*sb;
int	y;
{
	register COLOR	*sl0, *sl1, *mysl;
	register int	i;

	if (y < sb->nread - sb->nscans)			/* too far back? */
		return(NULL);
	for ( ; y >= sb->nread; sb->nread++) {		/* read as necessary */
		mysl = bscan(sb, sb->nread);
		if (sb->sampe == 0) {
			if (freadscan(mysl, sb->len, infp) < 0) {
				fprintf(stderr, "%s: %s: scanline read error\n",
						progname, infn);
				exit(1);
			}
		} else {
			sl0 = getascan(sb+1, 2*y);
			if (sl0 == NULL)
				return(NULL);
			sl1 = getascan(sb+1, 2*y+1);
			for (i = 0; i < sb->len; i++) {
				copycolor(mysl[i], sl0[2*i]);
				addcolor(mysl[i], sl0[2*i+1]);
				addcolor(mysl[i], sl1[2*i]);
				addcolor(mysl[i], sl1[2*i+1]);
				scalecolor(mysl[i], 0.25);
			}
		}
	}
	return(bscan(sb, y));
}


acuscan(scln, y)		/* get acuity-sampled scanline */
COLOR	*scln;
int	y;
{
	double	sr;
	double	dx, dy;
	int	ix, iy;
	register int	x;
					/* compute foveal y position */
	iy = dy = (y+.5)/numscans(&inpres)*fvyr - .5;
	while (iy >= fvyr-1) iy--;
	dy -= (double)iy;
	for (x = 0; x < scanlen(&inpres); x++) {
					/* compute foveal x position */
		ix = dx = (x+.5)/scanlen(&inpres)*fvxr - .5;
		while (ix >= fvxr-1) ix--;
		dx -= (double)ix;
					/* interpolate sample rate */
		sr = (1.-dy)*((1.-dx)*tsampr(ix,iy) + dx*tsampr(ix+1,iy)) +
			dy*((1.-dx)*tsampr(ix,iy+1) + dx*tsampr(ix+1,iy+1));

		acusample(scln[x], x, y, sr);	/* compute sample */
	}
}


acusample(col, x, y, sr)	/* interpolate sample at (x,y) using rate sr */
COLOR	col;
int	x, y;
double	sr;
{
	COLOR	c1;
	double	d;
	register SCANBAR	*sb0;

	for (sb0 = rootbar; sb0->sampe != 0 && 1<<sb0[1].sampe > sr; sb0++)
		;
	ascanval(col, x, y, sb0);
	if (sb0->sampe == 0)		/* don't extrapolate highest */
		return;
	ascanval(c1, x, y, sb0+1);
	d = ((1<<sb0->sampe) - sr)/(1<<sb0[1].sampe);
	scalecolor(col, 1.-d);
	scalecolor(c1, d);
	addcolor(col, c1);
}


ascanval(col, x, y, sb)		/* interpolate scanbar at orig. coords (x,y) */
COLOR	col;
int	x, y;
SCANBAR	*sb;
{
	COLOR	*sl0, *sl1, c1, c1y;
	double	dx, dy;
	int	ix, iy;

	if (sb->sampe == 0) {		/* no need to interpolate */
		sl0 = getascan(sb, y);
		copycolor(col, sl0[x]);
		return;
	}
					/* compute coordinates for sb */
	ix = dx = (x+.5)/(1<<sb->sampe) - .5;
	while (ix >= sb->len-1) ix--;
	dx -= (double)ix;
	iy = dy = (y+.5)/(1<<sb->sampe) - .5;
	while (iy >= (numscans(&inpres)>>sb->sampe)-1) iy--;
	dy -= (double)iy;
					/* get scanlines */
	sl0 = getascan(sb, iy);
#ifdef DEBUG
	if (sl0 == NULL) {
		fprintf(stderr, "%s: internal - cannot backspace in ascanval\n",
				progname);
		abort();
	}
#endif
	sl1 = getascan(sb, iy+1);
					/* 2D linear interpolation */
	copycolor(col, sl0[ix]);
	scalecolor(col, 1.-dx);
	copycolor(c1, sl0[ix+1]);
	scalecolor(c1, dx);
	addcolor(col, c1);
	copycolor(c1y, sl1[ix]);
	scalecolor(c1y, 1.-dx);
	copycolor(c1, sl1[ix+1]);
	scalecolor(c1, dx);
	addcolor(c1y, c1);
	scalecolor(col, 1.-dy);
	scalecolor(c1y, dy);
	addcolor(col, c1y);
	for (ix = 0; ix < 3; ix++)	/* make sure no negative */
		if (colval(col,ix) < 0.)
			colval(col,ix) = 0.;
}


SCANBAR	*
sballoc(se, ns, sl)		/* allocate scanbar */
int	se;		/* sampling rate exponent */
int	ns;		/* number of scanlines */
int	sl;		/* original scanline length */
{
	SCANBAR	*sbarr;
	register SCANBAR	*sb;

	sbarr = sb = (SCANBAR *)malloc((se+1)*sizeof(SCANBAR));
	if (sb == NULL)
		syserror("malloc");
	do {
		sb->sampe = se;
		sb->len = sl>>se;
		sb->nscans = ns;
		sb->sdata = (COLOR *)malloc(sb->len*ns*sizeof(COLOR));
		if (sb->sdata == NULL)
			syserror("malloc");
		sb->nread = 0;
		ns <<= 1;
		sb++;
	} while (--se >= 0);
	return(sbarr);
}


initacuity()			/* initialize variable acuity sampling */
{
	FVECT	diffx, diffy, cp;
	double	omega, maxsr;
	register int	x, y, i;

	compraydir();			/* compute ray directions */

	inpacuD = (float *)malloc(fvxr*fvyr*sizeof(float));
	if (inpacuD == NULL)
		syserror("malloc");
	maxsr = 1.;			/* compute internal sample rates */
	for (y = 1; y < fvyr-1; y++)
		for (x = 1; x < fvxr-1; x++) {
			for (i = 0; i < 3; i++) {
				diffx[i] = 0.5*fvxr/scanlen(&inpres) *
						(rdirscan(y)[x+1][i] -
						rdirscan(y)[x-1][i]);
				diffy[i] = 0.5*fvyr/numscans(&inpres) *
						(rdirscan(y+1)[x][i] -
						rdirscan(y-1)[x][i]);
			}
			fcross(cp, diffx, diffy);
			omega = 0.5 * sqrt(DOT(cp,cp));
			if (omega <= FTINY)
				tsampr(x,y) = 1.;
			else if ((tsampr(x,y) = PI/180. / sqrt(omega) /
					hacuity(plum(fovscan(y)[x]))) > maxsr)
				maxsr = tsampr(x,y);
		}
					/* copy perimeter (easier) */
	for (x = 1; x < fvxr-1; x++) {
		tsampr(x,0) = tsampr(x,1);
		tsampr(x,fvyr-1) = tsampr(x,fvyr-2);
	}
	for (y = 0; y < fvyr; y++) {
		tsampr(0,y) = tsampr(1,y);
		tsampr(fvxr-1,y) = tsampr(fvxr-2,y);
	}
					/* initialize with next power of two */
	rootbar = sballoc((int)(log(maxsr)/log(2.))+1, 2, scanlen(&inpres));
}
