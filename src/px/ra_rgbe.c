/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  program to convert from RADIANCE RLE to flat format
 */

#include  <stdio.h>

#include  "color.h"
#include  "resolu.h"

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
		fprintf(stderr, "can't open output \"%s\"\n",
				progname, argv[i+1]);
		exit(1);
	}
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
	extern double	pow();
	int	order;
	int  	xmax, ymax;
	COLR	*scanin;
	register int	x;
	int	y;
				/* get header info. */
	if (checkheader(stdin, COLRFMT, stdout) < 0 ||
			(order = fgetresolu(&xmax, &ymax, stdin)) < 0)
		quiterr("bad picture format");
	if (bradj)
		fputexpos(pow(2.0, (double)bradj), stdout);
	if (!doflat) {
		fputformat(COLRFMT, stdout);
		printf("%s -r\n\n", progname);
	} else
		printf("%s\n\n", progname);
	fputresolu(order, xmax, ymax, stdout);
						/* allocate scanline */
	scanin = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanin == NULL)
		quiterr("out of memory in transfer");
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		if (freadcolrs(scanin, xmax, stdin) < 0)
			quiterr("error reading input picture");
		if (bradj)
			shiftcolrs(scanin, xmax, bradj);
		if (doflat)
			fwrite((char *)scanin, sizeof(COLR), xmax, stdout);
		else
			fwritecolrs(scanin, xmax, stdout);
		if (ferror(stdout))
			quiterr("error writing rasterfile");
	}
						/* free scanline */
	free((char *)scanin);
}
