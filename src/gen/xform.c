/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  xform.c - program to transform object files.
 *		Transformations must preserve aspect ratio.
 *
 *     10/19/85
 *     11/6/86		Finally added error checking!
 */

#include  "standard.h"

#include  <ctype.h>

#include  "otypes.h"

int  xac;				/* global xform argument count */
char  **xav;				/* global xform argument pointer */
int  xfa;				/* start of xf arguments */

XF  tot;				/* total transformation */
int  reverse;				/* boolean true if scene inverted */

int  expand = 0;			/* boolean true to expand commands */

char  *idprefix = NULL;			/* prefix for object identifiers */

#define  ALIAS		NUMOTYPE	/* put alias at end of array */

#define  NUMTYPES	(NUMOTYPE+1)	/* total number of object types */

FUN  ofun[NUMTYPES] = INIT_OTYPE;	/* default types and actions */

typedef struct {
	short  nsargs;			/* # of string arguments */
	char  **sarg;			/* string arguments */
	short  niargs;			/* # of integer arguments */
	int  *iarg;			/* integer arguments */
	short  nfargs;			/* # of float arguments */
	double  *farg;			/* float arguments */
}  FUNARGS;

#define  MAXSTR		512		/* maximum string length */

FUNARGS  *getfargs();
char  *malloc();

#define  progname  (xav[0])


main(argc, argv)		/* get transform options and transform file */
int  argc;
char  *argv[];
{
	FILE  *fopen();
	FILE  *fp;
	int  a;
					/* check for array */
	for (a = 1; a < argc; a++)
		if (!strcmp(argv[a], "-a"))
			return(doarray(argc, argv, a));

	initotypes();

	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'n':
				if (argv[a][2] || a+1 >= argc)
					break;
				idprefix = argv[++a];
				continue;
			case 'e':
				if (argv[a][2])
					break;
				expand = 1;
				continue;
			}
		break;
	}

	xav = argv;
	xfa = a;

	a += xf(&tot, argc-a, argv+a);

	if (reverse = tot.sca < 0.0)
		tot.sca = -tot.sca;

	if (a < argc && argv[a][0] == '-') {
		fprintf(stderr, "%s: command line error at '%s'\n",
				argv[0], argv[a]);
		exit(1);
	}

	xac = a;
					/* simple header */
	putchar('#');
	for (a = 0; a < xac; a++)
		printf(" %s", xav[a]);
	putchar('\n');
					/* transform input */
	if (xac == argc)
		xform("standard input", stdin);
	else
		for (a = xac; a < argc; a++) {
			if ((fp = fopen(argv[a], "r")) == NULL) {
				fprintf(stderr, "%s: cannot open \"%s\"\n",
						progname, argv[a]);
				exit(1);
			}
			xform(argv[a], fp);
			fclose(fp);
		}

	return(0);
}


doarray(ac, av, ai)			/* make array */
char  **av;
int  ac, ai;
{
	char  *newav[256], **avp;
	char  newid[128], repts[32];
	char  *oldid = NULL;
	int  i, err;
	
	avp = newav+2;
	avp[0] = av[0];
	for (i = 1; i < ac; i++)
		if (!strcmp(av[i-1], "-n")) {
			oldid = av[i];
			avp[i] = newid;
		} else
			avp[i] = av[i];
	avp[ai] = "-i";
	avp[ai+1] = repts;
	avp[i] = NULL;
	if (oldid == NULL) {
		newav[0] = av[0];
		newav[1] = "-n";
		newav[2] = newid;
		avp = newav;
		ac += 2;
	}
	err = 0;
	for (i = 0; i < atoi(av[ai+1]); i++) {
		if (oldid == NULL)
			sprintf(newid, "a%d", i);
		else
			sprintf(newid, "%s.%d", oldid, i);
		sprintf(repts, "%d", i);
		err |= main(ac, avp);
	}
	return(err);
}


xform(name, fin)			/* transform stream by tot.xfm */
char  *name;
register FILE  *fin;
{
	register int  c;

	while ((c = getc(fin)) != EOF) {
		if (isspace(c))				/* blank */
			continue;
		if (c == '#') {				/* comment */
			putchar(c);
			do {
				if ((c = getc(fin)) == EOF)
					return;
				putchar(c);
			} while (c != '\n');
		} else if (c == '!') {			/* command */
			ungetc(c, fin);
			xfcomm(name, fin);
		} else {				/* object */
			ungetc(c, fin);
			xfobject(name, fin);
		}
	}
}


xfcomm(fname, fin)			/* transform a command */
FILE  *fin;
{
	FILE  *popen();
	char  *fgetline();
	FILE  *pin;
	char  buf[512];
	int  i;

	fgetline(buf, sizeof(buf), fin);
	if (expand) {
		if (xac > 2) {
			if ((pin = popen(buf+1, "r")) == NULL) {
				fprintf(stderr,
				"%s: (%s): cannot execute \"%s\"\n",
						progname, fname, buf);
				exit(1);
			}
			xform(buf, pin);
			pclose(pin);
		} else {
			fflush(stdout);
			system(buf+1);
		}
	} else {
		printf("\n%s", buf);
		if (xac > 1) {
			printf(" | %s -e", xav[0]);
			for (i = 1; i < xac; i++)
				printf(" %s", xav[i]);
		}
		putchar('\n');
	}
}


xfobject(fname, fin)				/* transform an object */
char  *fname;
FILE  *fin;
{
	char  stmp[MAXSTR];
	int  fn;
	
	fscanf(fin, "%s", stmp);	/* modifier */
	printf("\n%s ", stmp);
	fscanf(fin, "%s", stmp);	/* object type */
	if ((fn = otype(stmp)) < 0) {
		fprintf(stderr, "%s: (%s): unknown object type \"%s\"\n",
				progname, fname, stmp);
		exit(1);
	}
	printf("%s ", stmp);
	fscanf(fin, "%s", stmp);	/* object name */
	if (idprefix != NULL && issurface(fn))
		printf("%s.%s\n", idprefix, stmp);
	else
		printf("%s\n", stmp);
					/* transform arguments */
	if ((*ofun[fn].funp)(fin) < 0) {
		fprintf(stderr, "%s: (%s): bad %s \"%s\"\n",
				progname, fname, ofun[fn].funame, stmp);
		exit(1);
	}
}


o_default(fin)			/* pass on arguments unchanged */
FILE  *fin;
{
	register int  i;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
					/* string arguments */
	printf("%d", fa->nsargs);
	for (i = 0; i < fa->nsargs; i++)
		printf(" %s", fa->sarg[i]);
	printf("\n");
					/* integer arguments */
	printf("%d", fa->niargs);
	for (i = 0; i < fa->niargs; i++)
		printf(" %d", fa->iarg[i]);
	printf("\n");
					/* float arguments */
	printf("%d", fa->nfargs);
	for (i = 0; i < fa->nfargs; i++)
		printf(" %18.12g", fa->farg[i]);
	printf("\n");
	freefargs(fa);
	return(0);
}


addxform(fin)			/* add xf arguments to strings */
FILE  *fin;
{
	register int  i;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
					/* string arguments */
	printf("%d", fa->nsargs + xac-xfa);
	for (i = 0; i < fa->nsargs; i++)
		printf(" %s", fa->sarg[i]);
	for (i = xfa; i < xac; i++)	/* add xf arguments */
		printf(" %s", xav[i]);
	printf("\n");
					/* integer arguments */
	printf("%d", fa->niargs);
	for (i = 0; i < fa->niargs; i++)
		printf(" %d", fa->iarg[i]);
	printf("\n");
					/* float arguments */
	printf("%d", fa->nfargs);
	for (i = 0; i < fa->nfargs; i++)
		printf(" %18.12g", fa->farg[i]);
	printf("\n");
	freefargs(fa);
	return(0);
}


int
otype(ofname)			/* get object function number from its name */
register char  *ofname;
{
	register int  i;

	for (i = 0; i < NUMTYPES; i++)
		if (!strcmp(ofun[i].funame, ofname))
			return(i);

	return(-1);		/* not found */
}


alias(fin)			/* transfer alias */
FILE  *fin;
{
	char  aliasnm[MAXSTR];

	if (fscanf(fin, "%s", aliasnm) != 1)
		return(-1);
	printf("\t%s\n", aliasnm);
	return(0);
}


m_glow(fin)			/* transform arguments for proximity light */
FILE  *fin;
{
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 4)
		return(-1);
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g",
			fa->farg[0], fa->farg[1], fa->farg[2]);
	printf(" %18.12g\n", fa->farg[3] * tot.sca);
	freefargs(fa);
	return(0);
}


m_spot(fin)			/* transform arguments for spotlight */
FILE  *fin;
{
	double  v[3];
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 7)
		return(-1);
	printf("0\n0\n7");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			fa->farg[0], fa->farg[1], fa->farg[2], fa->farg[3]);
	multv3(v, fa->farg+4, tot.xfm);
	printf("\t%18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
	freefargs(fa);
	return(0);
}


m_dielectric(fin)		/* transform arguments for dielectric */
FILE  *fin;
{
	double  pow();
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 5)
		return(-1);
	printf("0\n0\n5");
	printf(" %18.12g %18.12g %18.12g",
		pow(fa->farg[0], 1.0/tot.sca),
		pow(fa->farg[1], 1.0/tot.sca),
		pow(fa->farg[2], 1.0/tot.sca));
	printf(" %18.12g %18.12g\n", fa->farg[3], fa->farg[4]);
	freefargs(fa);
	return(0);
}


m_interface(fin)		/* transform arguments for interface */
FILE  *fin;
{
	double  pow();
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 8)
		return(-1);
	printf("0\n0\n8\n");
	printf("%18.12g %18.12g %18.12g",
		pow(fa->farg[0], 1.0/tot.sca),
		pow(fa->farg[1], 1.0/tot.sca),
		pow(fa->farg[2], 1.0/tot.sca));
	printf(" %18.12g\n", fa->farg[3]);
	printf("%18.12g %18.12g %18.12g",
		pow(fa->farg[4], 1.0/tot.sca),
		pow(fa->farg[5], 1.0/tot.sca),
		pow(fa->farg[6], 1.0/tot.sca));
	printf(" %18.12g\n", fa->farg[7]);
	freefargs(fa);
	return(0);
}


text(fin)			/* transform text arguments */
FILE  *fin;
{
	int  i;
	double  v[3];
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->niargs != 0 || (fa->nfargs != 9 &&
			fa->nfargs != 11 && fa->nfargs != 15))
		return(-1);
					/* string arguments */
	printf("%d", fa->nsargs);
	for (i = 0; i < fa->nsargs; i++)
		printf(" %s", fa->sarg[i]);
	printf("\n0\n%d\n", fa->nfargs);
					/* anchor point */
	multp3(v, fa->farg, tot.xfm);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* right vector */
	multv3(v, fa->farg+3, tot.xfm);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* down vector */
	multv3(v, fa->farg+6, tot.xfm);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* forground and background */
	if (fa->nfargs == 11)
		printf(" %18.12g %18.12g\n", fa->farg[9], fa->farg[10]);
	else if (fa->nfargs == 15) {
		printf(" %18.12g %18.12g %18.12g\n",
				fa->farg[9], fa->farg[10], fa->farg[11]);
		printf(" %18.12g %18.12g %18.12g\n",
				fa->farg[12], fa->farg[13], fa->farg[14]);
	}
	freefargs(fa);
	return(0);
}


o_source(fin)			/* transform source arguments */
FILE  *fin;
{
	double  dv[3];
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 4)
		return(-1);
					/* transform direction vector */
	multv3(dv, fa->farg, tot.xfm);
					/* output */
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			dv[0], dv[1], dv[2], fa->farg[3]);
	freefargs(fa);
	return(0);
}


o_sphere(fin)			/* transform sphere arguments */
FILE  *fin;
{
	double  cent[3], rad;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 4)
		return(-1);
	
	multp3(cent, fa->farg, tot.xfm);	/* transform center */
	
	rad = fa->farg[3] * tot.sca;		/* scale radius */
	
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			cent[0], cent[1], cent[2], rad);
	freefargs(fa);
	return(0);
}


o_face(fin)			/* transform face arguments */
FILE  *fin;
{
	double  p[3];
	register int  i;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs % 3)
		return(-1);
	
	printf("0\n0\n%d\n", fa->nfargs);
	
	for (i = 0; i < fa->nfargs; i += 3) {
		if (reverse)
			multp3(p, fa->farg+(fa->nfargs-i-3), tot.xfm);
		else
			multp3(p, fa->farg+i, tot.xfm);
		printf(" %18.12g %18.12g %18.12g\n", p[0], p[1], p[2]);
	}
	freefargs(fa);
	return(0);
}


o_cone(fin)			/* transform cone and cup arguments */
FILE  *fin;
{
	double  p0[3], p1[3], r0, r1;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 8)
		return(-1);

	printf("0\n0\n8\n");

	multp3(p0, fa->farg, tot.xfm);
	multp3(p1, fa->farg+3, tot.xfm);
	r0 = fa->farg[6] * tot.sca;
	r1 = fa->farg[7] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", p1[0], p1[1], p1[2]);
	printf(" %18.12g %18.12g\n", r0, r1);

	freefargs(fa);
	return(0);
}


o_cylinder(fin)			/* transform cylinder and tube arguments */
FILE  *fin;
{
	double  p0[3], p1[3], rad;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 7)
		return(-1);

	printf("0\n0\n7\n");

	multp3(p0, fa->farg, tot.xfm);
	multp3(p1, fa->farg+3, tot.xfm);
	rad = fa->farg[6] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", p1[0], p1[1], p1[2]);
	printf(" %18.12g\n", rad);
	freefargs(fa);
	return(0);
}


o_ring(fin)			/* transform ring arguments */
FILE  *fin;
{
	double  p0[3], pd[3], r0, r1;
	register FUNARGS  *fa;

	if ((fa = getfargs(fin)) == NULL)
		return(-1);
	if (fa->nsargs != 0 || fa->niargs != 0 || fa->nfargs != 8)
		return(-1);

	printf("0\n0\n8\n");

	multp3(p0, fa->farg, tot.xfm);
	multv3(pd, fa->farg+3, tot.xfm);
	r0 = fa->farg[6] * tot.sca;
	r1 = fa->farg[7] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", pd[0], pd[1], pd[2]);
	printf(" %18.12g %18.12g\n", r0, r1);
	freefargs(fa);
	return(0);
}


FUNARGS *
getfargs(fp)		/* get function arguments from stream */
FILE  *fp;
{
	char  *strcpy();
	char  sbuf[MAXSTR];
	int  n;
	register FUNARGS  *fa;
	register int  i;

	if ((fa = (FUNARGS *)malloc((unsigned)sizeof(FUNARGS))) == NULL)
		goto memerr;
	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(NULL);
	if (fa->nsargs = n) {
		fa->sarg = (char **)malloc((unsigned)fa->nsargs*sizeof(char *));
		if (fa->sarg == NULL)
			goto memerr;
		for (i = 0; i < fa->nsargs; i++) {
			if (fscanf(fp, "%s", sbuf) != 1)
				return(NULL);
			if ((fa->sarg[i] = malloc((unsigned)strlen(sbuf)+1)) == NULL)
				goto memerr;
			strcpy(fa->sarg[i], sbuf);
		}
	} else
		fa->sarg = NULL;
	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(NULL);
	if (fa->niargs = n) {
		fa->iarg = (int *)malloc((unsigned)fa->niargs*sizeof(int));
		if (fa->iarg == NULL)
			goto memerr;
		for (i = 0; i < fa->niargs; i++)
			if (fscanf(fp, "%d", &fa->iarg[i]) != 1)
				return(NULL);
	} else
		fa->iarg = NULL;
	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(NULL);
	if (fa->nfargs = n) {
		fa->farg = (double *)malloc((unsigned)fa->nfargs*sizeof(double));
		if (fa->farg == NULL)
			goto memerr;
		for (i = 0; i < fa->nfargs; i++)
			if (fscanf(fp, "%lf", &fa->farg[i]) != 1)
				return(NULL);
	} else
		fa->farg = NULL;
	return(fa);
memerr:
	fprintf(stderr, "%s: out of memory in getfargs\n", progname);
	exit(1);
}


freefargs(fa)			/* free memory associated with fa */
register FUNARGS  *fa;
{
	register int  i;

	for (i = 0; i < fa->nsargs; i++)
		free(fa->sarg[i]);
	if (fa->nsargs)
		free(fa->sarg);
	if (fa->niargs)
		free(fa->iarg);
	if (fa->nfargs)
		free(fa->farg);
	free(fa);
}


initotypes()			/* initialize ofun[] array */
{
	extern int  o_source();
	extern int  o_sphere();
	extern int  o_face();
	extern int  o_cone();
	extern int  o_cylinder();
	extern int  o_ring();
	extern int  m_glow();
	extern int  m_spot();
	extern int  m_dielectric();
	extern int  m_interface();
	extern int  text();
	extern int  alias();
	extern int  passargs();
	extern int  addxform();
	register int  i;

	if (ofun[OBJ_SOURCE].funp == o_source)
		return;			/* done already */
					/* alias is additional */
	ofun[ALIAS].funame = ALIASID;
	ofun[ALIAS].flags = 0;
					/* functions get new transform */
	for (i = 0; i < NUMTYPES; i++)
		if (hasfunc(i))
			ofun[i].funp = addxform;
					/* special cases */
	ofun[OBJ_SOURCE].funp = o_source;
	ofun[OBJ_SPHERE].funp =
	ofun[OBJ_BUBBLE].funp = o_sphere;
	ofun[OBJ_FACE].funp = o_face;
	ofun[OBJ_CONE].funp =
	ofun[OBJ_CUP].funp = o_cone;
	ofun[OBJ_CYLINDER].funp =
	ofun[OBJ_TUBE].funp = o_cylinder;
	ofun[OBJ_RING].funp = o_ring;
	ofun[OBJ_INSTANCE].funp = addxform;
	ofun[MAT_GLOW].funp = m_glow;
	ofun[MAT_SPOT].funp = m_spot;
	ofun[MAT_DIELECTRIC].funp = m_dielectric;
	ofun[MAT_INTERFACE].funp = m_interface;
	ofun[PAT_CTEXT].funp =
	ofun[PAT_BTEXT].funp =
	ofun[MIX_TEXT].funp = text;
	ofun[ALIAS].funp = alias;
}
