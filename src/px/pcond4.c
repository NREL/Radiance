/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for veiling glare and loss of acuity.
 */

#include "pcond.h"


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
					t2 = acos(t2);
					t2 = 1./(t2*t2);
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
	if (vy >= fvyr-1) vy--;
	dy -= (double)vy;
	for (x = 0; x < scanlen(&inpres); x++) {
		vx = dx = (x+.5)/scanlen(&inpres)*fvxr - .5;
		if (vx >= fvxr-1) vx--;
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
