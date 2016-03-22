#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Routines to do the actual calculation for mkillum
 */

#include <string.h>

#include  "mkillum.h"
#include  "face.h"
#include  "cone.h"
#include  "source.h"
#include  "paths.h"

#ifndef NBSDFSAMPS
#define NBSDFSAMPS	256		/* BSDF resampling count */
#endif

COLORV *	distarr = NULL;		/* distribution array */
int		distsiz = 0;


void
newdist(			/* allocate & clear distribution array */
	int siz
)
{
	if (siz <= 0) {
		if (distsiz > 0)
			free(distarr);
		distarr = NULL;
		distsiz = 0;
		return;
	}
	if (distsiz < siz) {
		if (distsiz > 0)
			free(distarr);
		distarr = (COLORV *)malloc(sizeof(COLOR)*siz);
		if (distarr == NULL)
			error(SYSTEM, "out of memory in newdist");
		distsiz = siz;
	}
	memset(distarr, '\0', sizeof(COLOR)*siz);
}


int
process_ray(			/* process a ray result or report error */
	RAY *r,
	int rv
)
{
	COLORV	*colp;

	if (rv == 0)			/* no result ready */
		return(0);
	if (rv < 0)
		error(USER, "ray tracing process died");
	if (r->rno >= distsiz)
		error(INTERNAL, "bad returned index in process_ray");
	multcolor(r->rcol, r->rcoef);	/* in case it's a source ray */
	colp = &distarr[r->rno * 3];
	addcolor(colp, r->rcol);
	return(1);
}


void
raysamp(			/* queue a ray sample */
	int  ndx,
	FVECT  org,
	FVECT  dir
)
{
	RAY	myRay;
	int	rv;

	if ((ndx < 0) | (ndx >= distsiz))
		error(INTERNAL, "bad index in raysamp");
	VCOPY(myRay.rorg, org);
	VCOPY(myRay.rdir, dir);
	myRay.rmax = .0;
	rayorigin(&myRay, PRIMARY|SPECULAR, NULL, NULL);
	myRay.rno = ndx;
					/* queue ray, check result */
	process_ray(&myRay, ray_pqueue(&myRay));
}


void
srcsamps(			/* sample sources from this surface position */
	struct illum_args *il,
	FVECT org,
	FVECT nrm,
	MAT4 ixfm
)
{
	int  nalt=1, nazi=1;
	SRCINDEX  si;
	RAY  sr;
	FVECT	v;
	double  d;
	int  i, j;
						/* get sampling density */
	if (il->sampdens > 0) {
		i = PI * il->sampdens;
		nalt = sqrt(i/PI) + .5;
		nazi = PI*nalt + .5;
	}
	initsrcindex(&si);			/* loop over (sub)sources */
	for ( ; ; ) {
		VCOPY(sr.rorg, org);		/* pick side to shoot from */
		d = 5.*FTINY;
		VSUM(sr.rorg, sr.rorg, nrm, d);
		samplendx++;			/* increment sample counter */
		if (!srcray(&sr, NULL, &si))
			break;			/* end of sources */
						/* index direction */
		if (ixfm != NULL)
			multv3(v, sr.rdir, ixfm);
		else
			VCOPY(v, sr.rdir);
		if (v[2] >= -FTINY)
			continue;	/* only sample transmission */
		v[0] = -v[0]; v[1] = -v[1]; v[2] = -v[2];
		sr.rno = flatindex(v, nalt, nazi);
		d = nalt*nazi*(1./PI) * v[2];
		d *= si.dom;			/* solid angle correction */
		scalecolor(sr.rcoef, d);
		process_ray(&sr, ray_pqueue(&sr));
	}
}


void
rayclean()			/* finish all pending rays */
{
	RAY	myRay;

	while (process_ray(&myRay, ray_presult(&myRay, 0)))
		;
}


static void
mkaxes(			/* compute u and v to go with n */
	FVECT  u,
	FVECT  v,
	FVECT  n
)
{
	getperpendicular(u, n, 1);
	fcross(v, n, u);
}


static void
rounddir(		/* compute uniform spherical direction */
	FVECT  dv,
	double  alt,
	double  azi
)
{
	double  d1, d2;

	dv[2] = 1. - 2.*alt;
	d1 = sqrt(1. - dv[2]*dv[2]);
	d2 = 2.*PI * azi;
	dv[0] = d1*cos(d2);
	dv[1] = d1*sin(d2);
}


void
flatdir(		/* compute uniform hemispherical direction */
	FVECT  dv,
	double  alt,
	double  azi
)
{
	double  d1, d2;

	d1 = sqrt(alt);
	d2 = 2.*PI * azi;
	dv[0] = d1*cos(d2);
	dv[1] = d1*sin(d2);
	dv[2] = sqrt(1. - alt);
}


int
flatindex(		/* compute index for hemispherical direction */
	FVECT	dv,
	int	nalt,
	int	nazi
)
{
	double	d;
	int	i, j;
	
	d = 1.0 - dv[2]*dv[2];
	i = d*nalt;
	d = atan2(dv[1], dv[0]) * (0.5/PI);
	if (d < 0.0) d += 1.0;
	j = d*nazi + 0.5;
	if (j >= nazi) j = 0;
	return(i*nazi + j);
}


int
my_default(	/* default illum action */
	OBJREC  *ob,
	struct illum_args  *il,
	char  *nm
)
{
	sprintf(errmsg, "(%s): cannot make illum for %s \"%s\"",
			nm, ofun[ob->otype].funame, ob->oname);
	error(WARNING, errmsg);
	printobj(il->altmat, ob);
	return(1);
}


int
my_face(		/* make an illum face */
	OBJREC  *ob,
	struct illum_args  *il,
	char  *nm
)
{
	int  dim[2];
	int  n, nalt, nazi, alti;
	double  sp[2], r1, r2;
	int  h;
	FVECT  dn, org, dir;
	FVECT  u, v;
	double  ur[2], vr[2];
	MAT4  xfm;
	char  xfrot[64];
	int  nallow;
	FACE  *fa;
	int  i, j;
				/* get/check arguments */
	fa = getface(ob);
	if (fa->area == 0.0) {
		freeface(ob);
		return(my_default(ob, il, nm));
	}
				/* set up sampling */
	if (il->sampdens <= 0) {
		nalt = nazi = 1;	/* diffuse assumption */
	} else {
		n = PI * il->sampdens;
		nalt = sqrt(n/PI) + .5;
		nazi = PI*nalt + .5;
	}
	n = nazi*nalt;
	newdist(n);
				/* take first edge >= sqrt(area) */
	for (j = fa->nv-1, i = 0; i < fa->nv; j = i++) {
		u[0] = VERTEX(fa,i)[0] - VERTEX(fa,j)[0];
		u[1] = VERTEX(fa,i)[1] - VERTEX(fa,j)[1];
		u[2] = VERTEX(fa,i)[2] - VERTEX(fa,j)[2];
		if ((r1 = DOT(u,u)) >= fa->area-FTINY)
			break;
	}
	if (i < fa->nv) {	/* got one! -- let's align our axes */
		r2 = 1.0/sqrt(r1);
		u[0] *= r2; u[1] *= r2; u[2] *= r2;
		fcross(v, fa->norm, u);
	} else			/* oh well, we'll just have to wing it */
		mkaxes(u, v, fa->norm);
				/* now, find limits in (u,v) coordinates */
	ur[0] = vr[0] = FHUGE;
	ur[1] = vr[1] = -FHUGE;
	for (i = 0; i < fa->nv; i++) {
		r1 = DOT(VERTEX(fa,i),u);
		if (r1 < ur[0]) ur[0] = r1;
		if (r1 > ur[1]) ur[1] = r1;
		r2 = DOT(VERTEX(fa,i),v);
		if (r2 < vr[0]) vr[0] = r2;
		if (r2 > vr[1]) vr[1] = r2;
	}
	dim[0] = random();
				/* sample polygon */
	nallow = 5*n*il->nsamps;
	for (dim[1] = 0; dim[1] < n; dim[1]++)
		for (i = 0; i < il->nsamps; i++) {
					/* randomize direction */
		    h = ilhash(dim, 2) + i;
		    multisamp(sp, 2, urand(h));
		    alti = dim[1]/nazi;
		    r1 = (alti + sp[0])/nalt;
		    r2 = (dim[1] - alti*nazi + sp[1] - .5)/nazi;
		    flatdir(dn, r1, r2);
		    for (j = 0; j < 3; j++)
			    dir[j] = -dn[0]*u[j] - dn[1]*v[j] -
						dn[2]*fa->norm[j];
					/* randomize location */
		    do {
			multisamp(sp, 2, urand(h+4862+nallow));
			r1 = ur[0] + (ur[1]-ur[0]) * sp[0];
			r2 = vr[0] + (vr[1]-vr[0]) * sp[1];
			for (j = 0; j < 3; j++)
			    org[j] = r1*u[j] + r2*v[j]
					+ fa->offset*fa->norm[j];
		    } while (!inface(org, fa) && nallow-- > 0);
		    if (nallow < 0) {
			objerror(ob, WARNING, "bad aspect");
			rayclean();
			freeface(ob);
			return(my_default(ob, il, nm));
		    }
		    r1 = 5.*FTINY;
		    for (j = 0; j < 3; j++)
			org[j] += r1*fa->norm[j];
					/* send sample */
		    raysamp(dim[1], org, dir);
		}
				/* add in direct component? */
	if (il->flags & IL_LIGHT) {
		MAT4	ixfm;
		for (i = 3; i--; ) {
			ixfm[i][0] = u[i];
			ixfm[i][1] = v[i];
			ixfm[i][2] = fa->norm[i];
			ixfm[i][3] = 0.;
		}
		ixfm[3][0] = ixfm[3][1] = ixfm[3][2] = 0.;
		ixfm[3][3] = 1.;
		dim[0] = random();
		nallow = 10*il->nsamps;
		for (i = 0; i < il->nsamps; i++) {
					/* randomize location */
		    h = dim[0] + samplendx++;
		    do {
			multisamp(sp, 2, urand(h+nallow));
			r1 = ur[0] + (ur[1]-ur[0]) * sp[0];
			r2 = vr[0] + (vr[1]-vr[0]) * sp[1];
			for (j = 0; j < 3; j++)
			    org[j] = r1*u[j] + r2*v[j]
					+ fa->offset*fa->norm[j];
		    } while (!inface(org, fa) && nallow-- > 0);
		    if (nallow < 0) {
			objerror(ob, WARNING, "bad aspect");
			rayclean();
			freeface(ob);
			return(my_default(ob, il, nm));
		    }
					/* sample source rays */
		    srcsamps(il, org, fa->norm, ixfm);
		}
	}
				/* wait for all rays to finish */
	rayclean();
				/* write out the face and its distribution */
	if (average(il, distarr, n)) {
		if (il->sampdens > 0)
			flatout(il, distarr, nalt, nazi, u, v, fa->norm);
		illumout(il, ob);
	} else
		printobj(il->altmat, ob);
				/* clean up */
	freeface(ob);
	return(0);
}


int
my_sphere(	/* make an illum sphere */
	OBJREC  *ob,
	struct illum_args  *il,
	char  *nm
)
{
	int  dim[3];
	int  n, nalt, nazi;
	double  sp[4], r1, r2, r3;
	FVECT  org, dir;
	FVECT  u, v;
	int  i, j;
				/* check arguments */
	if (ob->oargs.nfargs != 4)
		objerror(ob, USER, "bad # of arguments");
				/* set up sampling */
	if (il->sampdens <= 0)
		nalt = nazi = 1;
	else {
		n = 4.*PI * il->sampdens;
		nalt = sqrt(2./PI*n) + .5;
		nazi = PI/2.*nalt + .5;
	}
	n = nalt*nazi;
	newdist(n);
	dim[0] = random();
				/* sample sphere */
	for (dim[1] = 0; dim[1] < nalt; dim[1]++)
	    for (dim[2] = 0; dim[2] < nazi; dim[2]++)
		for (i = 0; i < il->nsamps; i++) {
					/* next sample point */
		    multisamp(sp, 4, urand(ilhash(dim,3)+i));
					/* random direction */
		    r1 = (dim[1] + sp[0])/nalt;
		    r2 = (dim[2] + sp[1] - .5)/nazi;
		    rounddir(dir, r1, r2);
					/* random location */
		    mkaxes(u, v, dir);		/* yuck! */
		    r3 = sqrt(sp[2]);
		    r2 = 2.*PI*sp[3];
		    r1 = r3*ob->oargs.farg[3]*cos(r2);
		    r2 = r3*ob->oargs.farg[3]*sin(r2);
		    r3 = ob->oargs.farg[3]*sqrt(1.01-r3*r3);
		    for (j = 0; j < 3; j++) {
			org[j] = ob->oargs.farg[j] + r1*u[j] + r2*v[j] +
					r3*dir[j];
			dir[j] = -dir[j];
		    }
					/* send sample */
		    raysamp(dim[1]*nazi+dim[2], org, dir);
		}
				/* wait for all rays to finish */
	rayclean();
				/* write out the sphere and its distribution */
	if (average(il, distarr, n)) {
		if (il->sampdens > 0)
			roundout(il, distarr, nalt, nazi);
		else
			objerror(ob, WARNING, "diffuse distribution");
		illumout(il, ob);
	} else
		printobj(il->altmat, ob);
				/* clean up */
	return(1);
}


int
my_ring(		/* make an illum ring */
	OBJREC  *ob,
	struct illum_args  *il,
	char  *nm
)
{
	int  dim[2];
	int  n, nalt, nazi, alti;
	double  sp[2], r1, r2, r3;
	int  h;
	FVECT  dn, org, dir;
	FVECT  u, v;
	MAT4  xfm;
	CONE  *co;
	int  i, j;
				/* get/check arguments */
	co = getcone(ob, 0);
				/* set up sampling */
	if (il->sampdens <= 0) {
		nalt = nazi = 1;	/* diffuse assumption */
	} else {
		n = PI * il->sampdens;
		nalt = sqrt(n/PI) + .5;
		nazi = PI*nalt + .5;
	}
	n = nazi*nalt;
	newdist(n);
	mkaxes(u, v, co->ad);
	dim[0] = random();
				/* sample disk */
	for (dim[1] = 0; dim[1] < n; dim[1]++)
		for (i = 0; i < il->nsamps; i++) {
					/* next sample point */
		    h = ilhash(dim,2) + i;
					/* randomize direction */
		    multisamp(sp, 2, urand(h));
		    alti = dim[1]/nazi;
		    r1 = (alti + sp[0])/nalt;
		    r2 = (dim[1] - alti*nazi + sp[1] - .5)/nazi;
		    flatdir(dn, r1, r2);
		    for (j = 0; j < 3; j++)
			dir[j] = -dn[0]*u[j] - dn[1]*v[j] - dn[2]*co->ad[j];
					/* randomize location */
		    multisamp(sp, 2, urand(h+8371));
		    r3 = sqrt(CO_R0(co)*CO_R0(co) +
			    sp[0]*(CO_R1(co)*CO_R1(co) - CO_R0(co)*CO_R0(co)));
		    r2 = 2.*PI*sp[1];
		    r1 = r3*cos(r2);
		    r2 = r3*sin(r2);
		    r3 = 5.*FTINY;
		    for (j = 0; j < 3; j++)
			org[j] = CO_P0(co)[j] + r1*u[j] + r2*v[j] +
						r3*co->ad[j];
					/* send sample */
		    raysamp(dim[1], org, dir);
		}
				/* add in direct component? */
	if (il->flags & IL_LIGHT) {
		MAT4	ixfm;
		for (i = 3; i--; ) {
			ixfm[i][0] = u[i];
			ixfm[i][1] = v[i];
			ixfm[i][2] = co->ad[i];
			ixfm[i][3] = 0.;
		}
		ixfm[3][0] = ixfm[3][1] = ixfm[3][2] = 0.;
		ixfm[3][3] = 1.;
		dim[0] = random();
		for (i = 0; i < il->nsamps; i++) {
					/* randomize location */
		    h = dim[0] + samplendx++;
		    multisamp(sp, 2, urand(h));
		    r3 = sqrt(CO_R0(co)*CO_R0(co) +
			    sp[0]*(CO_R1(co)*CO_R1(co) - CO_R0(co)*CO_R0(co)));
		    r2 = 2.*PI*sp[1];
		    r1 = r3*cos(r2);
		    r2 = r3*sin(r2);
		    for (j = 0; j < 3; j++)
			org[j] = CO_P0(co)[j] + r1*u[j] + r2*v[j];
					/* sample source rays */
		    srcsamps(il, org, co->ad, ixfm);
		}
	}
				/* wait for all rays to finish */
	rayclean();
				/* write out the ring and its distribution */
	if (average(il, distarr, n)) {
		if (il->sampdens > 0)
			flatout(il, distarr, nalt, nazi, u, v, co->ad);
		illumout(il, ob);
	} else
		printobj(il->altmat, ob);
				/* clean up */
	freecone(ob);
	return(1);
}
