/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  ra_t8.c - program to convert between RADIANCE and
 *		Targa 8-bit color-mapped images.
 *
 *	8/22/88		Adapted from ra_pr.c
 */

#include  <stdio.h>

#include  "color.h"

#include  "pic.h"

#include  "targa.h"

#ifndef  BSD
#define  bcopy(s,d,n)		(void)memcpy(d,s,n)
extern char  *memcpy();
#endif
			/* descriptor for a picture file or frame buffer */
typedef struct {
	char	*name;			/* file name */
	FILE	*fp;			/* file pointer */
	int	nexty;			/* file positioning */
	int	bytes_line;		/* 0 == variable length lines */
	union {
		long	b;			/* initial scanline */
		long	*y;			/* individual scanline */
	} pos;				/* position(s) */
} pic;

#define  goodpic(h)	(my_imType(h) && my_mapType(h))
#define  my_imType(h)	(((h)->dataType==IM_CMAP || (h)->dataType==IM_CCMAP) \
				&& (h)->dataBits==8 && (h)->imType==0)
#define  my_mapType(h)	((h)->mapType==CM_HASMAP && \
				((h)->CMapBits==24 || (h)->CMapBits==32))

#define  taralloc(h)	(pixel *)emalloc((h)->x*(h)->y*sizeof(pixel))

extern pic	*openinput();

extern char	*ecalloc(), *emalloc();

extern long  ftell();

extern double  atof(), pow();

double	gamma = 2.0;			/* gamma correction */

pic	*inpic;

char  *progname;

char  errmsg[128];

COLR	*inl;

pixel	*tarData;

int  xmax, ymax;


main(argc, argv)
int  argc;
char  *argv[];
{
	colormap  rasmap;
	struct hdStruct  head;
	int  dither = 1;
	int  reverse = 0;
	int  ncolors = 256;
	int  greyscale = 0;
	int  i;
	
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'd':
				dither = !dither;
				break;
			case 'g':
				gamma = atof(argv[++i]);
				break;
			case 'r':
				reverse = !reverse;
				break;
			case 'b':
				greyscale = 1;
				break;
			case 'c':
				ncolors = atoi(argv[++i]);
				break;
			default:
				goto userr;
			}
		else
			break;

	if (reverse) {
		if (i < argc-2)
			goto userr;
		if (i <= argc-1 && freopen(argv[i], "r", stdin) == NULL) {
			sprintf(errmsg, "can't open input \"%s\"", argv[i]);
			quiterr(errmsg);
		}
					/* get header */
		if (getthead(&head, NULL, stdin) < 0)
			quiterr("bad targa file");
		if (!goodpic(&head))
			quiterr("incompatible format");
		xmax = head.x;
		ymax = head.y;
					/* open output file */
		if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
			sprintf(errmsg, "can't open output \"%s\"", argv[i+1]);
			quiterr(errmsg);
		}
					/* put header */
		printargs(i, argv, stdout);
		fputformat(COLRFMT, stdout);
		putchar('\n');
		fputresolu(YMAJOR|YDECR, xmax, ymax, stdout);
					/* convert file */
		tg2ra(&head);
	} else {
		if (i > argc-1 || i < argc-2)
			goto userr;
		if ((inpic = openinput(argv[i], &head)) == NULL) {
			sprintf(errmsg, "can't open input \"%s\"", argv[i]);
			quiterr(errmsg);
		}
		if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
			sprintf(errmsg, "can't open output \"%s\"", argv[i+1]);
			quiterr(errmsg);
		}
					/* write header */
		putthead(&head, NULL, stdout);
					/* convert file */
		if (greyscale)
			biq(dither,ncolors,1,rasmap);
		else
			ciq(dither,ncolors,1,rasmap);
					/* write data */
		writetarga(&head, tarData, stdout);
	}
	quiterr(NULL);
userr:
	fprintf(stderr,
	"Usage: %s [-d][-c ncolors][-b][-g gamma] input [output]\n",
			progname);
	fprintf(stderr, "   Or: %s -r [-g gamma] [input [output]]\n",
			progname);
	exit(1);
}


int
getint2(fp)			/* get a 2-byte positive integer */
register FILE	*fp;
{
	register int  b1, b2;

	if ((b1 = getc(fp)) == EOF || (b2 = getc(fp)) == EOF)
		quiterr("read error");

	return(b1 | b2<<8);
}


putint2(i, fp)			/* put a 2-byte positive integer */
register int  i;
register FILE	*fp;
{
	putc(i&0xff, fp);
	putc(i>>8&0xff, fp);
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


eputs(s)
char *s;
{
	fputs(s, stderr);
}


quit(code)
int code;
{
	exit(code);
}


getthead(hp, ip, fp)		/* read header from input */
struct hdStruct  *hp;
char  *ip;
register FILE  *fp;
{
	int	nidbytes;

	if ((nidbytes = getc(fp)) == EOF)
		return(-1);
	hp->mapType = getc(fp);
	hp->dataType = getc(fp);
	hp->mapOrig = getint2(fp);
	hp->mapLength = getint2(fp);
	hp->CMapBits = getc(fp);
	hp->XOffset = getint2(fp);
	hp->YOffset = getint2(fp);
	hp->x = getint2(fp);
	hp->y = getint2(fp);
	hp->dataBits = getc(fp);
	hp->imType = getc(fp);

	if (ip != NULL)
		if (nidbytes)
			fread((char *)ip, nidbytes, 1, fp);
		else
			*ip = '\0';
	else if (nidbytes)
		fseek(fp, (long)nidbytes, 1);

	return(feof(fp) || ferror(fp) ? -1 : 0);
}


putthead(hp, ip, fp)		/* write header to output */
struct hdStruct  *hp;
char  *ip;
register FILE  *fp;
{
	if (ip != NULL)
		putc(strlen(ip), fp);
	else
		putc(0, fp);
	putc(hp->mapType, fp);
	putc(hp->dataType, fp);
	putint2(hp->mapOrig, fp);
	putint2(hp->mapLength, fp);
	putc(hp->CMapBits, fp);
	putint2(hp->XOffset, fp);
	putint2(hp->YOffset, fp);
	putint2(hp->x, fp);
	putint2(hp->y, fp);
	putc(hp->dataBits, fp);
	putc(hp->imType, fp);

	if (ip != NULL)
		fputs(ip, fp);

	return(ferror(fp) ? -1 : 0);
}


pic *
openinput(fname, h)		/* open RADIANCE input file */
char  *fname;
register struct hdStruct  *h;
{
	register pic  *p;

	p = (pic *)emalloc(sizeof(pic));
	p->name = fname;
	if (fname == NULL)
		p->fp = stdin;
	else if ((p->fp = fopen(fname, "r")) == NULL)
		return(NULL);
					/* get header info. */
	if (checkheader(p->fp, COLRFMT, NULL) < 0 ||
			fgetresolu(&xmax, &ymax, p->fp) != (YMAJOR|YDECR))
		quiterr("bad picture format");
	p->nexty = 0;
	p->bytes_line = 0;		/* variable length lines */
	p->pos.y = (long *)ecalloc(ymax, sizeof(long));
	p->pos.y[0] = ftell(p->fp);
					/* assign header */
	h->textSize = 0;
	h->mapType = CM_HASMAP;
	h->dataType = IM_CMAP;
	h->mapOrig = 0;
	h->mapLength = 256;
	h->CMapBits = 24;
	h->XOffset = 0;
	h->YOffset = 0;
	h->x = xmax;
	h->y = ymax;
	h->dataBits = 8;
	h->imType = 0;
					/* allocate scanline */
	inl = (COLR *)emalloc(xmax*sizeof(COLR));
					/* allocate targa data */
	tarData = taralloc(h);

	return(p);
}


tg2ra(hp)			/* targa file to RADIANCE file */
struct hdStruct  *hp;
{
	union {
		BYTE  c3[256][3];
		BYTE  c4[256][4];
	} map;
	COLR  ctab[256];
	COLR  *scanline;
	register int  i, j;

					/* get color table */
	if ((hp->CMapBits==24 ? fread((char *)map.c3,sizeof(map.c3),1,stdin) :
			fread((char *)map.c4,sizeof(map.c4),1,stdin)) != 1)
		quiterr("error reading color table");
					/* convert table */
	for (i = hp->mapOrig; i < hp->mapOrig+hp->mapLength; i++)
		if (hp->CMapBits == 24)
			setcolr(ctab[i],
					pow((map.c3[i][2]+.5)/256.,gamma),
					pow((map.c3[i][1]+.5)/256.,gamma),
					pow((map.c3[i][0]+.5)/256.,gamma));
		else
			setcolr(ctab[i],
					pow((map.c4[i][3]+.5)/256.,gamma),
					pow((map.c4[i][2]+.5)/256.,gamma),
					pow((map.c4[i][1]+.5)/256.,gamma));

					/* allocate targa data */
	tarData = taralloc(hp);
					/* get data */
	readtarga(hp, tarData, stdin);
					/* allocate input scanline */
	scanline = (COLR *)emalloc(xmax*sizeof(COLR));
					/* convert file */
	for (i = ymax-1; i >= 0; i--) {
		for (j = 0; j < xmax; j++)
			copycolr(scanline[j], ctab[tarData[i*xmax+j]]);
		if (fwritecolrs(scanline, xmax, stdout) < 0)
			quiterr("error writing RADIANCE file");
	}
	free((char *)scanline);
	free((char *)tarData);
}


picreadline3(y, l3)			/* read in 3-byte scanline */
int  y;
register rgbpixel  *l3;
{
	register int	i;

	if (inpic->nexty != y) {			/* find scanline */
		if (inpic->bytes_line == 0) {
			if (inpic->pos.y[y] == 0) {
				while (inpic->nexty < y) {
					if (freadcolrs(inl, xmax, inpic->fp) < 0)
						quiterr("read error in picreadline3");
					inpic->pos.y[++inpic->nexty] = ftell(inpic->fp);
				}
			} else if (fseek(inpic->fp, inpic->pos.y[y], 0) == EOF)
				quiterr("seek error in picreadline3");
		} else if (fseek(inpic->fp, y*inpic->bytes_line+inpic->pos.b, 0) == EOF)
			quiterr("seek error in picreadline3");
	} else if (inpic->bytes_line == 0 && inpic->pos.y[inpic->nexty] == 0)
		inpic->pos.y[inpic->nexty] = ftell(inpic->fp);
	if (freadcolrs(inl, xmax, inpic->fp) < 0)	/* read scanline */
		quiterr("read error in picreadline3");
	inpic->nexty = y+1;
							/* convert scanline */
	normcolrs(inl, xmax, 0);
	for (i = 0; i < xmax; i++) {
		l3[i].r = inl[i][RED];
		l3[i].g = inl[i][GRN];
		l3[i].b = inl[i][BLU];
	}
}


picwriteline(y, l)			/* save output scanline */
int  y;
pixel  *l;
{
	bcopy((char *)l, (char *)&tarData[(ymax-1-y)*xmax], xmax*sizeof(pixel));
}


writetarga(h, d, fp)		/* write out targa data */
struct hdStruct  *h;
pixel  *d;
FILE  *fp;
{
	if (h->dataType == IM_CMAP) {		/* uncompressed */
		if (fwrite((char *)d,h->x*sizeof(pixel),h->y,fp) != h->y)
			quiterr("error writing targa file");
		return;
	}
	quiterr("unsupported output type");
}


readtarga(h, data, fp)		/* read in targa data */
struct hdStruct  *h;
pixel  *data;
FILE  *fp;
{
	register int  cnt, c;
	register pixel	*dp;

	if (h->dataType == IM_CMAP) {		/* uncompressed */
		if (fread((char *)data,h->x*sizeof(pixel),h->y,fp) != h->y)
			goto readerr;
		return;
	}
	for (dp = data; dp < data+h->x*h->y; ) {
		if ((c = getc(fp)) == EOF)
			goto readerr;
		cnt = (c & 0x7f) + 1;
		if (c & 0x80) {			/* repeated pixel */
			if ((c = getc(fp)) == EOF)
				goto readerr;
			while (cnt--)
				*dp++ = c;
		} else				/* non-repeating pixels */
			while (cnt--) {
				if ((c = getc(fp)) == EOF)
					goto readerr;
				*dp++ = c;
			}
	}
	return;
readerr:
	quiterr("error reading targa file");
}


picwritecm(cm)			/* write out color map */
colormap  cm;
{
	register int  i, j;

	for (j = 0; j < 256; j++)
		for (i = 2; i >= 0; i--)
			putc(cm[i][j], stdout);
}


picreadcm(map)			/* do gamma correction if requested */
colormap  map;
{
	register int  i, val;

	for (i = 0; i < 256; i++) {
		val = pow((i+0.5)/256.0, 1.0/gamma) * 256.0;
		map[0][i] = map[1][i] = map[2][i] = val;
	}
}
