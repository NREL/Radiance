/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rv3.c - miscellaneous routines for rview.
 *
 *     5/11/87
 */

#include  "ray.h"

#include  "rpaint.h"

#include  "random.h"


float *		/* keep consistent with COLOR typedef */
greyof(col)				/* convert color to greyscale */
register COLOR  col;
{
	static COLOR  gcol;
	double  b;

	b = bright(col);
	setcolor(gcol, b, b, b);
	return(gcol);
}


paint(p, xmin, ymin, xmax, ymax)	/* compute and paint a rectangle */
register PNODE  *p;
int  xmin, ymin, xmax, ymax;
{
	static RAY  thisray;
	double  h, v;
	register int  i;

	if (xmax - xmin <= 0 || ymax - ymin <= 0) {	/* empty */
		p->x = xmin;
		p->y = ymin;
		setcolor(p->v, 0.0, 0.0, 0.0);
		return;
	}
						/* jitter ray direction */
	p->x = h = xmin + (xmax-xmin)*frandom();
	p->y = v = ymin + (ymax-ymin)*frandom();
	
	rayview(thisray.rorg, thisray.rdir, &ourview, h, v);

	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	
	rayvalue(&thisray);

	copycolor(p->v, thisray.rcol);

	scalecolor(p->v, exposure);

	(*dev->paintr)(greyscale?greyof(p->v):p->v, xmin, ymin, xmax, ymax);
}


newimage()				/* start a new image */
{
						/* free old image */
	freepkids(&ptrunk);
						/* set up frame */
	if (ourview.hresolu > dev->xsiz || ourview.vresolu > dev->ysiz)
		error(USER, "resolution mismatch");
	pframe.l = pframe.d = 0;
	pframe.r = ourview.hresolu; pframe.u = ourview.vresolu;
	pdepth = 0;
						/* clear device */
	(*dev->clear)(ourview.hresolu, ourview.vresolu);
						/* get first value */
	paint(&ptrunk, 0, 0, ourview.hresolu, ourview.vresolu);
}


redraw()				/* redraw the image */
{
	(*dev->clear)(ourview.hresolu, ourview.vresolu);
	(*dev->comout)("redrawing...\n");
	repaint(0, 0, ourview.hresolu, ourview.vresolu);
	(*dev->comout)("\n");
}


repaint(xmin, ymin, xmax, ymax)			/* repaint a region */
int  xmin, ymin, xmax, ymax;
{
	RECT  reg;

	reg.l = xmin; reg.r = xmax;
	reg.d = ymin; reg.u = ymax;

	paintrect(&ptrunk, 0, 0, ourview.hresolu, ourview.vresolu, &reg);
}


paintrect(p, xmin, ymin, xmax, ymax, r)		/* paint picture rectangle */
register PNODE  *p;
int  xmin, ymin, xmax, ymax;
register RECT  *r;
{
	int  mx, my;

	if (dev->inpready)
		return;				/* break for input */

	if (xmax - xmin <= 0 || ymax - ymin <= 0)
		return;

	if (p->kid == NULL) {
		(*dev->paintr)(greyscale?greyof(p->v):p->v,
				xmin, ymin, xmax, ymax);	/* do this */
		return;
	}
	mx = (xmin + xmax) >> 1;				/* do kids */
	my = (ymin + ymax) >> 1;
	if (mx > r->l) {
		if (my > r->d)
			paintrect(p->kid+DL, xmin, ymin, mx, my, r);
		if (my < r->u)
			paintrect(p->kid+UL, xmin, my, mx, ymax, r);
	}
	if (mx < r->r) {
		if (my > r->d)
			paintrect(p->kid+DR, mx, ymin, xmax, my, r);
		if (my < r->u)
			paintrect(p->kid+UR, mx, my, xmax, ymax, r);
	}
}


PNODE *
findrect(x, y, p, r, pd)		/* find a rectangle */
int  x, y;
register PNODE  *p;
register RECT  *r;
int  pd;
{
	int  mx, my;

	while (p->kid != NULL && pd--) {

		mx = (r->l + r->r) >> 1;
		my = (r->d + r->u) >> 1;

		if (x < mx) {
			r->r = mx;
			if (y < my) {
				r->u = my;
				p = p->kid+DL;
			} else {
				r->d = my;
				p = p->kid+UL;
			}
		} else {
			r->l = mx;
			if (y < my) {
				r->u = my;
				p = p->kid+DR;
			} else {
				r->d = my;
				p = p->kid+UR;
			}
		}
	}
	return(p);
}


scalepict(p, sf)			/* scale picture values */
register PNODE  *p;
double  sf;
{
	scalecolor(p->v, sf);		/* do this node */

	if (p->kid == NULL)
		return;
					/* do children */
	scalepict(p->kid+DL, sf);
	scalepict(p->kid+DR, sf);
	scalepict(p->kid+UL, sf);
	scalepict(p->kid+UR, sf);
}


getpictcolrs(yoff, scan, p, xsiz, ysiz)	/* get scanline from picture */
int  yoff;
register COLR  *scan;
register PNODE  *p;
int  xsiz, ysiz;
{
	register int  mx;
	int  my;

	if (p->kid == NULL) {			/* do this node */
		setcolr(scan[0], colval(p->v,RED),
				colval(p->v,GRN),
				colval(p->v,BLU));
		for (mx = 1; mx < xsiz; mx++)
			copycolr(scan[mx], scan[0]);
		return;
	}
						/* do kids */
	mx = xsiz >> 1;
	my = ysiz >> 1;
	if (yoff < my) {
		getpictcolrs(yoff, scan, p->kid+DL, mx, my);
		getpictcolrs(yoff, scan+mx, p->kid+DR, xsiz-mx, my);
	} else {
		getpictcolrs(yoff-my, scan, p->kid+UL, mx, ysiz-my);
		getpictcolrs(yoff-my, scan+mx, p->kid+UR, xsiz-mx, ysiz-my);
	}
}


pcopy(p1, p2)				/* copy paint node p1 into p2 */
register PNODE  *p1, *p2;
{
	copycolor(p2->v, p1->v);
	p2->x = p1->x;
	p2->y = p1->y;
}


freepkids(p)				/* free pnode's children */
register PNODE  *p;
{
	if (p->kid == NULL)
		return;
	freepkids(p->kid+DL);
	freepkids(p->kid+DR);
	freepkids(p->kid+UL);
	freepkids(p->kid+UR);
	free((char *)p->kid);
	p->kid = NULL;
}


newview(vp)				/* change viewing parameters */
register VIEW  *vp;
{
	char  *err;

	if (vp->hresolu > dev->xsiz || vp->vresolu > dev->ysiz) {
		error(COMMAND, "view not set - resolution mismatch");
	} else if ((err = setview(vp)) != NULL) {
		sprintf(errmsg, "view not set - %s", err);
		error(COMMAND, errmsg);
	} else if (bcmp(vp, &ourview, sizeof(VIEW))) {
		bcopy(&ourview, &oldview, sizeof(VIEW));
		bcopy(vp, &ourview, sizeof(VIEW));
		newimage();
	}
}


moveview(angle, mag, vc)			/* move viewpoint */
double  angle, mag;
FVECT  vc;
{
	extern double  sqrt(), dist2();
	double  d;
	VIEW  nv;
	register int  i;

	VCOPY(nv.vup, ourview.vup);
	nv.hresolu = ourview.hresolu; nv.vresolu = ourview.vresolu;
	spinvector(nv.vdir, ourview.vdir, ourview.vup, angle*(PI/180.));
	d = sqrt(dist2(ourview.vp, vc)) / mag;
	for (i = 0; i < 3; i++)
		nv.vp[i] = vc[i] - d*nv.vdir[i];
	if ((nv.type = ourview.type) == VT_PAR) {
		nv.horiz = ourview.horiz / mag;
		nv.vert = ourview.vert / mag;
	} else {
		nv.horiz = ourview.horiz;
		nv.vert = ourview.vert;
	}
	newview(&nv);
}


spinvector(vres, vorig, vnorm, theta)	/* rotate vector around normal */
FVECT  vres, vorig, vnorm;
double  theta;
{
	extern double  sin(), cos();
	double  sint, cost, dotp;
	FVECT  vperp;
	register int  i;
	
	if (theta == 0.0) {
		VCOPY(vres, vorig);
		return;
	}
	sint = sin(theta);
	cost = cos(theta);
	dotp = DOT(vorig, vnorm);
	fcross(vperp, vnorm, vorig);
	for (i = 0; i < 3; i++)
		vres[i] = vnorm[i]*dotp*(1.-cost) +
				vorig[i]*cost + vperp[i]*sint;
}
