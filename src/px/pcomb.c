/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Combine picture files according to calcomp functions.
 *
 *	1/4/89
 */

#include <stdio.h>

#include <errno.h>

#include "color.h"

#include "calcomp.h"

#define MAXINP		32		/* maximum number of input files */
#define WINSIZ		9		/* scanline window size */
#define MIDSCN		4		/* current scan position */

#define BRT		(-1)		/* special index for brightness */

struct {
	char	*name;		/* file name */
	FILE	*fp;		/* stream pointer */
	COLOR	*scan[WINSIZ];	/* input scanline window */
	COLOR	coef;		/* coefficient */
}	input[MAXINP];			/* input pictures */

int	nfiles;				/* number of input files */

char	*vcolin[3] = {"ri", "gi", "bi"};
char	*vcolout[3] = {"ro", "go", "bo"};
char	vbrtin[] = "li";
char	vbrtout[] = "lo";
char	*vcolcoef[3] = {"rc", "gc", "bc"};
char	vbrtcoef[] = "lc";

#define vnfiles		"nfiles"
#define vxres		"xres"
#define vyres		"yres"
#define vxpos		"x"
#define vypos		"y"

int	nowarn = 0;			/* no warning messages? */

int	original = 0;			/* origninal values? */

int	xres=0, yres=0;			/* picture resolution */

int	xpos, ypos;			/* picture position */

int	wrongformat = 0;


main(argc, argv)
int	argc;
char	*argv[];
{
	double	f;
	int	a, i;
						/* scan options */
	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'x':
				xres = atoi(argv[++a]);
				continue;
			case 'y':
				yres = atoi(argv[++a]);
				continue;
			case 'w':
				nowarn = !nowarn;
				continue;
			case 'o':
				original = !original;
				continue;
			case 'f':
			case 'e':
				continue;
			}
		break;
	}
					/* process files */
	for (nfiles = 0; nfiles < MAXINP; nfiles++)
		setcolor(input[nfiles].coef, 1.0, 1.0, 1.0);
	nfiles = 0;
	for ( ; a < argc; a++) {
		if (nfiles >= MAXINP) {
			eputs(argv[0]);
			eputs(": too many picture files\n");
			quit(1);
		}
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case '\0':
				input[nfiles].name = "<stdin>";
				input[nfiles].fp = stdin;
				break;
			case 's':
				f = atof(argv[++a]);
				scalecolor(input[nfiles].coef, f);
				continue;
			case 'c':
				colval(input[nfiles].coef,RED)*=atof(argv[++a]);
				colval(input[nfiles].coef,GRN)*=atof(argv[++a]);
				colval(input[nfiles].coef,BLU)*=atof(argv[++a]);
				continue;
			default:
				goto usage;
			}
		else {
			input[nfiles].name = argv[a];
			input[nfiles].fp = fopen(argv[a], "r");
			if (input[nfiles].fp == NULL) {
				perror(argv[a]);
				quit(1);
			}
		}
		nfiles++;
	}
	initfiles();		/* initialize files and variables */
					/* go back and get expressions */
	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'x':
			case 'y':
			case 'w':
			case 'o':
				continue;
			case 'f':
				fcompile(argv[++a]);
				continue;
			case 'e':
				scompile(argv[++a], NULL, 0);
				continue;
			}
		break;
	}
						/* complete header */
	printargs(argc, argv, stdout);
	fputformat(COLRFMT, stdout);
	putchar('\n');
	fputresolu(YMAJOR|YDECR, xres, yres, stdout);
						/* combine pictures */
	combine();
	quit(0);
usage:
	eputs("Usage: ");
	eputs(argv[0]);
	eputs(
" [-w][-h][-x xr][-y yr][-e expr][-f file] [ [-s f][-c r g b] pic ..]\n");
	quit(1);
}


tputs(s)			/* put out string preceded by a tab */
char	*s;
{
	char	fmt[32];
	double	d;
	COLOR	ctmp;

	if (isformat(s)) {			/* check format */
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	} else if (original && isexpos(s)) {	/* exposure */
		d = 1.0/exposval(s);
		scalecolor(input[nfiles].coef, d);
	} else if (original && iscolcor(s)) {	/* color correction */
		colcorval(ctmp, s);
		colval(input[nfiles].coef,RED) /= colval(ctmp,RED);
		colval(input[nfiles].coef,GRN) /= colval(ctmp,GRN);
		colval(input[nfiles].coef,BLU) /= colval(ctmp,BLU);
	} else {				/* echo unaffected line */
		putchar('\t');
		fputs(s, stdout);
	}

}


initfiles()			/* ready files and set variables */
{
	double	l_colin(), l_coef();
	register int	n, i;
					/* process picture headers */
	for (n = 0; n < nfiles; n++) {
		fputs(input[n].name, stdout);
		fputs(":\n", stdout);
		getheader(input[n].fp, tputs, NULL);
		if (wrongformat) {
			eputs(input[n].name);
			eputs(": not in Radiance picture format\n");
			quit(1);
		}
		if (fgetresolu(&xpos, &ypos, input[n].fp) != (YMAJOR|YDECR)) {
			eputs(input[n].name);
			eputs(": bad picture size\n");
			quit(1);
		}
		if (xres == 0 && yres == 0) {
			xres = xpos;
			yres = ypos;
		} else if (xpos != xres || ypos != yres) {
			eputs(input[n].name);
			eputs(": resolution mismatch\n");
			quit(1);
		}
		for (i = 0; i < WINSIZ; i++)
			input[n].scan[i] = (COLOR *)emalloc(xres*sizeof(COLOR));
	}
						/* prime input */
	for (ypos = yres+(MIDSCN-1); ypos >= yres; ypos--)
		advance();
						/* define constants */
	varset(vnfiles, ':', (double)nfiles);
	varset(vxres, ':', (double)xres);
	varset(vyres, ':', (double)yres);
						/* set functions */
	for (i = 0; i < 3; i++) {
		funset(vcolcoef[i], 1, ':', l_coef);
		funset(vcolin[i], 1, '=', l_colin);
	}
	funset(vbrtcoef, 1, ':', l_coef);
	funset(vbrtin, 1, '=', l_colin);
}


combine()			/* combine pictures */
{
	EPNODE	*coldef[3], *brtdef;
	COLOR	*scanout;
	double	d;
	register int	i, j;
						/* check defined variables */
	for (j = 0; j < 3; j++) {
		if (vardefined(vcolout[j]))
			coldef[j] = eparse(vcolout[j]);
		else
			coldef[j] = NULL;
	}
	if (vardefined(vbrtout))
		brtdef = eparse(vbrtout);
	else
		brtdef = NULL;
						/* allocate scanline */
	scanout = (COLOR *)emalloc(xres*sizeof(COLOR));
						/* combine files */
	for (ypos = yres-1; ypos >= 0; ypos--) {
	    advance();
	    varset(vypos, '=', (double)ypos);
	    for (xpos = 0; xpos < xres; xpos++) {
		varset(vxpos, '=', (double)xpos);
		eclock++;
		if (brtdef != NULL) {
		    d = evalue(brtdef);
		    if (d < 0.0)
			d = 0.0;
		    setcolor(scanout[xpos], d, d, d);
		} else {
		    for (j = 0; j < 3; j++) {
			if (coldef[j] != NULL) {
				d = evalue(coldef[j]);
			} else {
			    d = 0.0;
			    for (i = 0; i < nfiles; i++)
				d += colval(input[i].scan[MIDSCN][xpos],j);
			}
			if (d < 0.0)
			    d = 0.0;
			colval(scanout[xpos],j) = d;
		    }
		}
	    }
	    if (fwritescan(scanout, xres, stdout) < 0) {
		    perror("write error");
		    quit(1);
	    }
	}
	efree(scanout);
}


advance()			/* read in next scanline */
{
	register COLOR	*st;
	register int	i, j;

	for (i = 0; i < nfiles; i++) {
		st = input[i].scan[WINSIZ-1];
		for (j = WINSIZ-1; j > 0; j--)	/* rotate window */
			input[i].scan[j] = input[i].scan[j-1];
		input[i].scan[0] = st;
		if (ypos < MIDSCN)		/* hit bottom */
			continue;
		if (freadscan(st, xres, input[i].fp) < 0) {
			eputs(input[i].name);
			eputs(": read error\n");
			quit(1);
		}
		for (j = 0; j < xres; j++)	/* adjust color */
			multcolor(st[j], input[i].coef);
	}
}


double
l_coef(nam)			/* return picture coefficients */
register char	*nam;
{
	register int	fn, n;
	double	d;

	fn = (d = argument(1)) - .5;
	if (d <= -.5 || fn >= nfiles) {
		errno = EDOM;
		return(0.0);
	}
	if (d < .5)
		return((double)nfiles);
	if (nam == vbrtcoef)
		return(bright(input[fn].coef));
	n = 3;
	while (n--)
		if (nam == vcolcoef[n])
			return(colval(input[fn].coef,n));
	eputs("Bad call to l_coef()!\n");
	quit(1);
}


double
l_colin(nam)			/* return color value for picture */
register char	*nam;
{
	int	fn;
	register int	n, xoff, yoff;
	double	d;

	fn = (d = argument(1)) - .5;
	if (d <= -.5 || fn >= nfiles) {
		errno = EDOM;
		return(0.0);
	}
	if (d < .5)
		return((double)nfiles);
	xoff = yoff = 0;
	n = nargum();
	if (n >= 2) {
		d = argument(2);
		if (d < 0.0) {
			xoff = d-.5;
			if (xpos+xoff < 0)
				xoff = -xpos;
		} else {
			xoff = d+.5;
			if (xpos+xoff >= xres)
				xoff = xres-1-xpos;
		}
	}
	if (n >= 3) {
		d = argument(3);
		if (d < 0.0) {
			yoff = d-.5;
			if (yoff+MIDSCN < 0)
				yoff = -MIDSCN;
			if (ypos+yoff < 0)
				yoff = -ypos;
		} else {
			yoff = d+.5;
			if (yoff+MIDSCN >= WINSIZ)
				yoff = WINSIZ-1-MIDSCN;
			if (ypos+yoff >= yres)
				yoff = yres-1-ypos;
		}
	}
	if (nam == vbrtin)
		return(bright(input[fn].scan[MIDSCN+yoff][xpos+xoff]));
	n = 3;
	while (n--)
	    if (nam == vcolin[n])
		return(colval(input[fn].scan[MIDSCN+yoff][xpos+xoff],n));
	eputs("Bad call to l_colin()!\n");
	quit(1);
}


wputs(msg)
char	*msg;
{
	if (!nowarn)
		eputs(msg);
}


eputs(msg)
char	*msg;
{
	fputs(msg, stderr);
}


quit(code)
int	code;
{
	exit(code);
}
