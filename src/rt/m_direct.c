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
 *	5+ coef1 dx1 dy1 dz1 funcfile transform..
 *	0
 *	n A1 A2 .. An
 *
 * The arguments for MAT_DIRECT2 are:
 *
 *	9+ coef1 dx1 dy1 dz1 coef2 dx2 dy2 dz2 funcfile transform..
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
	dir_check(m);
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
	setmap(m, r, &((FULLXF *)m->os)->b);
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
	if (errno)
		goto computerr;
	multv3(nr.rdir, nr.rdir, ((FULLXF *)m->os)->f.xfm);
	if (r->rox != NULL)
		multv3(nr.rdir, nr.rdir, r->rox->f.xfm);
	if (normalize(nr.rdir) == 0.0)
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
	dir_check(m);
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
	setmap(m, &tr, &((FULLXF *)m->os)->b);
	errno = 0;
	if (varvalue(sa[0]) <= FTINY)
		return(0);		/* insignificant */
	if (errno)
		goto computerr;
	for (i = 0; i < 3; i++)
		newdir[i] = varvalue(sa[i+1]);
	if (errno)
		goto computerr;
	multv3(newdir, newdir, ((FULLXF *)m->os)->f.xfm);
					/* normalization unnecessary */
	newdot = DOT(newdir, nv);
	if (newdot <= FTINY && newdot >= -FTINY)
		return(0);		/* new dir parallels plane */
				/* everything OK -- compute shear */
	for (i = 0; i < 3; i++)
		h[i] = newdir[i]/newdot - tr.rdir[i]/olddot;
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


static
dir_check(m)			/* check arguments and load function file */
register OBJREC  *m;
{
	register FULLXF  *mxf;
	register int  ff;

	ff = m->otype == MAT_DIRECT1 ? 5 : 9;
	if (ff > m->oargs.nsargs)
		objerror(m, USER, "too few arguments");
	funcfile(m->oargs.sarg[ff-1]);
	if (m->os == NULL) {
		mxf = (FULLXF *)malloc(sizeof(FULLXF));
		if (mxf == NULL)
			error(SYSTEM, "out of memory in dir_check");
		if (fullxf(mxf, m->oargs.nsargs-ff, m->oargs.sarg+ff) !=
				m->oargs.nsargs-ff)
			objerror(m, USER, "bad transform");
		if (mxf->f.sca < 0.0)
			mxf->f.sca = -mxf->f.sca;
		if (mxf->b.sca < 0.0)
			mxf->b.sca = -mxf->b.sca;
		m->os = (char *)mxf;
	}
}
