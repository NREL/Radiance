/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Radiance picture to PostScript file translator -- one way!
 */

#include  <stdio.h>
#ifdef MSDOS
#include  <fcntl.h>
#endif
#include  "color.h"

#define HMARGIN		(.5*72)			/* horizontal margin */
#define VMARGIN		(.5*72)			/* vertical margin */
#define PWIDTH		(8.5*72-2*HMARGIN)	/* width of device */
#define PHEIGHT		(11*72-2*VMARGIN)	/* height of device */

#define RUNCHR		'*'			/* character to start rle */
#define MINRUN		4			/* minimum run-length */
#define RSTRT		'!'			/* character for MINRUN */
#define MAXRUN		(MINRUN+'~'-RSTRT)	/* maximum run-length */

char  code[] =			/* 6-bit code lookup table */
	"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@+";

int  wrongformat = 0;			/* input in wrong format? */
double	pixaspect = 1.0;		/* pixel aspect ratio */

int  bradj = 0;				/* brightness adjustment */
int  ncopies = 1;			/* number of copies */

char  *progname;

int  xmax, ymax;

extern char  *malloc();


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
			case 'n':		/* number of copies */
				ncopies = atoi(argv[++i]);
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
#endif
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
	double	pwidth, pheight;
	double	iwidth, iheight;
					/* EPS comments */
	puts("%%!PS-Adobe-2.0 EPSF-2.0");
	printf("%%%%Title: %s\n", name);
	printf("%%%%Creator: %s = %s\n", progname, SCCSid);
	printf("%%%%Pages: %d\n", ncopies);
	if (landscape = xmax > pixaspect*ymax)
		puts("%%Orientation: Landscape");
	else
		puts("%%Orientation: Portrait");
	if (PWIDTH > PHEIGHT ^ landscape) {
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
	if (pwidth == PHEIGHT)
		printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
				HMARGIN+(pheight-iheight)*.5,
				VMARGIN+(pwidth-iwidth)*.5,
				HMARGIN+(pheight-iheight)*.5+iheight,
				VMARGIN+(pwidth-iwidth)*.5+iwidth);
	else
		printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
				HMARGIN+(pwidth-iwidth)*.5,
				VMARGIN+(pheight-iheight)*.5,
				HMARGIN+(pwidth-iwidth)*.5+iwidth,
				VMARGIN+(pheight-iheight)*.5+iheight);
	puts("%%EndComments");
	puts("save");
	puts("64 dict begin");
					/* define image reader */
	PSprocdef("read6bitRLE");
					/* set up transformation matrix */
	printf("%f %f translate\n", HMARGIN, VMARGIN);
	if (pwidth == PHEIGHT) {
		printf("0 %f translate\n", PHEIGHT);
		puts("-90 rotate");
	}
	printf("%f %f translate\n", (pwidth-iwidth)*.5, (pheight-iheight)*.5);
	printf("%f %f scale\n", iwidth, iheight);
	puts("%%%%EndProlog");
					/* start image procedure */
	printf("%d %d 8 [%d 0 0 %d 0 %d] {read6bitRLE} image\n", xmax, ymax,
			xmax, -ymax, ymax);
}


PStrailer()			/* print PostScript trailer */
{
	puts("%%Trailer");
	if (ncopies > 1)
		printf("/#copies %d def\n", ncopies);
	puts("showpage");
	puts("end");
	puts("restore");
	puts("%%EOF");
}


PSprocdef(nam)			/* define PS procedure to read image */
char  *nam;
{
	short  itab[128];
	register int  i;
				/* assign code values */
	for (i = 0; i < 128; i++)	/* clear */
		itab[i] = -1;
	for (i = 1; i < 63; i++)	/* assign greys */
		itab[code[i]] = i<<2 | 2;
	itab[code[0]] = 0;		/* black is black */
	itab[code[63]] = 255;		/* and white is white */
	printf("/codetab [");
	for (i = 0; i < 128; i++) {
		if (!(i & 0xf))
			putchar('\n');
		printf(" %3d", itab[i]);
	}
	printf("\n] def\n");
	printf("/scanline %d string def\n", xmax);
	printf("/nrept 0 def\n");
	printf("/readbyte { currentfile read not {stop} if } def\n");
	printf("/decode { codetab exch get } def\n");
	printf("/%s {\n", nam);
	printf("\t{ 0 1 %d { scanline exch\n", xmax-1);
	printf("\t\tnrept 0 le\n");
	printf("\t\t\t{ { readbyte dup %d eq\n", RUNCHR);
	printf("\t\t\t\t\t{ pop /nrept readbyte %d sub def\n", RSTRT-MINRUN+1);
	printf("\t\t\t\t\t\t/reptv readbyte decode def\n");
	printf("\t\t\t\t\t\treptv exit }\n");
	printf("\t\t\t\t\t{ decode dup 0 lt {pop} {exit} ifelse }\n");
	printf("\t\t\t\tifelse } loop }\n");
	printf("\t\t\t{ /nrept nrept 1 sub def reptv }\n");
	printf("\t\tifelse put\n");
	printf("\t\t} for\n");
	printf("\t} stopped {pop pop 0 string} {scanline} ifelse\n");
	printf("} bind def\n");
}


ra2ps()				/* convert Radiance scanlines to 6-bit */
{
	register COLR	*scanin;
	register int	c;
	int	lastc, cnt;
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
		normcolrs(scanin, xmax, bradj); /* normalize */
		lastc = -1; cnt = 0;
		for (x = 0; x < xmax; x++) {
			c = normbright(scanin[x]) + 2;
			if (c > 255) c = 255;
			c = code[c>>2];
			if (c == lastc && cnt < MAXRUN)
				cnt++;
			else {
				putrle(cnt, lastc);
				lastc = c;
				cnt = 1;
			}
		}
		putrle(cnt, lastc);
		if (ferror(stdout))
			quiterr("error writing PostScript file");
	}
	putchar('\n');
						/* free scanline */
	free((char *)scanin);
}


putrle(cnt, cod)		/* put out cnt of cod */
register int	cnt, cod;
{
	static int	col = 0;

	if (cnt >= MINRUN) {
		col += 3;
		putchar(RUNCHR);
		putchar(RSTRT-MINRUN+cnt);
		putchar(cod);
	} else {
		col += cnt;
		while (cnt-- > 0)
			putchar(cod);
	}
	if (col >= 72) {
		putchar('\n');
		col = 0;
	}
}
