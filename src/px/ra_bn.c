/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ra_bn.c - program to convert between RADIANCE and barneyscan picture format.
 *
 *	10/12/88
 */

#include  <stdio.h>

#include  "color.h"

extern double  atof(), pow();

double	gamma = 2.0;			/* gamma correction */

char  *progname;

char  errmsg[128];

FILE	*rafp, *bnfp[3];

int  xmax, ymax;


main(argc, argv)
int  argc;
char  *argv[];
{
	int  reverse = 0;
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'g':
				gamma = atof(argv[++i]);
				break;
			case 'r':
				reverse = !reverse;
				break;
			default:
				goto userr;
			}
		else
			break;

	if (reverse) {
		if (i > argc-1 || i < argc-2)
			goto userr;
		if (openbarney(argv[i], "r") < 0) {
			sprintf(errmsg, "cannot open input \"%s\"", argv[i]);
			quiterr(errmsg);
		}
		if (i == argc-1 || !strcmp(argv[i+1], "-"))
			rafp = stdout;
		else if ((rafp = fopen(argv[i+1], "w")) == NULL) {
			sprintf(errmsg, "cannot open output \"%s\"",
					argv[i+1]);
			quiterr(errmsg);
		}
					/* put header */
		printargs(i, argv, rafp);
		putc('\n', rafp);
		fputresolu(YMAJOR|YDECR, xmax, ymax, rafp);
					/* convert file */
		bn2ra();
	} else {
		if (i != argc-2)
			goto userr;
		if (!strcmp(argv[i], "-"))
			rafp = stdin;
		else if ((rafp = fopen(argv[i], "r")) == NULL) {
			sprintf(errmsg, "cannot open input \"%s\"",
					argv[i]);
			quiterr(errmsg);
		}
					/* get header */
		getheader(rafp, NULL);
		if (fgetresolu(&xmax, &ymax, rafp) != (YMAJOR|YDECR))
			quiterr("bad RADIANCE format");
		if (openbarney(argv[i+1], "w") < 0) {
			sprintf(errmsg, "cannot open output \"%s\"", argv[i+1]);
			quiterr(errmsg);
		}
					/* convert file */
		ra2bn();
	}
	quiterr(NULL);
userr:
	fprintf(stderr, "Usage: %s {input|-} output\n", progname);
	fprintf(stderr, "   or: %s -r [-g gamma] input [output|-]\n",
			progname);
	exit(1);
}


quiterr(err)		/* print message and exit */
char  *err;
{
	if (err != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	exit(0);
}


openbarney(fname, mode)			/* open barneyscan files */
char	*fname;
char	*mode;
{
	static char	suffix[3][4] = {"red", "grn", "blu"};
	int	i;
	char	file[128];

	for (i = 0; i < 3; i++) {
		sprintf(file, "%s.%s", fname, suffix[i]);
		if ((bnfp[i] = fopen(file, mode)) == NULL)
			return(-1);
	}
	if (mode[0] == 'r') {		/* get header */
		xmax = getint(bnfp[0]);
		ymax = getint(bnfp[0]);
		for (i = 1; i < 3; i++)
			if (getint(bnfp[i]) != xmax || getint(bnfp[i]) != ymax)
				return(-1);
	} else {			/* put header */
		for (i = 0; i < 3; i++) {
			putint(xmax, bnfp[i]);
			putint(ymax, bnfp[i]);
		}
	}
	return(0);
}


int
getint(fp)				/* get short int from barneyscan file */
register FILE	*fp;
{
	register short	val;

	val = getc(fp);
	val |= getc(fp) << 8;

	return(val);
}


putint(val, fp)				/* put short int to barneyscan file */
register int	val;
register FILE	*fp;
{
	putc(val&0xff, fp);
	putc((val >> 8)&0xff, fp);
}


ra2bn()					/* convert radiance to barneyscan */
{
	unsigned char	gmap[1024];
	register int	i,k,c;
	register COLOR	*inl;
	int	j;

	if ((inl = (COLOR *)malloc(xmax*sizeof(COLOR))) == NULL)
		quiterr("out of memory");
	for (i = 0; i < 1024; i++)
		gmap[i] = 256.*pow((i+.5)/1024., 1./gamma);
	for (j = 0; j < ymax; j++) {
		if (freadscan(inl, xmax, rafp) < 0)
			quiterr("error reading RADIANCE file");
		for (i = 0; i < xmax; i++)
			for (k = 0; k < 3; k++) {
				c = 1024.*colval(inl[i],k);
				if (c >= 1024)
					c = 1023;
				putc(gmap[c], bnfp[k]);
			}
	}
	free((char *)inl);
}


bn2ra()					/* convert barneyscan to radiance */
{
	float	gmap[256];
	register int	i,k,c;
	register COLOR	*outline;
	int	j;

	if ((outline = (COLOR *)malloc(xmax*sizeof(COLOR))) == NULL)
		quiterr("out of memory");
	for (i = 0; i < 256; i++)
		gmap[i] = pow((i+.5)/256., gamma);
	for (j = 0; j < ymax; j++) {
		for (i = 0; i < xmax; i++)
			for (k = 0; k < 3; k++)
				if ((c = getc(bnfp[k])) == EOF)
					quiterr("error reading barney file");
				else
					colval(outline[i],k) = gmap[c];
		if (fwritescan(outline, xmax, rafp) < 0)
			quiterr("error writing RADIANCE file");
	}
	free((char *)outline);
}
