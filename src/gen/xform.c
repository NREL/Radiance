/* Copyright (c) 1996 Regents of the University of California */

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

#include  "paths.h"

#include  <ctype.h>

#include  "object.h"

#include  "otypes.h"

int  xac;				/* global xform argument count */
char  **xav;				/* global xform argument pointer */
int  xfa;				/* start of xf arguments */

XF  tot;				/* total transformation */
int  reverse;				/* boolean true if scene mirrored */

int  invert = 0;			/* boolean true to invert surfaces */

int  expand = 0;			/* boolean true to expand commands */

char  *newmod = NULL;			/* new modifier for surfaces */

char  *idprefix = NULL;			/* prefix for object identifiers */

#define	 ALIAS		NUMOTYPE	/* put alias at end of array */

#define	 NUMTYPES	(NUMOTYPE+1)	/* total number of object types */

FUN  ofun[NUMTYPES] = INIT_OTYPE;	/* default types and actions */

short  tinvers[NUMOTYPE];		/* inverse types for surfaces */

int  nrept = 1;				/* number of array repetitions */

extern char  *malloc(), *fgetword();

char  *mainfn[MAXPATH];			/* main file name */
FILE  *mainfp = NULL;			/* main file pointer */

#define	 progname  (xav[0])


main(argc, argv)		/* get transform options and transform file */
int  argc;
char  *argv[];
{
	char  *fname;
	int  a;
					/* check for array */
	for (a = 1; a < argc; a++)
		if (!strcmp(argv[a], "-a"))
			return(doarray(argc, argv, a));

	initotypes();

	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'm':
				if (argv[a][2] || a+1 >= argc)
					break;
				newmod = argv[++a];
				continue;
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
			case 'I':
				if (argv[a][2])
					break;
				invert = 1;
				continue;
			}
		break;
	}

	xav = argv;
	xfa = a;

	a += xf(&tot, argc-a, argv+a);

	if (reverse = tot.sca < 0.0)
		tot.sca = -tot.sca;
	if (invert)
		reverse = !reverse;

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
	if (xac == argc) {
		openmain(NULL);
		xform(mainfn, mainfp);
	} else
		for (a = xac; a < argc; a++) {
			openmain(argv[a]);
			xform(mainfn, mainfp);
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
	int  n, i, err;
	
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
	nrept *= n = atoi(av[ai+1]);
	err = 0;
	for (i = 0; i < n; i++) {
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
	int  nobjs = 0;
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
			nobjs++;
		} else {				/* object */
			ungetc(c, fin);
			xfobject(name, fin);
			nobjs++;
		}
	}
	if (nobjs == 0)
		fprintf(stderr, "%s: (%s): warning - empty file\n",
				progname, name);
}


xfcomm(fname, fin)			/* transform a command */
char  *fname;
FILE  *fin;
{
	extern FILE  *popen();
	extern char  *fgetline();
	FILE  *pin;
	char  buf[512];
	int  i;

	fgetline(buf, sizeof(buf), fin);
	if (expand) {
		if ((pin = popen(buf+1, "r")) == NULL) {
			fprintf(stderr, "%s: (%s): cannot execute \"%s\"\n",
					progname, fname, buf);
			exit(1);
		}
		xform(buf, pin);
		pclose(pin);
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
	extern char  *strcpy();
	char  typ[16], nam[MAXSTR];
	int  fn;
						/* modifier and type */
	strcpy(typ, "EOF");
	fgetword(nam, sizeof(nam), fin);
	fgetword(typ, sizeof(typ), fin);
	if ((fn = otype(typ)) < 0) {
		fprintf(stderr, "%s: (%s): unknown object type \"%s\"\n",
				progname, fname, typ);
		exit(1);
	}
	if (ismodifier(fn))
		printf("\n%s %s ", nam, typ);
	else
		printf("\n%s %s ", newmod != NULL ? newmod : nam,
				invert ? ofun[tinvers[fn]].funame : typ);
						/* object name */
	fgetword(nam, sizeof(nam), fin);
	if (idprefix == NULL || ismodifier(fn))
		printf("%s\n", nam);
	else
		printf("%s.%s\n", idprefix, nam);
						/* transform arguments */
	if ((*ofun[fn].funp)(fin) < 0) {
		fprintf(stderr, "%s: (%s): bad %s \"%s\"\n",
				progname, fname, ofun[fn].funame, nam);
		exit(1);
	}
}


o_default(fin)			/* pass on arguments unchanged */
FILE  *fin;
{
	register int  i;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
					/* string arguments */
	printf("%d", fa.nsargs);
	for (i = 0; i < fa.nsargs; i++)
		printf(" %s", fa.sarg[i]);
	printf("\n");
#ifdef	IARGS
					/* integer arguments */
	printf("%d", fa.niargs);
	for (i = 0; i < fa.niargs; i++)
		printf(" %d", fa.iarg[i]);
	printf("\n");
#else
	printf("0\n");
#endif
					/* float arguments */
	printf("%d", fa.nfargs);
	for (i = 0; i < fa.nfargs; i++)
		printf(" %18.12g", fa.farg[i]);
	printf("\n");
	freefargs(&fa);
	return(0);
}


addxform(fin)			/* add xf arguments to strings */
FILE  *fin;
{
	register int  i;
	int  resetarr = 0;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
					/* string arguments */
	if (xac > xfa && strcmp(xav[xfa], "-i"))
		resetarr = 2;
	printf("%d", fa.nsargs + resetarr + xac-xfa);
	for (i = 0; i < fa.nsargs; i++)
		printf(" %s", fa.sarg[i]);
	if (resetarr)
		printf(" -i 1");
	for (i = xfa; i < xac; i++)	/* add xf arguments */
		printf(" %s", xav[i]);
	printf("\n");
#ifdef	IARGS
					/* integer arguments */
	printf("%d", fa.niargs);
	for (i = 0; i < fa.niargs; i++)
		printf(" %d", fa.iarg[i]);
	printf("\n");
#else
	printf("0\n");
#endif
					/* float arguments */
	printf("%d", fa.nfargs);
	for (i = 0; i < fa.nfargs; i++)
		printf(" %18.12g", fa.farg[i]);
	printf("\n");
	freefargs(&fa);
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

	if (fgetword(aliasnm, MAXSTR, fin) == NULL)
		return(-1);
	printf("\t%s\n", aliasnm);
	return(0);
}


m_glow(fin)			/* transform arguments for proximity light */
FILE  *fin;
{
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 4)
		return(-1);
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g",
			fa.farg[0], fa.farg[1], fa.farg[2]);
	printf(" %18.12g\n", fa.farg[3] * tot.sca);
	freefargs(&fa);
	return(0);
}


m_spot(fin)			/* transform arguments for spotlight */
FILE  *fin;
{
	FVECT  v;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 7)
		return(-1);
	printf("0\n0\n7");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			fa.farg[0], fa.farg[1], fa.farg[2], fa.farg[3]);
	multv3(v, fa.farg+4, tot.xfm);
	printf("\t%18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
	freefargs(&fa);
	return(0);
}


m_mist(fin)		/* transform arguments for mist */
FILE  *fin;
{
	FUNARGS	 fa;
	int	i;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nfargs > 5)
		return(-1);
	printf("%d", fa.nsargs);
	if (idprefix == NULL)
		for (i = 0; i < fa.nsargs; i++)
			printf(" %s", fa.sarg[i]);
	else
		for (i = 0; i < fa.nsargs; i++) {
			char	sname[256], *sp;
			register char	*cp1, *cp2 = sname;
							/* add idprefix */
			for (sp = fa.sarg[i]; *sp; sp = cp1) {
				for (cp1 = idprefix; *cp1; )
					*cp2++ = *cp1++;
				*cp2++ = '.';
				for (cp1 = sp; *cp1 &&
						(*cp2++ = *cp1++) != '>'; )
					;
			}
			*cp2 = '\0';
			printf(" %s", sname);
		}
	printf("\n0\n%d", fa.nfargs);
	if (fa.nfargs > 2)
		printf(" %12.6g %12.6g %12.6g", fa.farg[0]/tot.sca,
				fa.farg[1]/tot.sca, fa.farg[2]/tot.sca);
	if (fa.nfargs > 3)
		printf(" %12.6g", fa.farg[3]);
	if (fa.nfargs > 4)
		printf(" %12.6g", fa.farg[4]);
	printf("\n");
	freefargs(&fa);
	return(0);
}


m_dielectric(fin)		/* transform arguments for dielectric */
FILE  *fin;
{
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 5)
		return(-1);
	printf("0\n0\n5");
	printf(" %12.6g %12.6g %12.6g",
		pow(fa.farg[0], 1.0/tot.sca),
		pow(fa.farg[1], 1.0/tot.sca),
		pow(fa.farg[2], 1.0/tot.sca));
	printf(" %12.6g %12.6g\n", fa.farg[3], fa.farg[4]);
	freefargs(&fa);
	return(0);
}


m_interface(fin)		/* transform arguments for interface */
FILE  *fin;
{
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 8)
		return(-1);
	printf("0\n0\n8\n");
	printf("%12.6g %12.6g %12.6g",
		pow(fa.farg[0], 1.0/tot.sca),
		pow(fa.farg[1], 1.0/tot.sca),
		pow(fa.farg[2], 1.0/tot.sca));
	printf(" %12.6g\n", fa.farg[3]);
	printf("%12.6g %12.6g %12.6g",
		pow(fa.farg[4], 1.0/tot.sca),
		pow(fa.farg[5], 1.0/tot.sca),
		pow(fa.farg[6], 1.0/tot.sca));
	printf(" %12.6g\n", fa.farg[7]);
	freefargs(&fa);
	return(0);
}


text(fin)			/* transform text arguments */
FILE  *fin;
{
	int  i;
	FVECT  v;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nfargs < 9)
		return(-1);
					/* string arguments */
	printf("%d", fa.nsargs);
	for (i = 0; i < fa.nsargs; i++)
		printf(" %s", fa.sarg[i]);
	printf("\n0\n%d\n", fa.nfargs);
					/* anchor point */
	multp3(v, fa.farg, tot.xfm);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* right vector */
	multv3(v, fa.farg+3, tot.xfm);
	printf(" %18.12g %18.12g %18.12g\n", v[0], v[1], v[2]);
					/* down vector */
	multv3(v, fa.farg+6, tot.xfm);
	printf(" %18.12g %18.12g %18.12g", v[0], v[1], v[2]);
					/* remaining arguments */
	for (i = 9; i < fa.nfargs; i++) {
		if (i%3 == 0)
			putchar('\n');
		printf(" %18.12g", fa.farg[i]);
	}
	putchar('\n');
	freefargs(&fa);
	return(0);
}


o_source(fin)			/* transform source arguments */
FILE  *fin;
{
	FVECT  dv;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 4)
		return(-1);
					/* transform direction vector */
	multv3(dv, fa.farg, tot.xfm);
					/* output */
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			dv[0], dv[1], dv[2], fa.farg[3]);
	freefargs(&fa);
	return(0);
}


o_sphere(fin)			/* transform sphere arguments */
FILE  *fin;
{
	FVECT  cent;
	double	rad;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 4)
		return(-1);
	
	multp3(cent, fa.farg, tot.xfm); /* transform center */
	
	rad = fa.farg[3] * tot.sca;		/* scale radius */
	
	printf("0\n0\n4");
	printf(" %18.12g %18.12g %18.12g %18.12g\n",
			cent[0], cent[1], cent[2], rad);
	freefargs(&fa);
	return(0);
}


o_face(fin)			/* transform face arguments */
FILE  *fin;
{
	FVECT  p;
	register int  i;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs % 3)
		return(-1);
	
	printf("0\n0\n%d\n", fa.nfargs);
	
	for (i = 0; i < fa.nfargs; i += 3) {
		if (reverse)
			multp3(p, fa.farg+(fa.nfargs-i-3), tot.xfm);
		else
			multp3(p, fa.farg+i, tot.xfm);
		printf(" %18.12g %18.12g %18.12g\n", p[0], p[1], p[2]);
	}
	freefargs(&fa);
	return(0);
}


o_cone(fin)			/* transform cone and cup arguments */
FILE  *fin;
{
	FVECT  p0, p1;
	double	r0, r1;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 8)
		return(-1);

	printf("0\n0\n8\n");

	multp3(p0, fa.farg, tot.xfm);
	multp3(p1, fa.farg+3, tot.xfm);
	r0 = fa.farg[6] * tot.sca;
	r1 = fa.farg[7] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", p1[0], p1[1], p1[2]);
	printf(" %18.12g %18.12g\n", r0, r1);

	freefargs(&fa);
	return(0);
}


o_cylinder(fin)			/* transform cylinder and tube arguments */
FILE  *fin;
{
	FVECT  p0, p1;
	double	rad;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 7)
		return(-1);

	printf("0\n0\n7\n");

	multp3(p0, fa.farg, tot.xfm);
	multp3(p1, fa.farg+3, tot.xfm);
	rad = fa.farg[6] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", p1[0], p1[1], p1[2]);
	printf(" %18.12g\n", rad);
	freefargs(&fa);
	return(0);
}


o_ring(fin)			/* transform ring arguments */
FILE  *fin;
{
	FVECT  p0, pd;
	double	r0, r1;
	FUNARGS	 fa;

	if (readfargs(&fa, fin) != 1)
		return(-1);
	if (fa.nsargs != 0  || fa.nfargs != 8)
		return(-1);

	printf("0\n0\n8\n");

	multp3(p0, fa.farg, tot.xfm);
	multv3(pd, fa.farg+3, tot.xfm);
	if (invert) {
		pd[0] = -pd[0];
		pd[1] = -pd[1];
		pd[2] = -pd[2];
	}
	r0 = fa.farg[6] * tot.sca;
	r1 = fa.farg[7] * tot.sca;
	printf(" %18.12g %18.12g %18.12g\n", p0[0], p0[1], p0[2]);
	printf(" %18.12g %18.12g %18.12g\n", pd[0], pd[1], pd[2]);
	printf(" %18.12g %18.12g\n", r0, r1);
	freefargs(&fa);
	return(0);
}


initotypes()			/* initialize ofun[] array */
{
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
	ofun[MAT_MIST].funp = m_mist;
	ofun[PAT_CTEXT].funp =
	ofun[PAT_BTEXT].funp =
	ofun[MIX_TEXT].funp = text;
	ofun[ALIAS].funp = alias;
					/* surface inverses */
	tinvers[OBJ_FACE] = OBJ_FACE;
	tinvers[OBJ_SOURCE] = OBJ_SOURCE;
	tinvers[OBJ_CONE] = OBJ_CUP;
	tinvers[OBJ_CUP] = OBJ_CONE;
	tinvers[OBJ_SPHERE] = OBJ_BUBBLE;
	tinvers[OBJ_BUBBLE] = OBJ_SPHERE;
	tinvers[OBJ_RING] = OBJ_RING;
	tinvers[OBJ_CYLINDER] = OBJ_TUBE;
	tinvers[OBJ_TUBE] = OBJ_CYLINDER;
	tinvers[OBJ_INSTANCE] = OBJ_INSTANCE;	/* oh, well */
}


#ifdef  OLDXFORM
openmain(fname)
char  *fname;
{
	if (fname == NULL) {
		strcpy(mainfn, "standard input");
		mainfp = stdin;
		return;
	}
	if (mainfp != NULL) {
		if (!strcmp(fname, mainfn)) {
			rewind(mainfp);
			return;
		}
		fclose(mainfp);
	}
	if ((mainfp = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open file \"%s\"\n",
				progname, fname);
		exit(1);
	}
	strcpy(mainfn, fname);
}
#else
openmain(fname)		/* open fname for input, changing to its directory */
char  *fname;
{
	extern FILE  *tmpfile();
	extern char  *getlibpath(), *getpath();
	static char  origdir[MAXPATH];
	static char  curdir[MAXPATH];
	char  newdir[MAXPATH], *cp;

	if (fname == NULL) {			/* standard input */
		if (mainfp == NULL) {
			register int  c;
			strcpy(mainfn, "standard input");
			if (nrept <= 1) {
				mainfp = stdin;
				return;			/* just read once */
			}
							/* else copy */
			if ((mainfp = tmpfile()) == NULL) {
				fprintf(stderr,
					"%s: cannot create temporary file\n",
						progname);
				exit(1);
			}
			while ((c = getc(stdin)) != EOF)
				putc(c, mainfp);
			fclose(stdin);
		}
		rewind(mainfp);			/* rewind copy */
		return;
	}
						/* get full path */
	if ((cp = getpath(fname, getlibpath(), R_OK)) == NULL) {
		fprintf(stderr, "%s: cannot find file \"%s\"\n",
				progname, fname);
		exit(1);
	}
	if (cp[0] == '.' && ISDIRSEP(cp[1]))	/* remove leading ./ */
		cp += 2;
	if (mainfp == NULL) {			/* first call, initialize */
		getwd(origdir);
		strcpy(curdir, origdir);
	} else if (!strcmp(cp, mainfn)) {	/* just need to rewind? */
		rewind(mainfp);
		return;
	} else					/* else close old stream */
		fclose(mainfp);
						/* record path name */
	strcpy(mainfn, cp);
	if (expand) {				/* get directory component */
		if (ISDIRSEP(cp[0]))
			strcpy(newdir, cp);
		else
			sprintf(newdir, "%s%c%s", origdir, DIRSEP, cp);
		for (cp = newdir+strlen(newdir); !ISDIRSEP(cp[-1]); cp--)
			;
		*cp = '\0';
		if (strcmp(newdir, curdir)) {	/* change to new directory? */
			if (chdir(newdir) < 0) {
				fprintf(stderr,
				"%s: cannot change directory to \"%s\"\n",
						progname, newdir);
				exit(1);
			}
			strcpy(curdir, newdir);
		}
						/* get final path component */
		for (cp = fname+strlen(fname);
				cp > fname && !ISDIRSEP(cp[-1]); cp--)
			;
	}
						/* open the file */
	if ((mainfp = fopen(cp, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open file \"%s\"\n",
				progname, mainfn);
		exit(1);
	}
}
#endif
