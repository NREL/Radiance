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

struct {
	char	*name;		/* file name */
	FILE	*fp;		/* stream pointer */
	COLOR	*scan;		/* input scanline */
	COLOR	coef;		/* coefficient */
}	input[MAXINP];			/* input pictures */

int	nfiles;				/* number of input files */

char	*vcolin[3] = {"ri", "gi", "bi"};
char	*vcolout[3] = {"ro", "go", "bo"};
#define	vbrtin		"li"
#define vbrtout		"lo"

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


main(argc, argv)
int	argc;
char	*argv[];
{
	extern double	l_redin(), l_grnin(), l_bluin(), l_brtin(), atof();
	double	f;
	int	a;
	
	funset(vcolin[RED], 1, '=', l_redin);
	funset(vcolin[GRN], 1, '=', l_grnin);
	funset(vcolin[BLU], 1, '=', l_bluin);
	funset(vbrtin, 1, '=', l_brtin);
	
	for (a = 1; a < argc; a++)
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case '\0':
			case 's':
			case 'c':
				goto getfiles;
			case 'x':
				xres = atoi(argv[++a]);
				break;
			case 'y':
				yres = atoi(argv[++a]);
				break;
			case 'w':
				nowarn = !nowarn;
				break;
			case 'o':
				original = !original;
				break;
			case 'f':
				fcompile(argv[++a]);
				break;
			case 'e':
				scompile(argv[++a], NULL, 0);
				break;
 			default:
				goto usage;
			}
		else
			break;
getfiles:
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
		fputs(input[nfiles].name, stdout);
		fputs(":\n", stdout);
		getheader(input[nfiles].fp, tputs, NULL);
		if (wrongformat) {
			eputs(input[nfiles].name);
			eputs(": not in Radiance picture format\n");
			quit(1);
		}
		if (fgetresolu(&xpos, &ypos, input[nfiles].fp) !=
				(YMAJOR|YDECR)) {
			eputs(input[nfiles].name);
			eputs(": bad picture size\n");
			quit(1);
		}
		if (xres == 0 && yres == 0) {
			xres = xpos;
			yres = ypos;
		} else if (xpos != xres || ypos != yres) {
			eputs(argv[0]);
			eputs(": resolution mismatch\n");
			quit(1);
		}
		input[nfiles].scan = (COLOR *)emalloc(xres*sizeof(COLOR));
		nfiles++;
	}
	printargs(argc, argv, stdout);
	fputformat(COLRFMT, stdout);
	putchar('\n');
	fputresolu(YMAJOR|YDECR, xres, yres, stdout);
	combine();
	quit(0);
usage:
	eputs("Usage: ");
	eputs(argv[0]);
	eputs(
" [-w][-h][-x xr][-y yr][-e expr][-f file] [ [-s f][-c r g b] pic ..]\n");
	quit(1);
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
						/* predefine variables */
	varset(vnfiles, '=', (double)nfiles);
	varset(vxres, '=', (double)xres);
	varset(vyres, '=', (double)yres);
						/* allocate scanline */
	scanout = (COLOR *)emalloc(xres*sizeof(COLOR));
						/* combine files */
	for (ypos = yres-1; ypos >= 0; ypos--) {
	    for (i = 0; i < nfiles; i++)
		    if (freadscan(input[i].scan, xres, input[i].fp) < 0) {
			    eputs(input[i].name);
			    eputs(": read error\n");
			    quit(1);
		    }
	    varset(vypos, '=', (double)ypos);
	    for (xpos = 0; xpos < xres; xpos++) {
		for (i = 0; i < nfiles; i++)
			multcolor(input[i].scan[xpos],input[i].coef);
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
				d += colval(input[i].scan[xpos],j);
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


double
colin(fn, ci)			/* return color value for picture */
register int	fn, ci;
{
	if (fn == 0)
		return((double)nfiles);
	if (fn < 1 || fn > nfiles) {
		errno = EDOM;
		return(0.0);
	}
	return(colval(input[fn-1].scan[xpos],ci));
}


double
l_redin()			/* get red color */
{
	return(colin((int)(argument(1)+.5), RED));
}


double
l_grnin()			/* get green color */
{
	return(colin((int)(argument(1)+.5), GRN));
}


double
l_bluin()			/* get blue color */
{
	return(colin((int)(argument(1)+.5), BLU));
}


double
l_brtin()			/* get brightness value */
{
	register int	fn;

	fn = argument(1)+.5;
	if (fn == 0)
		return((double)nfiles);
	if (fn < 1 || fn > nfiles) {
		errno = EDOM;
		return(0.0);
	}
	return(bright(input[fn-1].scan[xpos]));
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
