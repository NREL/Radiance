/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  program to convert between RADIANCE and 24-bit TIFF files.
 */

#include  <stdio.h>

#include  "tiffio.h"

#include  "color.h"

extern double  atof();

extern char  *malloc(), *realloc();

double	gamma = 2.2;			/* gamma correction */

int  bradj = 0;				/* brightness adjustment */

char  *progname;


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
			/* not allowed to reset gamma...
			case 'g':
				gamma = atof(argv[++i]);
				break;
			*/
			case 'e':
				if (argv[i+1][0] != '+' && argv[i+1][0] != '-')
					goto userr;
				bradj = atoi(argv[++i]);
				break;
			case 'r':
				reverse = !reverse;
				break;
			case '\0':
				goto doneopts;
			default:
				goto userr;
			}
		else
			break;
doneopts:
	setcolrgam(gamma);

	if (reverse)
		if (i != argc-2 && i != argc-1)
			goto userr;
		else
			tiff2ra(argv[i], argv[i+1]);
	else
		if (i != argc-2)
			goto userr;
		else
			ra2tiff(argv[i], argv[i+1]);

	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-r][-e +/-stops] input output\n",
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


tiff2ra(inpf, outf)	/* convert TIFF file to Radiance picture */
char	*inpf, *outf;
{
	unsigned long	xmax, ymax;
	TIFF	*tif;
	unsigned short	pconfig;
	unsigned short	hi;
	register BYTE	*scanin;
	register COLR	*scanout;
	register int	x;
	int	y;
					/* open/check  TIFF file */
	if ((tif = TIFFOpen(inpf, "r")) == NULL)
		quiterr("cannot open TIFF input");
	if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &hi) || hi != 3)
		quiterr("unsupported samples per pixel");
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &hi) || hi != 8)
		quiterr("unsupported bits per sample");
	if (TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &hi) && hi != 2)
		quiterr("unsupported photometric interpretation");
	if (!TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &pconfig) ||
			(pconfig != 1 && pconfig != 2))
		quiterr("unsupported planar configuration");
	if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &xmax) ||
			!TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &ymax))
		quiterr("unknown input image resolution");
						/* allocate scanlines */
	scanin = (BYTE *)malloc(TIFFScanlineSize(tif));
	scanout = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanin == NULL || scanout == NULL)
		quiterr("out of memory in tiff2ra");
					/* open output and write header */
	if (outf != NULL && strcmp(outf, "-") &&
			freopen(outf, "w", stdout) == NULL)
		quiterr("cannot open Radiance output file");
	fputs(progname, stdout);
	if (bradj)
		printf(" -e %+d", bradj);
	fputs(" -r\n", stdout);
	fputformat(COLRFMT, stdout);
	putchar('\n');
	fputresolu(YDECR|YMAJOR, xmax, ymax, stdout);
						/* convert image */
	for (y = 0; y < ymax; y++) {
		if (pconfig == 1) {
			if (TIFFReadScanline(tif, scanin, y, 0) < 0)
				goto readerr;
			for (x = 0; x < xmax; x++) {
				scanout[x][RED] = scanin[3*x];
				scanout[x][GRN] = scanin[3*x+1];
				scanout[x][BLU] = scanin[3*x+2];
			}
		} else {
			if (TIFFReadScanline(tif, scanin, y, 0) < 0)
				goto readerr;
			for (x = 0; x < xmax; x++)
				scanout[x][RED] = scanin[x];
			if (TIFFReadScanline(tif, scanin, y, 1) < 0)
				goto readerr;
			for (x = 0; x < xmax; x++)
				scanout[x][GRN] = scanin[x];
			if (TIFFReadScanline(tif, scanin, y, 2) < 0)
				goto readerr;
			for (x = 0; x < xmax; x++)
				scanout[x][BLU] = scanin[x];
		}
		gambs_colrs(scanout, xmax);
		if (bradj)
			shiftcolrs(scanout, xmax, bradj);
		if (fwritecolrs(scanout, xmax, stdout) < 0)
			quiterr("error writing Radiance picture");
	}
						/* clean up */
	free((char *)scanin);
	free((char *)scanout);
	TIFFClose(tif);
	return;
readerr:
	quiterr("error reading TIFF input");
}


ra2tiff(inpf, outf)		/* convert Radiance file to 24-bit TIFF */
char	*inpf, *outf;
{
	TIFF	*tif;
	int	xmax, ymax;
	BYTE	*scanout;
	COLR	*scanin;
	register int	x;
	int	y;
						/* open Radiance file */
	if (strcmp(inpf, "-") && freopen(inpf, "r", stdin) == NULL)
		quiterr("cannot open Radiance input file");
	if (checkheader(stdin, COLRFMT, NULL) < 0 ||
			fgetresolu(&xmax, &ymax, stdin) != (YDECR|YMAJOR))
		quiterr("bad Radiance picture");
						/* open TIFF file */
	if ((tif = TIFFOpen(outf, "w")) == NULL)
		quiterr("cannot open TIFF output");
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (unsigned long)xmax);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (unsigned long)ymax);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, 2);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, 1);
						/* allocate scanlines */
	scanin = (COLR *)malloc(xmax*sizeof(COLR));
	scanout = (BYTE *)malloc(TIFFScanlineSize(tif));
	if (scanin == NULL || scanout == NULL)
		quiterr("out of memory in ra2tiff");
						/* convert image */
	for (y = 0; y < ymax; y++) {
		if (freadcolrs(scanin, xmax, stdin) < 0)
			quiterr("error reading Radiance picture");
		if (bradj)
			shiftcolrs(scanin, xmax, bradj);
		colrs_gambs(scanin, xmax);
		for (x = 0; x < xmax; x++) {
			scanout[3*x] = scanin[x][RED];
			scanout[3*x+1] = scanin[x][GRN];
			scanout[3*x+2] = scanin[x][BLU];
		}
		if (TIFFWriteScanline(tif, scanout, y, 0) < 0)
			quiterr("error writing TIFF output");
	}
						/* clean up */
	free((char *)scanin);
	free((char *)scanout);
	TIFFClose(tif);
}
