/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert a Wavefront .obj file to Radiance format.
 *
 * Currently, we support only polygonal geometry, and faces
 * must be either quads or triangles for smoothing to work.
 * Also, texture indices only work for triangles, though
 * I'm not sure they work correctly.
 */

#include "standard.h"

#include <ctype.h>

#define VOIDID		"void"		/* this is defined in object.h */

#define TCALNAME	"tmesh.cal"	/* triangle interp. file */
#define QCALNAME	"surf.cal"	/* quad interp. file */
#define PATNAME		"M-pat"		/* mesh pattern name (reused) */
#define TEXNAME		"M-nor"		/* mesh texture name (reused) */

#define  ABS(x)		((x)>=0 ? (x) : -(x))

#define pvect(v)	printf("%18.12g %18.12g %18.12g\n",(v)[0],(v)[1],(v)[2])

FVECT	*vlist;			/* our vertex list */
int	nvs;			/* number of vertices in our list */
FVECT	*vnlist;		/* vertex normal list */
int	nvns;
FLOAT	(*vtlist)[2];		/* texture vertex list */
int	nvts;

typedef FLOAT	BARYCCM[3][4];	/* barycentric coordinate system */

typedef int	VNDX[3];	/* vertex index (point,texture,normal) */

#define CHUNKSIZ	128	/* vertex allocation chunk size */

#define MAXARG		64	/* maximum # arguments in a statement */

char	*defmat = VOIDID;	/* default (starting) material name */
char	*defpat = "";		/* default (starting) picture name */
char	*defobj = "F";		/* default (starting) object name */

char	picfile[128];		/* current picture file */
char	matname[64];		/* current material name */
char	objname[128];		/* current object name */
int	lineno;			/* current line number */

int	nfaces;			/* total number of faces output */


main(argc, argv)		/* read in T-mesh files and convert */
int	argc;
char	*argv[];
{
	FILE	*fp;
	int	i;

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'o':		/* object name */
			defobj = argv[++i];
			break;
		case 'm':		/* default material */
			defmat = argv[++i];
			break;
		case 'p':		/* default picture */
			defpat = argv[++i];
			break;
		default:
			fprintf(stderr,
			"Usage: %s [-o obj][-m mat][-p pic] [file ..]\n",
					argv[0]);
			exit(1);
		}
	if (i >= argc)
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
}


convert(fname, fp)		/* convert a T-mesh */
char	*fname;
FILE	*fp;
{
	char	*argv[MAXARG];
	int	argc;
	int	nstats, nunknown;
	register int	i;
					/* start fresh */
	freeverts();
	strcpy(picfile, defpat);
	strcpy(matname, defmat);
	strcpy(objname, defobj);
	lineno = 0;
	nstats = nunknown = 0;

	printf("\n# Wavefront file read from: %s\n", fname);
					/* scan until EOF */
	while (argc = getstmt(argv, fp)) {
		switch (argv[0][0]) {
		case 'v':		/* vertex */
			switch (argv[0][1]) {
			case '\0':			/* point */
				if (badarg(argc-1,argv+1,"fff"))
					syntax(fname, lineno, "Bad vertex");
				newv(atof(argv[1]), atof(argv[2]),
						atof(argv[3]));
				break;
			case 'n':			/* normal */
				if (argv[0][2])
					goto unknown;
				if (badarg(argc-1,argv+1,"fff"))
					syntax(fname, lineno, "Bad normal");
				if (!newvn(atof(argv[1]), atof(argv[2]),
						atof(argv[3])))
					syntax(fname, lineno, "Zero normal");
				break;
			case 't':			/* texture */
				if (argv[0][2])
					goto unknown;
				if (badarg(argc-1,argv+1,"ff"))
					goto unknown;
				newvt(atof(argv[1]), atof(argv[2]));
				break;
			default:
				goto unknown;
			}
			break;
		case 'f':				/* face */
			if (argv[0][1])
				goto unknown;
			switch (argc-1) {
			case 0: case 1: case 2:
				syntax(fname, lineno, "Too few vertices");
				break;
			case 3:
				if (!puttri(argv[1], argv[2], argv[3]))
					syntax(fname, lineno, "Bad triangle");
				break;
			case 4:
				if (!putquad(argv[1], argv[2],
						argv[3], argv[4]))
					syntax(fname, lineno, "Bad quad");
				break;
			default:
				if (!putface(argc-1, argv+1))
					syntax(fname, lineno, "Bad face");
				break;
			}
			break;
		case 'u':
			if (!strcmp(argv[0], "usemtl")) {	/* material */
				if (argc < 2)
					break;		/* not fatal */
				strcpy(matname, argv[1]);
			} else if (!strcmp(argv[0], "usemap")) {/* map */
				if (argc < 2)
					break;		/* not fatal */
				if (!strcmp(argv[1], "off"))
					picfile[0] = '\0';
				else
					strcpy(picfile, argv[1]);
			} else
				goto unknown;
			break;
		case 'o':		/* object name */
			if (argv[0][1])
				goto unknown;
			if (argc < 2)
				break;		/* not fatal */
			strcpy(objname, argv[1]);
			break;
		case 'g':		/* group name(s) */
			if (argv[0][1])
				goto unknown;
			if (argc < 2)
				break;		/* not fatal */
			objname[0] = '\0';
			for (i = 1; i < argc; i++)
				if (objname[0])
					sprintf(objname+strlen(objname),
						".%s", argv[i]);
				else
					strcpy(objname, argv[i]);
			break;
		case '#':		/* comment */
			break;
		default:;		/* something we don't deal with */
		unknown:
			nunknown++;
			break;
		}
		nstats++;
	}
	printf("\n# Done processing file: %s\n", fname);
	printf("# %d lines, %d statements, %d unrecognized\n",
			lineno, nstats, nunknown);
}


int
getstmt(av, fp)				/* read the next statement from fp */
register char	*av[MAXARG];
FILE	*fp;
{
	extern char	*fgetline();
	static char	sbuf[MAXARG*10];
	register char	*cp;
	register int	i;

	do {
		if (fgetline(cp=sbuf, sizeof(sbuf), fp) == NULL)
			return(0);
		i = 0;
		for ( ; ; ) {
			while (isspace(*cp) || *cp == '\\') {
				if (*cp == '\n')
					lineno++;
				*cp++ = '\0';
			}
			if (!*cp || i >= MAXARG-1)
				break;
			av[i++] = cp;
			while (*++cp && !isspace(*cp))
				;
		}
		av[i] = NULL;
		lineno++;
	} while (!i);

	return(i);
}
		

cvtndx(vi, vs)				/* convert vertex string to index */
register VNDX	vi;
register char	*vs;
{
					/* get point */
	vi[0] = atoi(vs);
	if (vi[0] > 0) {
		if (vi[0]-- > nvs)
			return(0);
	} else if (vi[0] < 0) {
		vi[0] = nvs + vi[0];
		if (vi[0] < 0)
			return(0);
	} else
		return(0);
					/* get texture */
	while (*vs)
		if (*vs++ == '/')
			break;
	vi[1] = atoi(vs);
	if (vi[1] > 0) {
		if (vi[1]-- > nvts)
			return(0);
	} else if (vi[1] < 0) {
		vi[1] = nvts + vi[1];
		if (vi[1] < 0)
			return(0);
	} else
		vi[1] = -1;
					/* get normal */
	while (*vs)
		if (*vs++ == '/')
			break;
	vi[2] = atoi(vs);
	if (vi[2] > 0) {
		if (vi[2]-- > nvns)
			return(0);
	} else if (vi[2] < 0) {
		vi[2] = nvns + vi[2];
		if (vi[2] < 0)
			return(0);
	} else
		vi[2] = -1;
	return(1);
}


putface(ac, av)				/* put out an N-sided polygon */
register int	ac;
register char	**av;
{
	VNDX	vi;

	printf("\n%s polygon %s.%d\n", matname, objname, ++nfaces);
	printf("0\n0\n%d\n", 3*ac);
	while (ac--) {
		if (!cvtndx(vi, *av++))
			return(0);
		pvect(vlist[vi[0]]);
	}
	return(1);
}


puttri(v1, v2, v3)			/* put out a triangle */
char	*v1, *v2, *v3;
{
	char	*mod = matname;
	VNDX	v1i, v2i, v3i;
	BARYCCM	bvecs;
	int	texOK, patOK;

	if (!cvtndx(v1i, v1) || !cvtndx(v2i, v2) || !cvtndx(v3i, v3))
		return(0);
					/* compute barycentric coordinates */
	texOK = (v1i[2]>=0 && v2i[2]>=0 && v3i[2]>=0);
	patOK = picfile[0] && (v1i[1]>=0 && v2i[1]>=0 && v3i[1]>=0);
	if (texOK | patOK)
		if (comp_baryc(bvecs, vlist[v1i[0]], vlist[v2i[0]],
				vlist[v3i[0]]) < 0)
			return(-1);
					/* put out texture (if any) */
	if (texOK) {
		printf("\n%s texfunc %s\n", mod, TEXNAME);
		mod = TEXNAME;
		printf("4 dx dy dz %s\n", TCALNAME);
		printf("0\n21\n");
		put_baryc(bvecs);
		printf("\t%14.12g %14.12g %14.12g\n",
				vnlist[v1i[2]][0], vnlist[v2i[2]][0],
				vnlist[v3i[2]][0]);
		printf("\t%14.12g %14.12g %14.12g\n",
				vnlist[v1i[2]][1], vnlist[v2i[2]][1],
				vnlist[v3i[2]][1]);
		printf("\t%14.12g %14.12g %14.12g\n",
				vnlist[v1i[2]][2], vnlist[v2i[2]][2],
				vnlist[v3i[2]][2]);
	}
					/* put out pattern (if any) */
	if (patOK) {
		printf("\n%s colorpict %s\n", mod, PATNAME);
		mod = PATNAME;
		printf("7 noneg noneg noneg %s %s u v\n", picfile, TCALNAME);
		printf("0\n18\n");
		put_baryc(bvecs);
		printf("\t%f %f %f\n", vtlist[v1i[1]][0],
				vtlist[v2i[1]][0], vtlist[v3i[1]][0]);
		printf("\t%f %f %f\n", vtlist[v1i[1]][1],
				vtlist[v2i[1]][1], vtlist[v3i[1]][1]);
	}
					/* put out triangle */
	printf("\n%s polygon %s.%d\n", mod, objname, ++nfaces);
	printf("0\n0\n9\n");
	pvect(vlist[v1i[0]]);
	pvect(vlist[v2i[0]]);
	pvect(vlist[v3i[0]]);

	return(1);
}


int
comp_baryc(bcm, v1, v2, v3)		/* compute barycentric vectors */
register BARYCCM	bcm;
FLOAT	*v1, *v2, *v3;
{
	FLOAT	*vt;
	FVECT	va, vab, vcb;
	double	d;
	register int	i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			vab[i] = v1[i] - v2[i];
			vcb[i] = v3[i] - v2[i];
		}
		d = DOT(vcb,vcb);
		if (d <= FTINY)
			return(-1);
		d = DOT(vcb,vab)/d;
		for (i = 0; i < 3; i++)
			va[i] = vab[i] - vcb[i]*d;
		d = DOT(va,va);
		if (d <= FTINY)
			return(-1);
		for (i = 0; i < 3; i++) {
			va[i] /= d;
			bcm[j][i] = va[i];
		}
		bcm[j][3] = -DOT(v2,va);
					/* rotate vertices */
		vt = v1;
		v1 = v2;
		v2 = v3;
		v3 = vt;
	}
	return(0);
}


put_baryc(bcm)				/* put barycentric coord. vectors */
register BARYCCM	bcm;
{
	register int	i;

	for (i = 0; i < 3; i++)
		printf("%14.8f %14.8f %14.8f %14.8f\n",
				bcm[i][0], bcm[i][1], bcm[i][2], bcm[i][3]);
}


putquad(p0, p1, p2, p3)			/* put out a quadrilateral */
char  *p0, *p1, *p2, *p3;
{
	VNDX  p0i, p1i, p2i, p3i;
	FVECT  norm[4];
	int  axis;
	FVECT  v1, v2, vc1, vc2;
	int  ok1, ok2;
					/* get actual indices */
	if (!cvtndx(p0i,p0) || !cvtndx(p1i,p1) ||
			!cvtndx(p2i,p2) || !cvtndx(p3i,p3))
		return(0);
					/* compute exact normals */
	fvsum(v1, vlist[p1i[0]], vlist[p0i[0]], -1.0);
	fvsum(v2, vlist[p2i[0]], vlist[p0i[0]], -1.0);
	fcross(vc1, v1, v2);
	ok1 = normalize(vc1) != 0.0;
	fvsum(v1, vlist[p2i[0]], vlist[p3i[0]], -1.0);
	fvsum(v2, vlist[p1i[0]], vlist[p3i[0]], -1.0);
	fcross(vc2, v1, v2);
	ok2 = normalize(vc2) != 0.0;
	if (!(ok1 | ok2))
		return(-1);
					/* compute normal interpolation */
	axis = norminterp(norm, p0i, p1i, p2i, p3i);

					/* put out quadrilateral? */
	if (ok1 & ok2 && fdot(vc1,vc2) >= 1.0-FTINY*FTINY) {
		printf("\n%s ", matname);
		if (axis != -1) {
			printf("texfunc %s\n", TEXNAME);
			printf("4 surf_dx surf_dy surf_dz %s\n", QCALNAME);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v1, norm[3], vc1, -0.5);
			fvsum(v1, v1, vc2, -0.5);
			pvect(v1);
			printf("\n%s ", TEXNAME);
		}
		printf("polygon %s.%d\n", objname, ++nfaces);
		printf("0\n0\n12\n");
		pvect(vlist[p0i[0]]);
		pvect(vlist[p1i[0]]);
		pvect(vlist[p3i[0]]);
		pvect(vlist[p2i[0]]);
		return(1);
	}
					/* put out triangles? */
	if (ok1) {
		printf("\n%s ", matname);
		if (axis != -1) {
			printf("texfunc %s\n", TEXNAME);
			printf("4 surf_dx surf_dy surf_dz %s\n", QCALNAME);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v1, norm[3], vc1, -1.0);
			pvect(v1);
			printf("\n%s ", TEXNAME);
		}
		printf("polygon %s.%d\n", objname, ++nfaces);
		printf("0\n0\n9\n");
		pvect(vlist[p0i[0]]);
		pvect(vlist[p1i[0]]);
		pvect(vlist[p2i[0]]);
	}
	if (ok2) {
		printf("\n%s ", matname);
		if (axis != -1) {
			printf("texfunc %s\n", TEXNAME);
			printf("4 surf_dx surf_dy surf_dz %s\n", QCALNAME);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v2, norm[3], vc2, -1.0);
			pvect(v2);
			printf("\n%s ", TEXNAME);
		}
		printf("polygon %s.%d\n", objname, ++nfaces);
		printf("0\n0\n9\n");
		pvect(vlist[p2i[0]]);
		pvect(vlist[p1i[0]]);
		pvect(vlist[p3i[0]]);
	}
	return(1);
}


int
norminterp(resmat, p0i, p1i, p2i, p3i)	/* compute normal interpolation */
register FVECT  resmat[4];
register VNDX  p0i, p1i, p2i, p3i;
{
#define u  ((ax+1)%3)
#define v  ((ax+2)%3)

	register int  ax;
	MAT4  eqnmat;
	FVECT  v1;
	register int  i, j;

	if (!(p0i[2]>=0 && p1i[2]>=0 && p2i[2]>=0 && p3i[2]>=0))
		return(-1);
					/* find dominant axis */
	VCOPY(v1, vnlist[p0i[2]]);
	fvsum(v1, v1, vnlist[p1i[2]], 1.0);
	fvsum(v1, v1, vnlist[p2i[2]], 1.0);
	fvsum(v1, v1, vnlist[p3i[2]], 1.0);
	ax = ABS(v1[0]) > ABS(v1[1]) ? 0 : 1;
	ax = ABS(v1[ax]) > ABS(v1[2]) ? ax : 2;
					/* assign equation matrix */
	eqnmat[0][0] = vlist[p0i[0]][u]*vlist[p0i[0]][v];
	eqnmat[0][1] = vlist[p0i[0]][u];
	eqnmat[0][2] = vlist[p0i[0]][v];
	eqnmat[0][3] = 1.0;
	eqnmat[1][0] = vlist[p1i[0]][u]*vlist[p1i[0]][v];
	eqnmat[1][1] = vlist[p1i[0]][u];
	eqnmat[1][2] = vlist[p1i[0]][v];
	eqnmat[1][3] = 1.0;
	eqnmat[2][0] = vlist[p2i[0]][u]*vlist[p2i[0]][v];
	eqnmat[2][1] = vlist[p2i[0]][u];
	eqnmat[2][2] = vlist[p2i[0]][v];
	eqnmat[2][3] = 1.0;
	eqnmat[3][0] = vlist[p3i[0]][u]*vlist[p3i[0]][v];
	eqnmat[3][1] = vlist[p3i[0]][u];
	eqnmat[3][2] = vlist[p3i[0]][v];
	eqnmat[3][3] = 1.0;
					/* invert matrix (solve system) */
	if (!invmat4(eqnmat, eqnmat))
		return(-1);			/* no solution */
					/* compute result matrix */
	for (j = 0; j < 4; j++)
		for (i = 0; i < 3; i++)
			resmat[j][i] =	eqnmat[j][0]*vnlist[p0i[2]][i] +
					eqnmat[j][1]*vnlist[p1i[2]][i] +
					eqnmat[j][2]*vnlist[p2i[2]][i] +
					eqnmat[j][3]*vnlist[p3i[2]][i];
	return(ax);

#undef u
#undef v
}


freeverts()			/* free all vertices */
{
	if (nvs) {
		free((char *)vlist);
		nvs = 0;
	}
	if (nvts) {
		free((char *)vtlist);
		nvts = 0;
	}
	if (nvns) {
		free((char *)vnlist);
		nvns = 0;
	}
}


int
newv(x, y, z)			/* create a new vertex */
double	x, y, z;
{
	if (!(nvs%CHUNKSIZ)) {		/* allocate next block */
		if (nvs == 0)
			vlist = (FVECT *)malloc(CHUNKSIZ*sizeof(FVECT));
		else
			vlist = (FVECT *)realloc((char *)vlist,
					(nvs+CHUNKSIZ)*sizeof(FVECT));
		if (vlist == NULL) {
			fprintf(stderr,
			"Out of memory while allocating vertex %d\n", nvs);
			exit(1);
		}
	}
					/* assign new vertex */
	vlist[nvs][0] = x;
	vlist[nvs][1] = y;
	vlist[nvs][2] = z;
	return(++nvs);
}


int
newvn(x, y, z)			/* create a new vertex normal */
double	x, y, z;
{
	if (!(nvns%CHUNKSIZ)) {		/* allocate next block */
		if (nvns == 0)
			vnlist = (FVECT *)malloc(CHUNKSIZ*sizeof(FVECT));
		else
			vnlist = (FVECT *)realloc((char *)vnlist,
					(nvns+CHUNKSIZ)*sizeof(FVECT));
		if (vnlist == NULL) {
			fprintf(stderr,
			"Out of memory while allocating normal %d\n", nvns);
			exit(1);
		}
	}
					/* assign new normal */
	vnlist[nvns][0] = x;
	vnlist[nvns][1] = y;
	vnlist[nvns][2] = z;
	if (normalize(vnlist[nvns]) == 0.0)
		return(0);
	return(++nvns);
}


int
newvt(x, y)			/* create a new texture vertex */
double	x, y;
{
	if (!(nvts%CHUNKSIZ)) {		/* allocate next block */
		if (nvts == 0)
			vtlist = (FLOAT (*)[2])malloc(CHUNKSIZ*2*sizeof(FLOAT));
		else
			vtlist = (FLOAT (*)[2])realloc((char *)vtlist,
					(nvts+CHUNKSIZ)*2*sizeof(FLOAT));
		if (vtlist == NULL) {
			fprintf(stderr,
			"Out of memory while allocating texture vertex %d\n",
					nvts);
			exit(1);
		}
	}
					/* assign new texture vertex */
	vtlist[nvts][0] = x;
	vtlist[nvts][1] = y;
	return(++nvts);
}


syntax(fn, ln, er)			/* report syntax error and exit */
char	*fn;
int	ln;
char	*er;
{
	fprintf(stderr, "%s: Wavefront syntax error near line %d: %s\n",
			fn, ln, er);
	exit(1);
}
