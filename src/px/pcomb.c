/* Copyright (c) 1989 Regents of the University of California */

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

#define vxpos		"x"
#define vypos		"y"

int	nowarn = 0;			/* no warning messages? */

int	xres=0, yres=0;			/* picture resolution */

int	xpos, ypos;			/* picture position */


tputs(s)			/* put out string preceded by a tab */
char	*s;
{
	putchar('\t');
	fputs(s, stdout);
}


main(argc, argv)
int	argc;
char	*argv[];
{
	extern double	l_redin(), l_grnin(), l_bluin(), atof();
	double	f;
	int	a;
	
	funset(vcolin[RED], 1, l_redin);
	funset(vcolin[GRN], 1, l_grnin);
	funset(vcolin[BLU], 1, l_bluin);
	
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
			case 'f':
				fcompile(argv[++a]);
				break;
			case 'e':
				scompile(NULL, argv[++a]);
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
		getheader(input[nfiles].fp, tputs);
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
	putchar('\n');
	fputresolu(YMAJOR|YDECR, xres, yres, stdout);
	combine();
	quit(0);
usage:
	eputs("Usage: ");
	eputs(argv[0]);
eputs(" [-w][-x xr][-y yr][-e expr][-f file] [ [-s f][-c r g b] picture ..]\n");
	quit(1);
}


combine()			/* combine pictures */
{
	EPNODE	*coldef[3];
	COLOR	*scanout;
	register int	i, j;
						/* check defined variables */
	for (j = 0; j < 3; j++) {
		if (vardefined(vcolout[j]))
			coldef[j] = eparse(vcolout[j]);
		else
			coldef[j] = NULL;
	}
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
		varset(vypos, (double)ypos);
		for (xpos = 0; xpos < xres; xpos++) {
			varset(vxpos, (double)xpos);
			eclock++;
			for (j = 0; j < 3; j++) {
				if (coldef[j] != NULL) {
					colval(scanout[xpos],j) =
						evalue(coldef[j]);
				} else {
					colval(scanout[xpos],j) = 0.0;
					for (i = 0; i < nfiles; i++)
						colval(scanout[xpos],j) +=
						colval(input[i].coef,j) *
						colval(input[i].scan[xpos],j);
				}
				if (colval(scanout[xpos],j) < 0.0)
					colval(scanout[xpos],j) = 0.0;
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
