/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Replace markers in Radiance scene description with objects or instances.
 *
 *	Created:	17 Feb 1991	Greg Ward
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "fvect.h"

#ifdef  M_PI
#define  PI		M_PI
#else
#define  PI		3.14159265358979323846
#endif

#define  MAXVERT	6	/* maximum number of vertices for markers */

typedef struct {
	short	beg, end;		/* beginning and ending vertex */
	float	len2;			/* length squared */
}  EDGE;			/* a marker edge */

int	expand = 0;		/* expand commands? */

char	*modout = "void";	/* output modifier (for instances) */

double	markscale = 0.0;	/* scale markers by this to get unit */

char	*modin = NULL;		/* input modifier indicating marker */

char	*objname = NULL;	/* output object file (octree if instance) */
int	doxform;		/* true if xform, false if instance */

char	*progname;


main(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*fp;
	int	i, j;

	progname = argv[0];
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'i':
			doxform = 0;
			objname = argv[++i];
			break;
		case 'x':
			doxform = 1;
			objname = argv[++i];
			break;
		case 'e':
			expand = !expand;
			break;
		case 'm':
			modout = argv[++i];
			break;
		case 's':
			markscale = atof(argv[++i]);
			break;
		default:
			goto userr;
		}
	if (i < argc)
		modin = argv[i++];
	if (objname == NULL || modin == NULL)
		goto userr;
					/* simple header */
	putchar('#');
	for (j = 0; j < i; j++) {
		putchar(' ');
		fputs(argv[j], stdout);
	}
	putchar('\n');
	if (i == argc)
		convert("<stdin>", stdin);
	else
		for ( ; i < argc; i++) {
			if ((fp = fopen(argv[i], "r")) == NULL) {
				perror(argv[i]);
				exit(1);
			}
			convert(argv[i], fp);
			fclose(fp);
		}
	exit(0);
userr:
	fprintf(stderr,
"Usage: %s [-e][-s size][-m modout] {-x objfile|-i octree} modname [file ..]\n",
		progname);
	exit(1);
}


convert(name, fin)		/* replace marks in a stream */
char	*name;
register FILE	*fin;
{
	register int	c;

	while ((c = getc(fin)) != EOF) {
		if (isspace(c))				/* blank */
			continue;
		if (c == '#') {				/* comment */
			putchar(c);
			do {
				if ((c = getc(fin)) == EOF)
					return;
				putchar(c);
			} while (c != '\n');
		} else if (c == '!') {			/* command */
			ungetc(c, fin);
			cvcomm(name, fin);
		} else {				/* object */
			ungetc(c, fin);
			cvobject(name, fin);
		}
	}
}


cvcomm(fname, fin)		/* convert a command */
char	*fname;
FILE	*fin;
{
	FILE	*pin, *popen();
	char	buf[512], *fgetline();

	fgetline(buf, sizeof(buf), fin);
	if (expand) {
		if ((pin = popen(buf+1, "r")) == NULL) {
			fprintf(stderr,
			"%s: (%s): cannot execute \"%s\"\n",
					progname, fname, buf);
			exit(1);
		}
		convert(buf, pin);
		pclose(pin);
	} else
		printf("\n%s\n", buf);
}


cvobject(fname, fin)		/* convert an object */
char	*fname;
FILE	*fin;
{
	char	buf[128], typ[16], nam[128];
	int	i, j, n;

	if (fscanf(fin, "%s %s %s", buf, typ, nam) != 3)
		goto readerr;
	if (!strcmp(buf, modin) && !strcmp(typ, "polygon")) {
		replace(fname, nam, fin);
		return;
	}
	printf("\n%s %s %s\n", buf, typ, nam);
	if (!strcmp(typ, "alias")) {		/* alias special case */
		if (fscanf(fin, "%s", buf) != 1)
			goto readerr;
		printf("\t%s\n", buf);
		return;
	}
	for (i = 0; i < 3; i++) {		/* pass along arguments */
		if (fscanf(fin, "%d", &n) != 1)
			goto readerr;
		printf("%d", n);
		for (j = 0; j < n; j++) {
			if (fscanf(fin, "%s", buf) != 1)
				goto readerr;
			if (j%3 == 0)
				putchar('\n');
			printf("\t%s", buf);
		}
		putchar('\n');
	}
	return;
readerr:
	fprintf(stderr, "%s: (%s): read error for %s \"%s\"\n",
			progname, fname, typ, nam);
}


replace(fname, mark, fin)		/* replace marker */
char	*fname, *mark;
FILE	*fin;
{
	int	n;
	char	buf[256];

	if (doxform) {
		sprintf(buf, "xform -e -n %s", mark);
		if (buildxf(buf+strlen(buf), fin) < 0)
			goto badxf;
		sprintf(buf+strlen(buf), " %s", objname);
		if (expand) {
			fflush(stdout);
			system(buf);
		} else
			printf("\n!%s\n", buf);
	} else {
		if ((n = buildxf(buf, fin)) < 0)
			goto badxf;
		printf("\n%s instance %s\n", modout, mark);
		printf("%d %s%s\n", n+1, objname, buf);
		printf("\n0\n0\n");
	}
	return;
badxf:
	fprintf(stderr, "%s: (%s): bad arguments for marker \"%s\"\n",
			progname, fname, mark);
	exit(1);
}


edgecmp(e1, e2)			/* compare two edges, descending order */
EDGE	*e1, *e2;
{
	if (e1->len2 > e2->len2)
		return(-1);
	if (e1->len2 < e2->len2)
		return(1);
	return(0);
}


int
buildxf(xf, fin)		/* build transform for marker */
char	*xf;
FILE	*fin;
{
	static FVECT	vlist[MAXVERT];
	static EDGE	elist[MAXVERT];
	FVECT	xvec, yvec, zvec;
	double	xlen;
	int	n;
	register int	i;
	/*
	 * Read and sort vectors:  longest is hypotenuse,
	 *	second longest is x' axis,
	 *	third longest is y' axis (approximate),
	 *	other vectors are ignored.
	 * It is an error if the x' and y' axes do
	 *	not share a common vertex (their origin).
	 */
	if (fscanf(fin, "%*d %*d %d", &n) != 1)
		return(-1);
	if (n%3 != 0)
		return(-1);
	n /= 3;
	if (n < 3 || n > MAXVERT)
		return(-1);
					/* sort edges in descending order */
	for (i = 0; i < n; i++) {
		if (fscanf(fin, "%lf %lf %lf", &vlist[i][0],
				&vlist[i][1], &vlist[i][2]) != 3)
			return(-1);
		if (i) {
			elist[i].beg = i-1;
			elist[i].end = i;
			elist[i].len2 = dist2(vlist[i-1],vlist[i]);
		}
	}
	elist[0].beg = n-1;
	elist[0].end = 0;
	elist[0].len2 = dist2(vlist[n-1],vlist[0]);
	qsort(elist, n, sizeof(EDGE), edgecmp);
					/* find x' and y' */
	if (elist[1].end == elist[2].beg || elist[1].end == elist[2].end) {
		i = elist[1].beg;
		elist[1].beg = elist[1].end;
		elist[1].end = i;
	}
	if (elist[2].end == elist[1].beg) {
		i = elist[2].beg;
		elist[2].beg = elist[2].end;
		elist[2].end = i;
	}
	if (elist[1].beg != elist[2].beg)
		return(-1);		/* x' and y' not connected! */
	for (i = 0; i < 3; i++) {
		xvec[i] = vlist[elist[1].end][i] - vlist[elist[1].beg][i];
		yvec[i] = vlist[elist[2].end][i] - vlist[elist[2].beg][i];
	}
	if ((xlen = normalize(xvec)) == 0.0)
		return(-1);
	fcross(zvec, xvec, yvec);
	if (normalize(zvec) == 0.0)
		return(-1);
	fcross(yvec, zvec, xvec);
	n = 0;				/* start transformation... */
	if (markscale > 0.0) {		/* add scale factor */
		sprintf(xf, " -s %f", xlen*markscale);
		n += 2;
		xf += strlen(xf);
	}
					/* add rotation */
	n += addrot(xf, xvec, yvec, zvec);
	xf += strlen(xf);
					/* add translation */
	n += 4;
	sprintf(xf, " -t %f %f %f", vlist[elist[1].beg][0],
			vlist[elist[1].beg][1], vlist[elist[1].beg][2]);
	return(n);			/* all done */
}


addrot(xf, xp, yp, zp)		/* compute rotation (x,y,z) => (xp,yp,zp) */
char	*xf;
FVECT	xp, yp, zp;
{
	double	tx, ty, tz;

	tx = atan2(yp[2], zp[2]);
	ty = asin(-xp[2]);
	tz = atan2(xp[1], xp[0]);
	sprintf(xf, " -rx %f -ry %f -rz %f", tx*(180./PI),
			ty*(180./PI), tz*(180./PI));
	return(6);
}
