#ifndef lint
static const char RCSid[] = "$Id$";
#endif
/*
 *  Load Wavefront .OBJ file and convert to triangles with mesh info.
 *  Code borrowed largely from obj2rad.c
 */

#include "copyright.h"
#include "standard.h"
#include "cvmesh.h"
#include "triangulate.h"
#include <ctype.h>

typedef int	VNDX[3];	/* vertex index (point,map,normal) */

#define CHUNKSIZ	1024	/* vertex allocation chunk size */

#define MAXARG		512	/* maximum # arguments in a statement */

static FVECT	*vlist;		/* our vertex list */
static int	nvs;		/* number of vertices in our list */
static FVECT	*vnlist;	/* vertex normal list */
static int	nvns;
static RREAL	(*vtlist)[2];	/* map vertex list */
static int	nvts;

static char	*inpfile;	/* input file name */
static int	havemats;	/* materials available? */
static char	material[256];	/* current material name */
static char	group[256];	/* current group name */
static int	lineno;		/* current line number */
static int	faceno;		/* current face number */

static int getstmt(char	*av[MAXARG], FILE *fp);
static int cvtndx(VNDX vi, char *vs);
static int putface(int ac, char **av);
static OBJECT getmod(void);
static int puttri(char	*v1, char *v2, char *v3);
static void freeverts(void);
static int newv(double	x, double y, double z);
static int newvn(double	x, double y, double z);
static int newvt(double	x, double y);
static void syntax(char	*er);


void
wfreadobj(		/* read in .OBJ file and convert */
	char	*objfn
)
{
	FILE	*fp;
	char	*argv[MAXARG];
	int	argc;
	int	nstats, nunknown;

	if (objfn == NULL) {
		inpfile = "<stdin>";
		fp = stdin;
	} else if ((fp = fopen(inpfile=objfn, "r")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\"", inpfile);
		error(USER, errmsg);
	}
	havemats = (nobjects > 0);
	nstats = nunknown = 0;
	material[0] = '\0';
	group[0] = '\0';
	lineno = 0; faceno = 0;
					/* scan until EOF */
	while ( (argc = getstmt(argv, fp)) ) {
		switch (argv[0][0]) {
		case 'v':		/* vertex */
			switch (argv[0][1]) {
			case '\0':			/* point */
				if (badarg(argc-1,argv+1,"fff"))
					syntax("bad vertex");
				newv(atof(argv[1]), atof(argv[2]),
						atof(argv[3]));
				break;
			case 'n':			/* normal */
				if (argv[0][2])
					goto unknown;
				if (badarg(argc-1,argv+1,"fff"))
					syntax("bad normal");
				if (!newvn(atof(argv[1]), atof(argv[2]),
						atof(argv[3])))
					syntax("zero normal");
				break;
			case 't':			/* coordinate */
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
			faceno++;
			switch (argc-1) {
			case 0: case 1: case 2:
				syntax("too few vertices");
				break;
			case 3:
				if (!puttri(argv[1], argv[2], argv[3]))
					syntax("bad triangle");
				break;
			default:
				if (!putface(argc-1, argv+1))
					syntax("bad face");
				break;
			}
			break;
		case 'u':				/* usemtl/usemap */
			if (!strcmp(argv[0], "usemap"))
				break;
			if (strcmp(argv[0], "usemtl"))
				goto unknown;
			if (argc > 1)
				strcpy(material, argv[1]);
			else
				material[0] = '\0';
			break;
		case 'o':		/* object name */
			if (argv[0][1])
				goto unknown;
			break;
		case 'g':		/* group name */
			if (argv[0][1])
				goto unknown;
			if (argc > 1)
				strcpy(group, argv[1]);
			else
				group[0] = '\0';
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
				/* clean up */
	freeverts();
	fclose(fp);
	if (nunknown > 0) {
		sprintf(errmsg, "%d of %d statements unrecognized",
				nunknown, nstats);
		error(WARNING, errmsg);
	}
}


static int
getstmt(				/* read the next statement from fp */
	char	*av[MAXARG],
	FILE	*fp
)
{
	static char	sbuf[MAXARG*16];
	char	*cp;
	int	i;

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
			if (!*cp)
				break;
			if (i >= MAXARG-1) {
				sprintf(errmsg,
			"%s: too many arguments near line %d (limit %d)\n",
					inpfile, lineno+1, MAXARG-1);
				break;
			}
			av[i++] = cp;
			while (*++cp && !isspace(*cp))
				;
		}
		av[i] = NULL;
		lineno++;
	} while (!i);

	return(i);
}


static int
cvtndx(				/* convert vertex string to index */
	VNDX	vi,
	char	*vs
)
{
					/* get point */
	vi[0] = atoi(vs);
	if (vi[0] > 0) {
		if (vi[0]-- > nvs)
			return(0);
	} else if (vi[0] < 0) {
		vi[0] += nvs;
		if (vi[0] < 0)
			return(0);
	} else
		return(0);
					/* get map coord. */
	while (*vs)
		if (*vs++ == '/')
			break;
	vi[1] = atoi(vs);
	if (vi[1] > 0) {
		if (vi[1]-- > nvts)
			return(0);
	} else if (vi[1] < 0) {
		vi[1] += nvts;
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
		vi[2] += nvns;
		if (vi[2] < 0)
			return(0);
	} else
		vi[2] = -1;
	return(1);
}

/* determine dominant axis for triangle */
static int
dominant_axis(char *v1, char *v2, char *v3)
{
	VNDX	v1i, v2i, v3i;
	FVECT	e1, e2, vn;
	int	i, imax;

	if (!cvtndx(v1i, v1) || !cvtndx(v2i, v2) || !cvtndx(v3i, v3))
		return(-1);
	VSUB(e1, vlist[v2i[0]], vlist[v1i[0]]);
	VSUB(e2, vlist[v3i[0]], vlist[v2i[0]]);
	VCROSS(vn, e1, e2);
	for (i = imax = 2; i--; )
		if (vn[i]*vn[i] > vn[imax]*vn[imax])
			imax = i;
	return(vn[imax]*vn[imax] > FTINY*FTINY ? imax : -1);
}

/* callback for triangle output from polygon */
static int
tri_out(const Vert2_list *tp, int a, int b, int c)
{
	return( puttri(	((char **)tp->p)[a],
			((char **)tp->p)[b],
			((char **)tp->p)[c] ) );
}

static int
putface(				/* put out an N-sided polygon */
	int	ac,
	char	**av
)
{
	Vert2_list	*poly = polyAlloc(ac);
	int		i, ax, ay;

	if (poly == NULL)
		return(0);
	poly->p = (void *)av;
	for (i = ac-3; i >= 0; i--)	/* identify dominant axis */
		if ((ax = dominant_axis(av[i], av[i+1], av[i+2])) >= 0)
			break;
	if (ax < 0)
		return(1);		/* ignore degenerate face */
	if (++ax >= 3) ax = 0;
	ay = ax;
	if (++ay >= 3) ay = 0;
	for (i = 0; i < ac; i++) {	/* convert to 2-D polygon */
		VNDX	vi;
		if (!cvtndx(vi, av[i])) {
			error(WARNING, "bad vertex reference");
			polyFree(poly);
			return(0);
		}
		poly->v[i].mX = vlist[vi[0]][ax];
		poly->v[i].mY = vlist[vi[0]][ay];
	}
					/* break into triangles & output */
	if (!polyTriangulate(poly, &tri_out)) {
		sprintf(errmsg, "self-intersecting face with %d vertices", ac);
		error(WARNING, errmsg);
	}
	polyFree(poly);
	return(1);
}


static OBJECT
getmod(void)				/* get current modifier ID */
{
	char	*mnam;
	OBJECT	mod;

	if (!havemats)
		return(OVOID);
	if (!strcmp(material, VOIDID))
		return(OVOID);
	if (material[0])		/* prefer usemtl statements */
		mnam = material;
	else if (group[0])		/* else use group name */
		mnam = group;
	else
		return(OVOID);
	mod = modifier(mnam);
	if (mod == OVOID) {
		sprintf(errmsg, "%s: undefined modifier \"%s\"",
				inpfile, mnam);
		error(USER, errmsg);
	}
	return(mod);
}


static int
puttri(			/* convert a triangle */
	char	*v1,
	char	*v2,
	char	*v3
)
{
	VNDX	v1i, v2i, v3i;
	RREAL	*v1c, *v2c, *v3c;
	RREAL	*v1n, *v2n, *v3n;
	
	if (!cvtndx(v1i, v1) || !cvtndx(v2i, v2) || !cvtndx(v3i, v3)) {
		error(WARNING, "bad vertex reference");
		return(0);
	}
	if (v1i[1]>=0 && v2i[1]>=0 && v3i[1]>=0) {
		v1c = vtlist[v1i[1]];
		v2c = vtlist[v2i[1]];
		v3c = vtlist[v3i[1]];
	} else
		v1c = v2c = v3c = NULL;

	if (v1i[2]>=0 && v2i[2]>=0 && v3i[2]>=0) {
		v1n = vnlist[v1i[2]];
		v2n = vnlist[v2i[2]];
		v3n = vnlist[v3i[2]];
	} else
		v1n = v2n = v3n = NULL;
	
	return(cvtri(getmod(), vlist[v1i[0]], vlist[v2i[0]], vlist[v3i[0]],
			v1n, v2n, v3n, v1c, v2c, v3c) >= 0);
}


static void
freeverts(void)			/* free all vertices */
{
	if (nvs) {
		free((void *)vlist);
		nvs = 0;
	}
	if (nvts) {
		free((void *)vtlist);
		nvts = 0;
	}
	if (nvns) {
		free((void *)vnlist);
		nvns = 0;
	}
}


static int
newv(			/* create a new vertex */
	double	x,
	double	y,
	double	z
)
{
	if (!(nvs%CHUNKSIZ)) {		/* allocate next block */
		if (nvs == 0)
			vlist = (FVECT *)malloc(CHUNKSIZ*sizeof(FVECT));
		else
			vlist = (FVECT *)realloc((void *)vlist,
					(nvs+CHUNKSIZ)*sizeof(FVECT));
		if (vlist == NULL)
			error(SYSTEM, "out of memory in newv");
	}
					/* assign new vertex */
	vlist[nvs][0] = x;
	vlist[nvs][1] = y;
	vlist[nvs][2] = z;
	return(++nvs);
}


static int
newvn(			/* create a new vertex normal */
	double	x,
	double	y,
	double	z
)
{
	if (!(nvns%CHUNKSIZ)) {		/* allocate next block */
		if (nvns == 0)
			vnlist = (FVECT *)malloc(CHUNKSIZ*sizeof(FVECT));
		else
			vnlist = (FVECT *)realloc((void *)vnlist,
					(nvns+CHUNKSIZ)*sizeof(FVECT));
		if (vnlist == NULL)
			error(SYSTEM, "out of memory in newvn");
	}
					/* assign new normal */
	vnlist[nvns][0] = x;
	vnlist[nvns][1] = y;
	vnlist[nvns][2] = z;
	if (normalize(vnlist[nvns]) == 0.0)
		return(0);
	return(++nvns);
}


static int
newvt(			/* create a new texture map vertex */
	double	x,
	double	y
)
{
	if (!(nvts%CHUNKSIZ)) {		/* allocate next block */
		if (nvts == 0)
			vtlist = (RREAL (*)[2])malloc(CHUNKSIZ*2*sizeof(RREAL));
		else
			vtlist = (RREAL (*)[2])realloc((void *)vtlist,
					(nvts+CHUNKSIZ)*2*sizeof(RREAL));
		if (vtlist == NULL)
			error(SYSTEM, "out of memory in newvt");
	}
					/* assign new vertex */
	vtlist[nvts][0] = x;
	vtlist[nvts][1] = y;
	return(++nvts);
}


static void
syntax(			/* report syntax error and exit */
	char	*er
)
{
	sprintf(errmsg, "%s: Wavefront syntax error near line %d: %s\n",
			inpfile, lineno, er);
	error(USER, errmsg);
}
