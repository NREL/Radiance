/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for light-redirecting materials and
 *   their associated virtual light sources
 */

#include  "ray.h"

#include  "otypes.h"

#include  "source.h"

/*
 * The arguments for MAT_DIRECT1 are:
 *
 *	4+ coef1 dx1 dy1 dz1 transform..
 *	0
 *	n A1 A2 .. An
 *
 * The arguments for MAT_DIRECT2 are:
 *
 *	8+ coef1 dx1 dy1 dz1 coef2 dx2 dy2 dz2 transform..
 *	0
 *	n A1 A2 .. An
 */


extern double  varvalue();

int  dir_proj();
VSMATERIAL  direct1_vs = {dir_proj, 1};
VSMATERIAL  direct2_vs = {dir_proj, 2};


m_direct(m, r)			/* shade redirected ray */
register OBJREC  *m;
register RAY  *r;
{
					/* check if source ray */
	if (r->rsrc >= 0 && source[r->rsrc].so != r->ro)
		return;				/* got the wrong guy */
					/* compute first projection */
	if (m->otype == MAT_DIRECT1 ||
			(r->rsrc < 0 || source[r->rsrc].sa.sv.pn == 0))
		redirect(m, r, 0);
					/* compute second projection */
	if (m->otype == MAT_DIRECT2 &&
			(r->rsrc < 0 || source[r->rsrc].sa.sv.pn == 1))
		redirect(m, r, 1);
}


redirect(m, r, n)		/* compute n'th ray redirection */
OBJREC  *m;
RAY  *r;
int  n;
{
	register char  **sa;
	RAY  nr;
	double  coef;
	register int  j;
					/* set up function */
	setfunc(m, r);
	if (m->oargs.nsargs < 4+4*n)
		objerror(m, USER, "too few arguments");
	sa = m->oargs.sarg + 4*n;
					/* compute coefficient */
	errno = 0;
	coef = varvalue(sa[0]);
	if (errno)
		goto computerr;
	if (coef <= FTINY || rayorigin(&nr, r, TRANS, coef) < 0)
		return(0);
					/* compute direction */
	errno = 0;
	for (j = 0; j < 3; j++)
		nr.rdir[j] = varvalue(sa[j+1]);
	if (errno || normalize(nr.rdir) == 0.0)
		goto computerr;
					/* compute value */
	if (r->rsrc >= 0)
		nr.rsrc = source[r->rsrc].sa.sv.sn;
	rayvalue(&nr);
	scalecolor(nr.rcol, coef);
	addcolor(r->rcol, nr.rcol);
	return(1);
computerr:
	objerror(m, WARNING, "compute error");
	return(-1);
}


dir_proj(pm, o, s, n)		/* compute a director's projection */
MAT4  pm;
OBJREC  *o;
SRCREC  *s;
int  n;
{
	RAY  tr;
	register OBJREC  *m;
	char  **sa;
	FVECT  cent, newdir, nv, h;
	double  olddot, newdot, od;
	register int  i, j;
				/* get material arguments */
	m = objptr(o->omod);
	if (m->oargs.nsargs < 4+4*n)
		objerror(m, USER, "too few arguments");
	sa = m->oargs.sarg + 4*n;
				/* initialize test ray */
	getmaxdisk(cent, o);
	if (s->sflags & SDISTANT)
		for (i = 0; i < 3; i++) {
			tr.rdir[i] = -s->sloc[i];
			tr.rorg[i] = cent[i] - tr.rdir[i];
		}
	else {
		for (i = 0; i < 3; i++) {
			tr.rdir[i] = cent[i] - s->sloc[i];
			tr.rorg[i] = s->sloc[i];
		}
		if (normalize(tr.rdir) == 0.0)
			return(0);		/* at source! */
	}
	od = getplaneq(nv, o);
	olddot = DOT(tr.rdir, nv);
	if (olddot <= FTINY && olddot >= -FTINY)
		return(0);		/* old dir parallels plane */
	rayorigin(&tr, NULL, PRIMARY, 1.0);
	if (!(*ofun[o->otype].funp)(o, &tr))
		return(0);		/* no intersection! */
				/* compute redirection */
	setfunc(m, &tr);
	errno = 0;
	if (varvalue(sa[0]) <= FTINY)
		return(0);		/* insignificant */
	if (errno)
		goto computerr;
	for (i = 0; i < 3; i++)
		newdir[i] = varvalue(sa[i+1]);
	if (errno)
		goto computerr;
	newdot = DOT(newdir, nv);
	if (newdot <= FTINY && newdot >= -FTINY)
		return(0);		/* new dir parallels plane */
				/* everything OK -- compute shear */
	for (i = 0; i < 3; i++)
		h[i] = tr.rdir[i]/olddot + newdir[i]/newdot;
	setident4(pm);
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			pm[i][j] += nv[i]*h[j];
		pm[3][j] = -od*h[j];
	}
	if (newdot > 0.0 ^ olddot > 0.0)	/* add mirroring */
		for (j = 0; j < 3; j++) {
			for (i = 0; i < 3; i++)
				pm[i][j] -= 2.*nv[i]*nv[j];
			pm[3][j] += 2.*od*nv[j];
		}
	return(1);
computerr:
	objerror(m, WARNING, "projection compute error");
	return(0);
}
