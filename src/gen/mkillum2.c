/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines to do the actual calculation for mkillum
 */

#include  "mkillum.h"

#include  "face.h"

#include  "cone.h"

#include  "random.h"


o_default(ob, il, rt, nm)	/* default illum action */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
	sprintf(errmsg, "(%s): cannot make illum for %s \"%s\"",
			nm, ofun[ob->otype].funame, ob->oname);
	error(WARNING, errmsg);
	if (!(il->flags & IL_LIGHT))
		printobj(il->altmat, ob);
}


o_face(ob, il, rt, nm)		/* make an illum face */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
#define MAXMISS		(5*n*il->nsamps)
	int  dim[4];
	int  n, nalt, nazi;
	float  *distarr;
	double  r1, r2;
	FVECT  dn, org, dir;
	FVECT  u, v;
	double  ur[2], vr[2];
	int  nmisses;
	register FACE  *fa;
	register int  i, j;
				/* get/check arguments */
	fa = getface(ob);
	if (fa->area == 0.0) {
		freeface(ob);
		o_default(ob, il, rt, nm);
		return;
	}
				/* set up sampling */
	n = PI * il->sampdens;
	nalt = sqrt(n/PI) + .5;
	nazi = PI*nalt + .5;
	n = nalt*nazi;
	distarr = (float *)calloc(n, 3*sizeof(float));
	if (distarr == NULL)
		error(SYSTEM, "out of memory in o_face");
	mkaxes(u, v, fa->norm);
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
	nmisses = 0;
	for (dim[1] = 0; dim[1] < nalt; dim[1]++)
	    for (dim[2] = 0; dim[2] < nazi; dim[2]++)
		for (i = 0; i < il->nsamps; i++) {
					/* random direction */
		    dim[3] = 1;
		    r1 = (dim[1]+urand(urind(ilhash(dim,4),i)))/nalt;
		    dim[3] = 2;
		    r2 = (dim[2]+urand(urind(ilhash(dim,4),i)))/nalt;
		    flatdir(dn, r1, r2);
		    for (j = 0; j < 3; j++)
			dir[j] = dn[0]*u[j] + dn[1]*v[j] - dn[2]*fa->norm[j];
					/* random location */
		    do {
			dim[3] = 3;
			r1 = ur[0] + (ur[1]-ur[0]) *
					urand(urind(ilhash(dim,4),i+nmisses));
			dim[3] = 4;
			r2 = vr[0] + (vr[1]-vr[0]) *
					urand(urind(ilhash(dim,4),i+nmisses));
			for (j = 0; j < 3; j++)
			    org[j] = r1*u[j] + r2*v[j]
					+ fa->offset*fa->norm[j];
		    } while (!inface(org, fa) && nmisses++ < MAXMISS);
		    if (nmisses > MAXMISS) {
			objerror(ob, WARNING, "bad aspect");
			rt->nrays = 0;
			freeface(ob);
			free((char *)distarr);
			o_default(ob, il, rt, nm);
			return;
		    }
		    for (j = 0; j < 3; j++)
			org[j] += .001*fa->norm[j];
					/* send sample */
		    raysamp(distarr+dim[1]*nazi+dim[2], org, dir, rt);
		}
	rayflush(rt);
				/* write out the face w/ distribution */
	flatout(il, distarr, nalt, nazi, u, v, fa->norm);
	illumout(il, ob);
				/* clean up */
	freeface(ob);
	free((char *)distarr);
#undef MAXMISS
}


o_sphere(ob, il, rt, nm)	/* make an illum sphere */
register OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
	int  dim[4];
	int  n, nalt, nazi;
	float  *distarr;
	double  r1, r2;
	FVECT  org, dir;
	FVECT  u, v;
	register int  i, j;
				/* check arguments */
	if (ob->oargs.nfargs != 4)
		objerror(ob, USER, "bad # of arguments");
				/* set up sampling */
	n = 4.*PI * il->sampdens;
	nalt = sqrt(n/PI) + .5;
	nazi = PI*nalt + .5;
	n = nalt*nazi;
	distarr = (float *)calloc(n, 3*sizeof(float));
	if (distarr == NULL)
		error(SYSTEM, "out of memory in o_sphere");
	dim[0] = random();
				/* sample sphere */
	for (dim[1] = 0; dim[1] < nalt; dim[1]++)
	    for (dim[2] = 0; dim[2] < nazi; dim[2]++)
		for (i = 0; i < il->nsamps; i++) {
					/* random direction */
		    dim[3] = 1;
		    r1 = (dim[1]+urand(urind(ilhash(dim,4),i)))/nalt;
		    dim[3] = 2;
		    r2 = (dim[2]+urand(urind(ilhash(dim,4),i)))/nalt;
		    rounddir(dir, r1, r2);
					/* random location */
		    mkaxes(u, v, dir);		/* yuck! */
		    dim[3] = 3;
		    r1 = sqrt(urand(urind(ilhash(dim,4),i)));
		    dim[3] = 4;
		    r2 = 2.*PI*urand(urind(ilhash(dim,4),i));
		    for (j = 0; j < 3; j++)
			org[j] = ob->oargs.farg[j] + ob->oargs.farg[3] *
					( r1*cos(r2)*u[j] + r1*sin(r2)*v[j]
						- sqrt(1.01-r1*r1)*dir[j] );

					/* send sample */
		    raysamp(distarr+dim[1]*nazi+dim[2], org, dir, rt);
		}
	rayflush(rt);
				/* write out the sphere w/ distribution */
	roundout(il, distarr, nalt, nazi);
	illumout(il, ob);
				/* clean up */
	free((char *)distarr);
}


o_ring(ob, il, rt, nm)		/* make an illum ring */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
	int  dim[4];
	int  n, nalt, nazi;
	float  *distarr;
	double  r1, r2;
	FVECT  dn, org, dir;
	FVECT  u, v;
	register CONE  *co;
	register int  i, j;
				/* get/check arguments */
	co = getcone(ob, 0);
				/* set up sampling */
	n = PI * il->sampdens;
	nalt = sqrt(n/PI) + .5;
	nazi = PI*nalt + .5;
	n = nalt*nazi;
	distarr = (float *)calloc(n, 3*sizeof(float));
	if (distarr == NULL)
		error(SYSTEM, "out of memory in o_ring");
	mkaxes(u, v, co->ad);
	dim[0] = random();
				/* sample disk */
	for (dim[1] = 0; dim[1] < nalt; dim[1]++)
	    for (dim[2] = 0; dim[2] < nazi; dim[2]++)
		for (i = 0; i < il->nsamps; i++) {
					/* random direction */
		    dim[3] = 1;
		    r1 = (dim[1]+urand(urind(ilhash(dim,4),i)))/nalt;
		    dim[3] = 2;
		    r2 = (dim[2]+urand(urind(ilhash(dim,4),i)))/nalt;
		    flatdir(dn, r1, r2);
		    for (j = 0; j < 3; j++)
			dir[j] = dn[0]*u[j] + dn[1]*v[j] - dn[2]*co->ad[j];
					/* random location */
		    dim[3] = 3;
		    r1 = sqrt(CO_R0(co)*CO_R0(co) +
				urand(urind(ilhash(dim,4),i))*
				(CO_R1(co)*CO_R1(co) - CO_R0(co)*CO_R0(co)));
		    dim[3] = 4;
		    r2 = 2.*PI*urand(urind(ilhash(dim,4),i));
		    for (j = 0; j < 3; j++)
			org[j] = CO_P0(co)[j] +
					r1*cos(r2)*u[j] + r1*sin(r2)*v[j]
					+ .001*co->ad[j];

					/* send sample */
		    raysamp(distarr+dim[1]*nazi+dim[2], org, dir, rt);
		}
	rayflush(rt);
				/* write out the ring w/ distribution */
	flatout(il, distarr, nalt, nazi, u, v, co->ad);
	illumout(il, ob);
				/* clean up */
	freecone(ob);
	free((char *)distarr);
}


raysamp(res, org, dir, rt)	/* compute a ray sample */
float  res[3];
FVECT  org, dir;
register struct rtproc  *rt;
{
	register float  *fp;

	if (rt->nrays == rt->bsiz)
		rayflush(rt);
	rt->dest[rt->nrays] = res;
	fp = rt->buf + 6*rt->nrays++;
	*fp++ = org[0]; *fp++ = org[1]; *fp++ = org[2];
	*fp++ = dir[0]; *fp++ = dir[1]; *fp = dir[2];
}


rayflush(rt)			/* flush buffered rays */
register struct rtproc  *rt;
{
	register int  i;

	if (rt->nrays <= 0)
		return;
	i = 6*rt->nrays + 3;
	rt->buf[i++] = 0.; rt->buf[i++] = 0.; rt->buf[i] = 0.;
	if ( process(rt->pd, (char *)rt->buf, (char *)rt->buf,
			3*sizeof(float)*rt->nrays,
			6*sizeof(float)*(rt->nrays+1)) <
			3*sizeof(float)*rt->nrays )
		error(SYSTEM, "error reading from rtrace process");
	i = rt->nrays;
	while (i--) {
		rt->dest[i][0] += rt->buf[3*i];
		rt->dest[i][1] += rt->buf[3*i+1];
		rt->dest[i][2] += rt->buf[3*i+2];
	}
	rt->nrays = 0;
}


mkaxes(u, v, n)			/* compute u and v to go with n */
FVECT  u, v, n;
{
	register int  i;

	v[0] = v[1] = v[2] = 0.0;
	for (i = 0; i < 3; i++)
		if (n[i] < 0.6 && n[i] > -0.6)
			break;
	v[i] = 1.0;
	fcross(u, v, n);
	normalize(u);
	fcross(v, n, u);
}


rounddir(dv, alt, azi)		/* compute uniform spherical direction */
register FVECT  dv;
double  alt, azi;
{
	double  d1, d2;

	dv[2] = 1. - 2.*alt;
	d1 = sqrt(1. - dv[2]*dv[2]);
	d2 = 2.*PI * azi;
	dv[0] = d1*cos(d2);
	dv[1] = d1*sin(d2);
}


flatdir(dv, alt, azi)		/* compute uniform hemispherical direction */
register FVECT  dv;
double  alt, azi;
{
	double  d1, d2;

	d1 = sqrt(alt);
	d2 = 2.*PI * azi;
	dv[0] = d1*cos(d2);
	dv[1] = d1*sin(d2);
	dv[2] = sqrt(1. - d1*d1);
}
