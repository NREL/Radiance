/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Radiance picture to PostScript file translator -- one way!
 */

#include  <stdio.h>
#include  "color.h"
#include  "random.h"

#define HMARGIN		(.5*72)			/* horizontal margin */
#define VMARGIN		(.5*72)			/* vertical margin */
#define PWIDTH		(8.5*72-2*HMARGIN)	/* width of device */
#define PHEIGHT		(11*72-2*VMARGIN)	/* height of device */

char  code[] =			/* 6-bit code lookup table */
	"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@+";

int  wrongformat = 0;			/* input in wrong format? */
double  pixaspect = 1.0;		/* pixel aspect ratio */

int  bradj = 0;				/* brightness adjustment */

char  *progname;

int  xmax, ymax;


headline(s)		/* check header line */
char  *s;
{
	char  fmt[32];

	if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	} else if (isaspect(s))
		pixaspect *= aspectval(s);
}


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'e':		/* exposure adjustment */
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
				/* get our header */
	getheader(stdin, headline, NULL);
	if (wrongformat || fgetresolu(&xmax, &ymax, stdin) < 0)
		quiterr("bad picture format");
				/* write header */
	PSheader(i <= argc-1 ? argv[i] : "<stdin>");
				/* convert file */
	ra2ps();
				/* write trailer */
	PStrailer();
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-e +/-stops] [input [output]]\n", progname);
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


PSheader(name)			/* print PostScript header */
char  *name;
{
	int  landscape = 0;
	double  pwidth, pheight;
	double  iwidth, iheight;

	printf("%%!\n");
	printf("%%%%Title: %s\n", name);
	printf("%%%%Creator: %s\n", progname);
	printf("%%%%Pages: 1\n");
	if (landscape = xmax > pixaspect*ymax)
		printf("%%%%Landscape\n");
	else
		printf("%%%%Portrait\n");
	printf("%%%%EndComments\n");
	printf("gsave\n");
	printf("64 dict begin\n");
					/* set up transformation matrix */
	printf("%f %f translate\n", HMARGIN, VMARGIN);
	if (PWIDTH > PHEIGHT ^ landscape) {
		printf("0 %f translate\n", PHEIGHT);
		printf("-90 rotate\n");
		pwidth = PHEIGHT;
		pheight = PWIDTH;
	} else {
		pwidth = PWIDTH;
		pheight = PHEIGHT;
	}
	if (pheight/pwidth > pixaspect*ymax/xmax) {
		iwidth = pwidth;
		iheight = pwidth*pixaspect*ymax/xmax;
	} else {
		iheight = pheight;
		iwidth = pheight*xmax/(pixaspect*ymax);
	}
	printf("%f %f translate\n", (pwidth-iwidth)*.5, (pheight-iheight)*.5);
	printf("%f %f scale\n", iwidth, iheight);
	PSprocdef("read6bit");
	printf("%%%%EndProlog\n");
	printf("%d %d 8 [%d 0 0 %d 0 %d] {read6bit} image", xmax, ymax,
			xmax, -ymax, ymax);
}


PStrailer()			/* print PostScript trailer */
{
	puts("%%Trailer");
	puts("end");
	puts("showpage");
	puts("grestore");
	puts("%%EOF");
}


PSprocdef(nam)			/* define PS procedure to read image */
char  *nam;
{
	short  itab[128];
	register int  i;
	
	for (i = 0; i < 128; i++)
		itab[i] = -1;
	for (i = 0; i < 64; i++)
		itab[code[i]] = i<<2 | 2;
	printf("/decode [");
	for (i = 0; i < 128; i++) {
		if (!(i & 0xf))
			putchar('\n');
		printf(" %3d", itab[i]);
	}
	printf("\n] def\n");
	printf("/scanline %d string def\n", xmax);
	printf("/%s {\n", nam);
	printf("\t{ 0 1 %d { scanline exch\n", xmax-1);
	printf("\t\t{ decode currentfile read not {stop} if get\n");
	printf("\tdup 0 lt {pop} {exit} ifelse } loop put } for\n");
	printf("} stopped {pop pop pop 0 string} {scanline} ifelse } def\n");
}


ra2ps()				/* convert Radiance scanlines to 6-bit */
{
	COLR	*scanin;
	register int	col = 0;
	register int	c;
	register int	x;
	int	y;
						/* allocate scanline */
	scanin = (COLR *)malloc(xmax*sizeof(COLR));
	if (scanin == NULL)
		quiterr("out of memory in ra2ps");
						/* convert image */
	for (y = ymax-1; y >= 0; y--) {
		if (freadcolrs(scanin, xmax, stdin) < 0)
			quiterr("error reading Radiance picture");
		normcolrs(scanin, xmax, bradj);	/* normalize */
		for (x = 0; x < xmax; x++) {
			if (!(col++ & 0x3f))
				putchar('\n');
			c = normbright(scanin[x]) + (random()&3);
			if (c > 255) c = 255;
			putchar(code[c>>2]);
		}
		if (ferror(stdout))
			quiterr("error writing PostScript file");
	}
	putchar('\n');
						/* free scanline */
	free((char *)scanin);
}
