/* Copyright (c) 1986 Regents of the University of California */

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


extern char  *malloc();

#define  CHECKRAD	1.5	/* radius to check for filtering */

COLOR  exposure = WHTCOLOR;	/* exposure for the frame */

double  rad = 0.0;		/* output pixel radius for filtering */

int  nrows = 0;			/* number of rows for output */
int  ncols = 0;			/* number of columns for output */

int  singlepass = 0;		/* true means skip first pass */

int  avghot = 0;		/* true means average in bright spots */

double  hotlvl = 1000.0;	/* level considered "hot" */

int  npts = 0;			/* (half) number of points for stars */

double  spread = 1e-4;		/* spread for star points */

#define  TEMPLATE	"/usr/tmp/pfXXXXXX"

char  *tfname = NULL;

int  xres, yres;		/* resolution of input */

double  x_c, y_r;		/* conversion factors */

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
	extern int  quit();
	FILE  *fin;
	long  fpos;
	double  d;
	int  i;

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
				ncols = atoi(argv[++i]);
				break;
			case 'y':
				nrows = atoi(argv[++i]);
				break;
			case 'e':
				if (argv[i+1][0] == '+' || argv[i+1][0] == '-')
					d = pow(2.0, atof(argv[i+1]));
				else
					d = atof(argv[i+1]);
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
			case '1':
				singlepass = 1;
				break;
			case 'p':
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
					/* copy header */
	copyheader(fin, stdout);
					/* add new header info. */
	printargs(i, argv, stdout);
					/* get picture size */
	if (fscanf(fin, "-Y %d +X %d\n", &yres, &xres) != 2) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		quit(1);
	}
	if (ncols <= 0)
		ncols = xres;
	if (nrows <= 0)
		nrows = yres;

	if (singlepass) {
					/* skip exposure, etc. */
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
}


scan2init()			/* prepare scanline arrays */
{
	double  e;
	register int  i;

	x_c = (double)ncols/xres;
	y_r = (double)nrows/yres;

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
	barsize = 2 * yrad;
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
	e = bright(exposure);
	if (e < 1-1e-7 || e > 1+1e-7)		/* record exposure */
		printf("EXPOSURE=%e\n", e);
	printf("\n");
	printf("-Y %d +X %d\n", nrows, ncols);	/* write picture size */
}


quit(code)		/* remove temporary file and exit */
int  code;
{
	if (tfname != NULL)
		unlink(tfname);
	exit(code);
}
