/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  pfilt.c - program to post-process picture file.
 *
 *     9/26/85
 */

#include  <stdio.h>

#include  <signal.h>

#include  "color.h"

#include  "resolu.h"

extern char  *malloc();
extern float  *matchlamp();

#define  FEQ(a,b)	((a) >= .98*(b) && (a) <= 1.02*(b))

#define  CHECKRAD	1.5	/* radius to check for filtering */

COLOR  exposure = WHTCOLOR;	/* exposure for the frame */

double  rad = 0.0;		/* output pixel radius for filtering */

int  nrows = 0;			/* number of rows for output */
int  ncols = 0;			/* number of columns for output */

double  x_c = 1.0;		/* ratio of output x size to input */
double  y_r = 1.0;		/* ratio of output y size to input */

int  singlepass = 0;		/* true means skip first pass */

int  avghot = 0;		/* true means average in bright spots */

double  hotlvl = 1000.0;	/* level considered "hot" */

int  npts = 0;			/* (half) number of points for stars */

double  spread = 1e-4;		/* spread for star points */

#define  TEMPLATE	"/usr/tmp/pfXXXXXX"

char  *tfname = NULL;

char  *lampdat = "lamp.tab";	/* lamp data file */

int  order;			/* scanline ordering of input */
int  xres, yres;		/* resolution of input */
double  inpaspect = 1.0;	/* pixel aspect ratio of input */
int  correctaspect = 0;		/* aspect ratio correction? */

int  wrongformat = 0;

int  xrad;			/* x window size */
int  yrad;			/* y window size */

int  barsize;			/* size of input scan bar */
COLOR  **scanin;		/* input scan bar */
COLOR  *scanout;		/* output scan line */

char  *progname;


main(argc, argv)
int  argc;
char  **argv;
{
	extern char  *mktemp();
	extern double  atof(), pow();
	extern long  ftell();
	extern int  quit(), headline();
	FILE  *fin;
	float  *lampcolor;
	char  *lamptype = NULL;
	long  fpos;
	double  outaspect = 0.0;
	double  d;
	int  i, j;

	if (signal(SIGINT, quit) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, quit) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	signal(SIGTERM, quit);
	signal(SIGPIPE, quit);
#ifdef  SIGXCPU
	signal(SIGXCPU, quit);
	signal(SIGXFSZ, quit);
#endif

	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'x':
				i++;
				if (argv[i][0] == '/') {
					x_c = 1.0/atof(argv[i]+1);
					ncols = 0;
				} else
					ncols = atoi(argv[i]);
				break;
			case 'y':
				i++;
				if (argv[i][0] == '/') {
					y_r = 1.0/atof(argv[i]+1);
					nrows = 0;
				} else
					nrows = atoi(argv[i]);
				break;
			case 'c':
				correctaspect = !correctaspect;
				break;
			case 'p':
				i++;
				outaspect = atof(argv[i]);
				break;
			case 'e':
				if (argv[i+1][0] == '+' || argv[i+1][0] == '-')
					d = pow(2.0, atof(argv[i+1]));
				else
					d = atof(argv[i+1]);
				if (d < 1e-20 || d > 1e20) {
					fprintf(stderr,
						"%s: exposure out of range\n",
							argv[0]);
					exit(1);
				}
				switch (argv[i][2]) {
				case '\0':
					scalecolor(exposure, d);
					break;
				case 'r':
					colval(exposure,RED) *= d;
					break;
				case 'g':
					colval(exposure,GRN) *= d;
					break;
				case 'b':
					colval(exposure,BLU) *= d;
					break;
				default:
					goto badopt;
				}
				i++;
				break;
			case 'f':
				lampdat = argv[++i];
				break;
			case 't':
				lamptype = argv[++i];
				break;
			case '1':
				singlepass = 1;
				break;
			case '2':
				singlepass = 0;
				break;
			case 'n':
				npts = atoi(argv[++i]) / 2;
				break;
			case 's':
				spread = atof(argv[++i]);
				break;
			case 'a':
				avghot = !avghot;
				break;
			case 'h':
				hotlvl = atof(argv[++i]);
				break;
			case 'r':
				rad = atof(argv[++i]);
				break;
			case 'b':
				rad = 0.0;
				break;
			default:;
			badopt:
				fprintf(stderr, "%s: unknown option: %s\n",
						progname, argv[i]);
				quit(1);
				break;
			}
		else
			break;
					/* get lamp data (if necessary) */
	if (lamptype != NULL) {
		if (loadlamps(lampdat) < 0)
			quit(1);
		if ((lampcolor = matchlamp(lamptype)) == NULL) {
			fprintf(stderr, "%s: unknown lamp type\n", lamptype);
			quit(1);
		}
		for (i = 0; i < 3; i++)
			if (lampcolor[i] > 1e-4)
				colval(exposure,i) /= lampcolor[i];
		freelamps();
	}
					/* open input file */
	if (i == argc) {
		if (singlepass)
			fin = stdin;
		else {
			tfname = mktemp(TEMPLATE);
			if ((fin = fopen(tfname, "w+")) == NULL) {
				fprintf(stderr, "%s: can't create ", progname);
				fprintf(stderr, "temp file \"%s\"\n", tfname);
				quit(1);
			}
			copyfile(stdin, fin);
			if (fseek(fin, 0L, 0) == -1) {
				fprintf(stderr, "%s: seek fail\n", progname);
				quit(1);
			}
		}
	} else if (i == argc-1) {
		if ((fin = fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i]);
			quit(1);
		}
	} else {
		fprintf(stderr, "%s: bad # file arguments\n", progname);
		quit(1);
	}
					/* get header */
	getheader(fin, headline, NULL);
	if (wrongformat) {
		fprintf(stderr, "%s: input must be a Radiance picture\n",
				progname);
		quit(1);
	}
					/* add new header info. */
	printargs(i, argv, stdout);
					/* get picture size */
	if ((order = fgetresolu(&xres, &yres, fin)) < 0) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		quit(1);
	}
	if (!(order & YMAJOR))
		inpaspect = 1.0/inpaspect;
					/* compute output resolution */
	if (ncols <= 0)
		ncols = x_c*xres + .5;
	if (nrows <= 0)
		nrows = y_r*yres + .5;
	if (outaspect > .01) {
		d = inpaspect * yres/xres / outaspect;
		if (d * ncols > nrows)
			ncols = nrows / d;
		else
			nrows = ncols * d;
	}
	x_c = (double)ncols/xres;
	y_r = (double)nrows/yres;

	if (singlepass) {		/* skip exposure, etc. */
		pass1default();
		pass2(fin);
		quit(0);
	}

	fpos = ftell(fin);		/* save input file position */
	
	pass1(fin);

	if (fseek(fin, fpos, 0) == -1) {
		fprintf(stderr, "%s: seek fail\n", progname);
		quit(1);
	}
	pass2(fin);

	quit(0);
}


headline(s)				/* process line from header */
char  *s;
{
	char  fmt[32];

	fputs(s, stdout);		/* copy to output */
	if (isaspect(s))		/* get aspect ratio */
		inpaspect *= aspectval(s);
	else if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	}
}


copyfile(in, out)			/* copy a file */
register FILE  *in, *out;
{
	register int  c;

	while ((c = getc(in)) != EOF)
		putc(c, out);

	if (ferror(out)) {
		fprintf(stderr, "%s: write error in copyfile\n", progname);
		quit(1);
	}
}


pass1(in)				/* first pass of picture file */
FILE  *in;
{
	int  i;
	COLOR  *scan;

	pass1init();

	scan = (COLOR *)malloc(xres*sizeof(COLOR));
	if (scan == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	for (i = 0; i < yres; i++) {
		if (freadscan(scan, xres, in) < 0) {
			nrows = nrows * i / yres;	/* adjust frame */
			if (nrows <= 0) {
				fprintf(stderr, "%s: empty frame\n", progname);
				quit(1);
			}
			fprintf(stderr, "%s: warning - partial frame (%d%%)\n",
					progname, 100*i/yres);
			yres = i;
			y_r = (double)nrows/yres;
			break;
		}
		pass1scan(scan, i);
	}
	free((char *)scan);
}


pass2(in)			/* last pass on file, write to stdout */
FILE  *in;
{
	int  yread;
	int  ycent, xcent;
	int  r, c;
 	
	pass2init();
	scan2init();
	yread = 0;
	for (r = 0; r < nrows; r++) {
		ycent = (long)r*yres/nrows;
		while (yread <= ycent+yrad) {
			if (yread < yres) {
				if (freadscan(scanin[yread%barsize],
						xres, in) < 0) {
					fprintf(stderr,
						"%s: bad read (y=%d)\n",
						progname, yres-1-yread);
					quit(1);
				}
				pass2scan(scanin[yread%barsize], yread);
			}
			yread++;
		}
		for (c = 0; c < ncols; c++) {
			xcent = (long)c*xres/ncols;
			if (rad <= 0.0)
				dobox(scanout[c], xcent, ycent, c, r);
			else
				dogauss(scanout[c], xcent, ycent, c, r);
		}
		if (fwritescan(scanout, ncols, stdout) < 0) {
			fprintf(stderr, "%s: write error in pass2\n", progname);
			quit(1);
		}
	}
					/* skip leftovers */
	while (yread < yres) {
		if (freadscan(scanin[0], xres, in) < 0)
			break;
		yread++;
	}
}


scan2init()			/* prepare scanline arrays */
{
	COLOR	ctmp;
	double  d;
	register int  i;

	if (rad <= 0.0) {
		xrad = xres/ncols/2 + 1;
		yrad = yres/nrows/2 + 1;
	} else {
		if (nrows >= yres && ncols >= xres)
			rad *= (y_r + x_c)/2.0;

		xrad = CHECKRAD*rad/x_c + 1;
		yrad = CHECKRAD*rad/y_r + 1;

		initmask();		/* initialize filter table */
	}
	barsize = 2*yrad + 1;
	scanin = (COLOR **)malloc(barsize*sizeof(COLOR *));
	for (i = 0; i < barsize; i++) {
		scanin[i] = (COLOR *)malloc(xres*sizeof(COLOR));
		if (scanin[i] == NULL) {
			fprintf(stderr, "%s: out of memory\n", progname);
			quit(1);
		}
	}
	scanout = (COLOR *)malloc(ncols*sizeof(COLOR));
	if (scanout == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
					/* record pixel aspect ratio */
	if (!correctaspect) {
		d = order & YMAJOR ? x_c/y_r : y_r/x_c ;
		if (!FEQ(d,1.0))
			fputaspect(d, stdout);
	}
					/* record exposure */
	d = bright(exposure);
	if (!FEQ(d,1.0))
		fputexpos(d, stdout);
					/* record color correction */
	copycolor(ctmp, exposure);
	scalecolor(ctmp, 1.0/d);
	if (!FEQ(colval(ctmp,RED),colval(ctmp,GRN)) ||
			!FEQ(colval(ctmp,GRN),colval(ctmp,BLU)))
		fputcolcor(ctmp, stdout);
	printf("\n");
					/* write out resolution */
	fputresolu(order, ncols, nrows, stdout);
}


quit(code)		/* remove temporary file and exit */
int  code;
{
	if (tfname != NULL)
		unlink(tfname);
	exit(code);
}
