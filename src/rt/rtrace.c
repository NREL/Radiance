/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  rtrace.c - program and variables for individual ray tracing.
 *
 *     6/11/86
 */

/*
 *  Input is in the form:
 *
 *	xorg	yorg	zorg	xdir	ydir	zdir
 *
 *  The direction need not be normalized.  Output is flexible.
 *  If the direction vector is (0,0,0), then the output is flushed.
 *  All values default to ascii representation of real
 *  numbers.  Binary representations can be selected
 *  with '-ff' for float or '-fd' for double.  By default,
 *  radiance is computed.  The '-i' or '-I' options indicate that
 *  irradiance values are desired.
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

int  dimlist[MAXDIM];			/* sampling dimensions */
int  ndims = 0;				/* number of sampling dimensions */
int  samplendx = 0;			/* index for this sample */

int  imm_irrad = 0;			/* compute immediate irradiance? */

int  inform = 'a';			/* input format */
int  outform = 'a';			/* output format */
char  *outvals = "v";			/* output specification */

int  hresolu = 0;			/* horizontal (scan) size */
int  vresolu = 0;			/* vertical resolution */

double  dstrsrc = 0.0;			/* square source distribution */
double  shadthresh = .05;		/* shadow threshold */
double  shadcert = .5;			/* shadow certainty */
int  directrelay = 1;			/* number of source relays */
int  vspretest = 512;			/* virtual source pretest density */
int  directinvis = 0;			/* sources invisible? */
double  srcsizerat = .25;		/* maximum ratio source size/dist. */

double  specthresh = .5;		/* specular sampling threshold */
double  specjitter = 1.;		/* specular sampling jitter */

int  maxdepth = 6;			/* maximum recursion depth */
double  minweight = 4e-3;		/* minimum ray weight */

COLOR  ambval = BLKCOLOR;		/* ambient value */
double  ambacc = 0.2;			/* ambient accuracy */
int  ambres = 32;			/* ambient resolution */
int  ambdiv = 128;			/* ambient divisions */
int  ambssamp = 0;			/* ambient super-samples */
int  ambounce = 0;			/* ambient bounces */
char  *amblist[128];			/* ambient include/exclude list */
int  ambincl = -1;			/* include == 1, exclude == 0 */

extern OBJREC  Lamb;			/* a Lambertian surface */

static RAY  thisray;			/* for our convenience */

static int  oputo(), oputd(), oputv(), oputl(), 
		oputp(), oputn(), oputs(), oputw(), oputm();

static int  (*ray_out[10])(), (*every_out[10])();
static int  castonly;

static int  puta(), putf(), putd();

static int  (*putreal)();


quit(code)			/* quit program */
int  code;
{
	exit(code);
}


rtrace(fname)				/* trace rays from file */
char  *fname;
{
	long  vcount = hresolu>1 ? hresolu*vresolu : vresolu;
	long  nextflush = hresolu;
	FILE  *fp;
	FVECT  orig, direc;
					/* set up input */
	if (fname == NULL)
		fp = stdin;
	else if ((fp = fopen(fname, "r")) == NULL) {
		sprintf(errmsg, "cannot open input file \"%s\"", fname);
		error(SYSTEM, errmsg);
	}
					/* set up output */
	setoutput(outvals);
	switch (outform) {
	case 'a': putreal = puta; break;
	case 'f': putreal = putf; break;
	case 'd': putreal = putd; break;
	}
					/* process file */
	while (getvec(orig, inform, fp) == 0 &&
			getvec(direc, inform, fp) == 0) {

		if (normalize(direc) == 0.0) {		/* zero ==> flush */
			fflush(stdout);
			continue;
		}
		samplendx++;
							/* compute and print */
		if (imm_irrad)
			irrad(orig, direc);
		else
			traceray(orig, direc);
							/* flush if time */
		if (--nextflush == 0) {
			fflush(stdout);
			nextflush = hresolu;
		}
		if (ferror(stdout))
			error(SYSTEM, "write error");
		if (--vcount == 0)			/* check for end */
			break;
	}
	if (vcount > 0)
		error(USER, "read error");
	fclose(fp);
}


setoutput(vs)				/* set up output tables */
register char  *vs;
{
	extern int  ourtrace(), (*trace)();
	register int (**table)() = ray_out;

	castonly = 1;
	while (*vs)
		switch (*vs++) {
		case 't':				/* trace */
			*table = NULL;
			table = every_out;
			trace = ourtrace;
			castonly = 0;
			break;
		case 'o':				/* origin */
			*table++ = oputo;
			break;
		case 'd':				/* direction */
			*table++ = oputd;
			break;
		case 'v':				/* value */
			*table++ = oputv;
			castonly = 0;
			break;
		case 'l':				/* length */
			*table++ = oputl;
			castonly = 0;
			break;
		case 'p':				/* point */
			*table++ = oputp;
			break;
		case 'n':				/* normal */
			*table++ = oputn;
			break;
		case 's':				/* surface */
			*table++ = oputs;
			break;
		case 'w':				/* weight */
			*table++ = oputw;
			break;
		case 'm':				/* modifier */
			*table++ = oputm;
			break;
		}
	*table = NULL;
}


traceray(org, dir)		/* compute and print ray value(s) */
FVECT  org, dir;
{
	register int  (**tp)();

	VCOPY(thisray.rorg, org);
	VCOPY(thisray.rdir, dir);
	rayorigin(&thisray, NULL, PRIMARY, 1.0);
	if (castonly)
		localhit(&thisray, &thescene) || sourcehit(&thisray);
	else
		rayvalue(&thisray);

	if (ray_out[0] == NULL)
		return;
	for (tp = ray_out; *tp != NULL; tp++)
		(**tp)(&thisray);
	if (outform == 'a')
		putchar('\n');
}


irrad(org, dir)			/* compute immediate irradiance value */
FVECT  org, dir;
{
	register int  i;

	for (i = 0; i < 3; i++) {
		thisray.rorg[i] = org[i] + dir[i];
		thisray.rdir[i] = -dir[i];
	}
	rayorigin(&thisray, NULL, PRIMARY, 1.0);
					/* pretend we hit surface */
	thisray.rot = 1.0;
	thisray.rod = 1.0;
	VCOPY(thisray.ron, dir);
	for (i = 0; i < 3; i++)		/* fudge factor */
		thisray.rop[i] = org[i] + 1e-4*dir[i];
					/* compute and print */
	(*ofun[Lamb.otype].funp)(&Lamb, &thisray);
	oputv(&thisray);
	if (outform == 'a')
		putchar('\n');
}


getvec(vec, fmt, fp)		/* get a vector from fp */
register FVECT  vec;
int  fmt;
FILE  *fp;
{
	extern char  *fgetword();
	static float  vf[3];
	char  buf[32];
	register int  i;

	switch (fmt) {
	case 'a':					/* ascii */
		for (i = 0; i < 3; i++) {
			if (fgetword(buf, sizeof(buf), fp) == NULL ||
					!isflt(buf))
				return(-1);
			vec[i] = atof(buf);
		}
		break;
	case 'f':					/* binary float */
		if (fread((char *)vf, sizeof(float), 3, fp) != 3)
			return(-1);
		vec[0] = vf[0]; vec[1] = vf[1]; vec[2] = vf[2];
		break;
	case 'd':					/* binary double */
		if (fread((char *)vec, sizeof(double), 3, fp) != 3)
			return(-1);
		break;
	}
	return(0);
}


static
ourtrace(r)				/* print ray values */
RAY  *r;
{
	register int  (**tp)();

	if (every_out[0] == NULL)
		return;
	tabin(r);
	for (tp = every_out; *tp != NULL; tp++)
		(**tp)(r);
	putchar('\n');
}


static
tabin(r)				/* tab in appropriate amount */
RAY  *r;
{
	register RAY  *rp;

	for (rp = r->parent; rp != NULL; rp = rp->parent)
		putchar('\t');
}


static
oputo(r)				/* print origin */
register RAY  *r;
{
	(*putreal)(r->rorg[0]);
	(*putreal)(r->rorg[1]);
	(*putreal)(r->rorg[2]);
}


static
oputd(r)				/* print direction */
register RAY  *r;
{
	(*putreal)(r->rdir[0]);
	(*putreal)(r->rdir[1]);
	(*putreal)(r->rdir[2]);
}


static
oputv(r)				/* print value */
register RAY  *r;
{
	(*putreal)(colval(r->rcol,RED));
	(*putreal)(colval(r->rcol,GRN));
	(*putreal)(colval(r->rcol,BLU));
}


static
oputl(r)				/* print length */
register RAY  *r;
{
	(*putreal)(r->rt);
}


static
oputp(r)				/* print point */
register RAY  *r;
{
	if (r->rot < FHUGE) {
		(*putreal)(r->rop[0]);
		(*putreal)(r->rop[1]);
		(*putreal)(r->rop[2]);
	} else {
		(*putreal)(0.0);
		(*putreal)(0.0);
		(*putreal)(0.0);
	}
}


static
oputn(r)				/* print normal */
register RAY  *r;
{
	if (r->rot < FHUGE) {
		(*putreal)(r->ron[0]);
		(*putreal)(r->ron[1]);
		(*putreal)(r->ron[2]);
	} else {
		(*putreal)(0.0);
		(*putreal)(0.0);
		(*putreal)(0.0);
	}
}


static
oputs(r)				/* print name */
register RAY  *r;
{
	if (r->ro != NULL)
		fputs(r->ro->oname, stdout);
	else
		putchar('*');
	putchar('\t');
}


static
oputw(r)				/* print weight */
register RAY  *r;
{
	(*putreal)(r->rweight);
}


static
oputm(r)				/* print modifier */
register RAY  *r;
{
	if (r->ro != NULL)
		fputs(objptr(r->ro->omod)->oname, stdout);
	else
		putchar('*');
	putchar('\t');
}


static
puta(v)				/* print ascii value */
double  v;
{
	printf("%e\t", v);
}


static
putd(v)				/* print binary double */
double  v;
{
	fwrite((char *)&v, sizeof(v), 1, stdout);
}


static
putf(v)				/* print binary float */
double  v;
{
	float f = v;

	fwrite((char *)&f, sizeof(f), 1, stdout);
}
