/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  program to convert from RADIANCE RLE to flat format
 */

#include  <stdio.h>
#include  <math.h>
#include  "color.h"
#include  "resolu.h"

#ifdef MSDOS
#include  <fcntl.h>
#endif

extern char  *malloc();

int  bradj = 0;				/* brightness adjustment */

int  doflat = 1;			/* produce flat file */

char  *progname;


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i;

	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'r':
				doflat = !doflat;
				break;
			case 'e':
				if (argv[i+1][0] != '+' && argv[i+1][0] != '-')
					goto userr;
				bradj = atoi(argv[++i]);
				break;
			default:
				goto userr;
			}
		else
			break;

	if (i < argc-2)
		goto userr;
	if (i <= argc-1 && freopen(argv[i], "r", stdin) == NULL) {
		fprintf(stderr, "%s: can't open input \"%s\"\n",
				progname, argv[i]);
		exit(1);
	}
	if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
		fprintf(stderr, "%s: can't open output \"%s\"\n",
				progname, argv[i+1]);
		exit(1);
	}
#ifdef MSDOS
	setmode(fileno(stdin), O_BINARY);
	setmode(fileno(stdout), O_BINARY);
#endif
	transfer();
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-r][-e +/-stops] [input [output]]\n",
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


transfer()		/* transfer Radiance picture */
{
	static char	ourfmt[LPICFMT+1] = PICFMT;
	int	order;
	int	xmax, ymax;
	COLR	*scanin;
	int	y;
				/* get header info. */
	if ((y = checkheader(stdin, ourfmt, stdout)) < 0 ||
			(order = fgetresolu(&xmax, &ymax, stdin)) < 0)
		quiterr("bad picture format");
	if (!y)
		strcpy(ourfmt, COLRFMT);
	fputs(progname, stdout);
	if (bradj)
		fprintf(stdout, " -e %+d", bradj);
	if (!doflat)
		fputs(" -r", stdout);
	fputc('\n', stdout);
	if (bradj)
		fputexpos(pow(2.0, (double)bradj), stdout);
	fputformat(ourfmt, stdout);
	fputc('\n', stdout);
	fputresolu(order, xmax, ymax, stdout);
						/* allocate scanline */
	scanin = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanin == NULL)
		quiterr("out of memory in transfer");
						/* convert image */
	for (y = ymax; y--; ) {
		if (freadcolrs(scanin, xmax, stdin) < 0)
			quiterr("error reading input picture");
		if (bradj)
			shiftcolrs(scanin, xmax, bradj);
		if (doflat)
			fwrite((char *)scanin, sizeof(COLR), xmax, stdout);
		else
			fwritecolrs(scanin, xmax, stdout);
		if (ferror(stdout))
			quiterr("error writing output picture");
	}
						/* free scanline */
	free((char *)scanin);
}
