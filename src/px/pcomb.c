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

#define MAXINP		16		/* maximum number of input files */

struct {
	char	*name;		/* file name */
	FILE	*fp;		/* stream pointer */
	COLOR	*scan;		/* input scanline */
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
	extern double	l_redin(), l_grnin(), l_bluin();
	int	a;
	
	funset(vcolin[RED], 1, l_redin);
	funset(vcolin[GRN], 1, l_grnin);
	funset(vcolin[BLU], 1, l_bluin);
	
	for (a = 1; a < argc; a++)
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case '\0':
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
				eputs("Usage: ");
				eputs(argv[0]);
	eputs(" [-w][-x xres][-y yres][-e expr][-f file] [picture ..]\n");
				quit(1);
			}
		else
			break;
getfiles:
	nfiles = 0;
	for ( ; a < argc; a++) {
		if (nfiles >= MAXINP) {
			eputs(argv[0]);
			eputs(": too many picture files\n");
			quit(1);
		}
		if (argv[a][0] == '-') {
			input[nfiles].name = "<stdin>";
			input[nfiles].fp = stdin;
		} else {
			input[nfiles].name = argv[a];
			input[nfiles].fp = fopen(argv[a], "r");
			if (input[nfiles].fp == NULL) {
				eputs(argv[a]);
				eputs(": cannot open\n");
				quit(1);
			}
		}
		fputs(input[nfiles].name, stdout);
		fputs(":\n", stdout);
		getheader(input[nfiles].fp, tputs);
		if (fscanf(input[nfiles].fp, "-Y %d +X %d\n", &ypos, &xpos) != 2) {
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
	printf("-Y %d +X %d\n", yres, xres);
	combine();
	quit(0);
}


combine()			/* combine pictures */
{
	int	coldef[3];
	COLOR	*scanout;
	register int	i, j;
						/* check defined variables */
	for (j = 0; j < 3; j++)
		coldef[j] = vardefined(vcolout[j]);
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
			for (j = 0; j < 3; j++)
				if (coldef[j]) {
					colval(scanout[xpos],j) = varvalue(vcolout[j]);
					if (colval(scanout[xpos],j) < 0.0)
						colval(scanout[xpos],j) = 0.0;
				} else {
					colval(scanout[xpos],j) = 0.0;
					for (i = 0; i < nfiles; i++)
						colval(scanout[xpos],j) += colval(input[i].scan[xpos],j);
					colval(scanout[xpos],j) /= (double)nfiles;
				}
		}
		if (fwritescan(scanout, xres, stdout) < 0) {
			eputs("write error\n");
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
