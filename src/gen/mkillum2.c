/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines to do the actual calcultion and output for mkillum
 */

#include  "mkillum.h"

#include  "face.h"

#include  "cone.h"

#include  "random.h"


printobj(mod, obj)		/* print out an object */
char  *mod;
register OBJREC  *obj;
{
	register int  i;

	printf("\n%s %s %s", mod, ofun[obj->otype].funame, obj->oname);
	printf("\n%d", obj->oargs.nsargs);
	for (i = 0; i < obj->oargs.nsargs; i++)
		printf(" %s", obj->oargs.sarg[i]);
#ifdef  IARGS
	printf("\n%d", obj->oargs.niargs);
	for (i = 0; i < obj->oargs.niargs; i++)
		printf(" %d", obj->oargs.iarg[i]);
#else
	printf("\n0");
#endif
	printf("\n%d", obj->oargs.nfargs);
	for (i = 0; i < obj->oargs.nfargs; i++) {
		if (i%3 == 0)
			putchar('\n');
		printf(" %18.12g", obj->oargs.farg[i]);
	}
	putchar('\n');
}


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
		printobj(il->altname, ob);
}


o_face(ob, il, rt, nm)		/* make an illum face */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
}


o_sphere(ob, il, rt, nm)	/* make an illum sphere */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
	int  dim[4];
	int  n, nalt, nazi;
	float  *distarr;
	double  r1, r2;
	FVECT  pos, dir;
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
			org[j] = obj->oargs.farg[j] + obj->oargs.farg[3] *
					( r1*cos(r2)*u[j] + r1*sin(r2)*v[j]
						- sqrt(1.01-r1*r1)*dir[j] );

					/* send sample */
		    raysamp(distarr+dim[1]*nazi+dim[2], org, dir, rt);
		}
	rayflush(rt);
				/* write out distribution */
	rounddist(distarr, nalt, nazi, il, ob);
				/* clean up */
	free((char *)distarr);
}


o_ring(ob, il, rt, nm)		/* make an illum ring */
OBJREC  *ob;
struct illum_args  *il;
struct rtproc  *rt;
char  *nm;
{
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
