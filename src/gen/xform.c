/* Copyright (c) 1986 Regents of the University of California */

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

#include  <stdio.h>

#include  <ctype.h>


typedef struct {
	char  *funame;			/* function name */
	int  (*funp)();			/* pointer to function */
}  FUN;

int  xac;				/* global xform argument count */
char  **xav;				/* global xform argument pointer */
int  xfa;				/* start of xf arguments */

double  totxform[4][4];			/* total transformation matrix */
double  totscale;			/* total scale factor */
int  reverse;				/* boolean true if scene inverted */

int  expand = 0;			/* boolean true to expand commands */

char  *idprefix = NULL;			/* prefix for object identifiers */

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

FUN  ofun[] = {
	{ "source", o_source },
	{ "sphere", o_sphere },
	{ "bubble", o_sphere },
	{ "polygon", o_face },
	{ "cone", o_cone },
	{ "cup", o_cone },
	{ "cylinder", o_cylinder },
	{ "tube", o_cylinder },
	{ "ring", o_ring },
	{ "instance", addxform },
	{ "alias", alias },
	{ "antimatter", passargs },
	{ "glow", m_glow },
	{ "spotlight", m_spot },
	{ "dielectric", m_dielectric },
	{ "interface", m_interface },
	{ "colortext", text },
	{ "brighttext", text },
	{ "texfunc", addxform },
	{ "colorfunc", addxform },
	{ "brightfunc", addxform },
	{ "colorpict", addxform },
	{ "colordata", addxform },
	{ "brightdata", addxform },
	{ "mixfunc", addxform },
	{ "mixdata", addxform },
	{ "mixtext", text },
	{ "light", passargs },
	{ "illum", passargs },
	{ "plastic", passargs },
	{ "metal", passargs },
	{ "trans", passargs },
	{ "glass", passargs },
	{ 0 }					/* terminator */
};

#define  issurface(t)		((t)<=9)

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

	xav = argv;

	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'n':
				idprefix = argv[++a];
				continue;
			case 'e':
				expand = 1;
				continue;
			}
		break;
	}

	xfa = a;

	totscale = 1.0;
	setident4(totxform);

	a += xf(totxform, &totscale, argc-a, argv+a);

	if (reverse = totscale < 0.0)
		totscale = -totscale;

	xac = a;

	putchar('#');				/* simple header */
	for (a = 0; a < xac; a++)
		printf(" %s", xav[a]);
	putchar('\n');

	if (a == argc)
		xform("standard input", stdin);
	else
		for ( ; a < argc; a++) {
			if ((fp = fopen(argv[a], "r")) == NULL) {
				fprintf(stderr, "%s: cannot open \"%s\"\n",
						progname, argv[a]);
				exit(1);
			}
			xform(argv[a], fp);
			fclose(fp);
		}

	exit(0);
}


xform(name, fin)			/* transform stream by totxform */
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
			if (expand)
				xfcomm(name, fin);
			else {
				putchar('\n');
				while ((c = getc(fin)) != EOF && c != '\n')
					putchar(c);
				printf(" |");
				for (c = 0; c < xac; c++)
					printf(" %s", xav[c]);
				putchar('\n');
			}
		} else {				/* object */
			ungetc(c, fin);
			xfobject(name, fin);
		}
	}
}


xfcomm(fname, fin)				/* expand a command */
FILE  *fin;
{
	FILE  *popen();
	char  *fgets();
	FILE  *pin;
	char  buf[512];

	buf[0] = '\0';
	fgets(buf, sizeof(buf), fin);
	if (buf[0] && buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';
	if ((pin = popen(buf+1, "r")) == NULL) {
		fprintf(stderr, "%s: (%s): cannot execute \"%s\"\n",
				progname, fname, buf);
		exit(1);
	}
	xform(buf, pin);
	pclose(pin);
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


passargs(fin)			/* pass on arguments unchanged */
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

	for (i = 0; ofun[i].funame != NULL; i++)
		if (!strcmp(ofun[i].funame, ofname))
			return(i);

	return(-1);		/* not found */
}


alias(fin)			/* transfer alias */
FILE  *fin;
{
	char  alias[MAXSTR];

	if (fscanf(fin, "%s", alias) != 1)
		return(-1);
	printf("\t%s\n", alias);
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
	printf(" %18.12g\n", fa->farg[3] * totscale);
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
	multv3(v, fa->farg+4, totxform);
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
		pow(fa->farg[0], 1.0/totscale),
		pow(fa->farg[1], 1.0/totscale),
		pow(fa->farg[2], 1.0/totscale));
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
		pow(fa->farg[0], 1.0/totscale),
		pow(fa->farg[1], 1.0/totscale),
		pow(fa->farg[2], 1.0/totscale));
	printf(" %18.12g\n", fa->farg[3]);
	printf("%18.12g %18.12g %18.12g",
		pow(fa->farg[4], 1.0/totscale),
		pow(fa->farg[5], 1.0/totscale),
		pow(fa->farg[6], 1.0/totscale));
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
	multp3(v, fa->farg, totxform);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* right vector */
	multv3(v, fa->farg+3, totxform);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* down vector */
	multv3(v, fa->farg+6, totxform);
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
	multv3(dv, fa->farg, totxform);
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
	
	multp3(cent, fa->farg, totxform);	/* transform center */
	
	rad = fa->farg[3] * totscale;		/* scale radius */
	
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
			multp3(p, fa->farg+(fa->nfargs-i-3), totxform);
		else
			multp3(p, fa->farg+i, totxform);
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

	multp3(p0, fa->farg, totxform);
	multp3(p1, fa->farg+3, totxform);
	r0 = fa->farg[6] * totscale;
	r1 = fa->farg[7] * totscale;
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

	multp3(p0, fa->farg, totxform);
	multp3(p1, fa->farg+3, totxform);
	rad = fa->farg[6] * totscale;
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

	multp3(p0, fa->farg, totxform);
	multv3(pd, fa->farg+3, totxform);
	r0 = fa->farg[6] * totscale;
	r1 = fa->farg[7] * totscale;
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
