/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  cone.c - routines for making cones
 *
 *     2/12/86
 */

#include  "standard.h"

#include  "object.h"

#include  "otypes.h"

#include  "cone.h"

/*
 *     In general, a cone may be any one of a cone, a cylinder, a ring,
 *  a cup (inverted cone), or a tube (inverted cylinder).
 *     Most cones are specified with a starting point and radius and
 *  an ending point and radius.  In the cases of a cylinder or tube,
 *  only one radius is needed.  In the case of a ring, a normal direction
 *  is specified instead of a second endpoint.
 *
 *	mtype (cone|cup) name
 *	0
 *	0
 *	8 P0x P0y P0z P1x P1y P1z R0 R1
 *
 *	mtype (cylinder|tube) name
 *	0
 *	0
 *	7 P0x P0y P0z P1x P1y P1z R
 *
 *	mtype ring name
 *	0
 *	0
 *	8 Px Py Pz Nx Ny Nz R0 R1
 */


CONE *
getcone(o, getxf)			/* get cone structure */
register OBJREC  *o;
int  getxf;
{
	double  fabs(), sqrt();
	register CONE  *co;

	if ((co = (CONE *)o->os) == NULL) {

		co = (CONE *)malloc(sizeof(CONE));
		if (co == NULL)
			error(SYSTEM, "out of memory in makecone");

		co->ca = o->oargs.farg;
						/* get radii */
		if (o->otype == OBJ_CYLINDER || o->otype == OBJ_TUBE) {
			if (o->oargs.nfargs != 7)
				goto argcerr;
			if (co->ca[6] <= FTINY)
				goto raderr;
			co->r0 = co->r1 = 6;
		} else {
			if (o->oargs.nfargs != 8)
				goto argcerr;
			if (co->ca[6] < 0.0 || co->ca[7] < 0.0)
				goto raderr;
			if (fabs(co->ca[7] - co->ca[6]) <= FTINY)
				goto raderr;
			co->r0 = 6;
			co->r1 = 7;
		}
						/* get axis orientation */
		co->p0 = 0;
		if (o->otype == OBJ_RING) {
			if (co->ca[6] > co->ca[7]) {	/* make r0 smaller */
				co->r0 = 7;
				co->r1 = 6;
			}
			co->p1 = 0;
			VCOPY(co->ad, o->oargs.farg+3);
		} else {
			co->p1 = 3;
			co->ad[0] = co->ca[3] - co->ca[0];
			co->ad[1] = co->ca[4] - co->ca[1];
			co->ad[2] = co->ca[5] - co->ca[2];
		}
		co->al = normalize(co->ad);
		if (co->al == 0.0)
			objerror(o, USER, "zero orientation");
					/* compute axis and side lengths */
		if (o->otype == OBJ_RING) {
			co->al = 0.0;
			co->sl = co->ca[co->r1] - co->ca[co->r0];
		} else if (o->otype == OBJ_CONE || o->otype == OBJ_CUP) {
			co->sl = co->ca[7] - co->ca[6];
			co->sl = sqrt(co->sl*co->sl + co->al*co->al);
		} else { /* OBJ_CYLINDER || OBJ_TUBE */
			co->sl = co->al;
		}
		co->tm = NULL;
		o->os = (char *)co;
	}
	if (getxf && co->tm == NULL)
		conexform(co);
	return(co);

argcerr:
	objerror(o, USER, "bad # arguments");
raderr:
	objerror(o, USER, "illegal radii");
}


freecone(o)			/* free memory associated with cone */
OBJREC  *o;
{
	register CONE  *co = (CONE *)o->os;

	if (co->tm != NULL)
		free((char *)co->tm);
	free(o->os);
	o->os = NULL;
}


conexform(co)			/* get cone transformation matrix */
register CONE  *co;
{
	double  sqrt(), fabs();
	double  m4[4][4];
	register double  d;
	register int  i;

	co->tm = (double (*)[4])malloc(sizeof(m4));
	if (co->tm == NULL)
		error(SYSTEM, "out of memory in conexform");

				/* translate to origin */
	setident4(co->tm);
	if (co->r0 == co->r1)
		d = 0.0;
	else
		d = co->ca[co->r0] / (co->ca[co->r1] - co->ca[co->r0]);
	for (i = 0; i < 3; i++)
		co->tm[3][i] = d*(co->ca[co->p1+i] - co->ca[co->p0+i])
				- co->ca[co->p0+i];
	
				/* rotate to positive z-axis */
	setident4(m4);
	d = co->ad[1]*co->ad[1] + co->ad[2]*co->ad[2];
	if (d <= FTINY*FTINY) {
		m4[0][0] = 0.0;
		m4[0][2] = co->ad[0];
		m4[2][0] = -co->ad[0];
		m4[2][2] = 0.0;
	} else {
		d = sqrt(d);
		m4[0][0] = d;
		m4[1][0] = -co->ad[0]*co->ad[1]/d;
		m4[2][0] = -co->ad[0]*co->ad[2]/d;
		m4[1][1] = co->ad[2]/d;
		m4[2][1] = -co->ad[1]/d;
		m4[0][2] = co->ad[0];
		m4[1][2] = co->ad[1];
		m4[2][2] = co->ad[2];
	}
	multmat4(co->tm, co->tm, m4);

				/* scale z-axis */
	setident4(m4);
	if (co->p0 != co->p1 && co->r0 != co->r1) {
		d = fabs(co->ca[co->r1] - co->ca[co->r0]);
		m4[2][2] = d/co->al;
	}
	multmat4(co->tm, co->tm, m4);
}
