/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  pcompos.c - program to composite pictures.
 *
 *     6/30/87
 */

#include  <stdio.h>

#include  "color.h"


#define  MAXFILE	32

					/* output picture size */
int  xsiz = 0;
int  ysiz = 0;
					/* input dimensions */
int  xmin = 0;
int  ymin = 0;
int  xmax = 0;
int  ymax = 0;

COLR  bgcolr = BLKCOLR;			/* background color */

int  checkthresh = 0;			/* check threshold value */

char  *progname;

struct {
	char  *name;			/* file name */
	FILE  *fp;			/* stream pointer */
	int  xres, yres;		/* picture size */
	int  xloc, yloc;		/* anchor point */
	int  hasmin, hasmax;		/* has threshold values */
	COLR  thmin, thmax;		/* thresholds */
} input[MAXFILE];		/* our input files */

int  nfile;			/* number of files */

int  wrongformat = 0;


tabputs(s)			/* print line preceded by a tab */
char  *s;
{
	char  fmt[32];

	if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	} else {
		putc('\t', stdout);
		fputs(s, stdout);
	}
}


main(argc, argv)
int  argc;
char  *argv[];
{
	double  atof();
	int  an;

	progname = argv[0];

	for (an = 1; an < argc && argv[an][0] == '-'; an++)
		switch (argv[an][1]) {
		case 'x':
			xmax = xsiz = atoi(argv[++an]);
			break;
		case 'y':
			ymax = ysiz = atoi(argv[++an]);
			break;
		case 'b':
			setcolr(bgcolr, atof(argv[an+1]),
					atof(argv[an+2]),
					atof(argv[an+3]));
			an += 3;
			break;
		case '\0':
		case 't':
			goto dofiles;
		default:
			goto userr;
		}
dofiles:
	for (nfile = 0; an < argc; nfile++) {
		if (nfile >= MAXFILE) {
			fprintf(stderr, "%s: too many files\n", progname);
			quit(1);
		}
		input[nfile].hasmin = input[nfile].hasmax = 0;
		while (an < argc && (argv[an][0] == '-' || argv[an][0] == '+'))
			switch (argv[an][1]) {
			case 't':
				checkthresh = 1;
				if (argv[an][0] == '-') {
					input[nfile].hasmin = 1;
					setcolr(input[nfile].thmin,
							atof(argv[an+1]),
							atof(argv[an+1]),
							atof(argv[an+1]));
				} else {
					input[nfile].hasmax = 1;
					setcolr(input[nfile].thmax,
							atof(argv[an+1]),
							atof(argv[an+1]),
							atof(argv[an+1]));
				}
				an += 2;
				break;
			case '\0':
				if (argv[an][0] == '-')
					goto getfile;
			/* fall through */
			default:
				goto userr;
			}
getfile:
		if (argc-an < 3)
			goto userr;
		if (!strcmp(argv[an], "-")) {
			input[nfile].name = "<stdin>";
			input[nfile].fp = stdin;
		} else {
			input[nfile].name = argv[an];
			if ((input[nfile].fp = fopen(argv[an], "r")) == NULL) {
				perror(argv[an]);
				quit(1);
			}
		}
		an++;
						/* get header */
		printf("%s:\n", input[nfile].name);
		getheader(input[nfile].fp, tabputs, NULL);
		if (wrongformat) {
			fprintf(stderr, "%s: not a Radiance picture\n",
					input[nfile].name);
			quit(1);
		}
						/* get picture size */
		if (fgetresolu(&input[nfile].xres, &input[nfile].yres,
				input[nfile].fp) != (YMAJOR|YDECR)) {
			fprintf(stderr, "%s: bad picture size\n",
					input[nfile].name);
			quit(1);
		}
		input[nfile].xloc = atoi(argv[an++]);
		input[nfile].yloc = atoi(argv[an++]);
		if (input[nfile].xloc < xmin)
			xmin = input[nfile].xloc;
		if (input[nfile].yloc < ymin)
			ymin = input[nfile].yloc;
		if (input[nfile].xloc+input[nfile].xres > xmax)
			xmax = input[nfile].xloc+input[nfile].xres;
		if (input[nfile].yloc+input[nfile].yres > ymax)
			ymax = input[nfile].yloc+input[nfile].yres;
	}
	if (xsiz <= 0)
		xsiz = xmax;
	if (ysiz <= 0)
		ysiz = ymax;
					/* add new header info. */
	printargs(argc, argv, stdout);
	fputformat(COLRFMT, stdout);
	printf("\n-Y %d +X %d\n", ysiz, xsiz);

	compos();
	
	quit(0);
userr:
	fprintf(stderr, "Usage: %s [-x xres][-y yres][-b r g b] ", progname);
	fprintf(stderr, "[-t min1][+t max1] file1 x1 y1 ..\n");
	quit(1);
}


compos()				/* composite pictures */
{
	COLR  *scanin, *scanout;
	int  y;
	register int  x, i;

	scanin = (COLR *)malloc((xmax-xmin)*sizeof(COLR));
	if (scanin == NULL)
		goto memerr;
	scanin -= xmin;
	if (checkthresh) {
		scanout = (COLR *)malloc(xsiz*sizeof(COLR));
		if (scanout == NULL)
			goto memerr;
	} else
		scanout = scanin;
	for (y = ymax-1; y >= 0; y--) {
		for (x = 0; x < xsiz; x++)
			copycolr(scanout[x], bgcolr);
		for (i = 0; i < nfile; i++) {
			if (input[i].yloc > y ||
					input[i].yloc+input[i].yres <= y)
				continue;
			if (freadcolrs(scanin+input[i].xloc,
					input[i].xres, input[i].fp) < 0) {
				fprintf(stderr, "%s: read error (y==%d)\n",
						input[i].name,
						y-input[i].yloc);
				quit(1);
			}
			if (y >= ysiz)
				continue;
			if (checkthresh) {
				x = input[i].xloc+input[i].xres;
				if (x > xsiz)
					x = xsiz;
				for (x--; x >= 0 && x >= input[i].xloc; x--) {
					if (input[i].hasmin &&
					cmpcolr(scanin[x], input[i].thmin) <= 0)
						continue;
					if (input[i].hasmax &&
					cmpcolr(scanin[x], input[i].thmax) >= 0)
						continue;
					copycolr(scanout[x], scanin[x]);
				}
			}
		}
		if (y >= ysiz)
			continue;
		if (fwritecolrs(scanout, xsiz, stdout) < 0) {
			perror(progname);
			quit(1);
		}
	}
	return;
memerr:
	perror(progname);
	quit(1);
}


int
cmpcolr(c1, c2)			/* compare two colr's (improvisation) */
register COLR  c1, c2;
{
	register int  i, j;

	j = 4;				/* check exponents first! */
	while (j--)
		if (i = c1[j] - c2[j])
			return(i);
	return(0);
}


quit(code)		/* exit gracefully */
int  code;
{
	exit(code);
}
