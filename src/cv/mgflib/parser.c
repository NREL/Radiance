/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Parse an MGF file, converting or discarding unsupported entities
 */

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "parser.h"
#include "lookup.h"
#include "messages.h"

/*
 * Global definitions of variables declared in parser.h
 */
			/* entity names */

char	mg_ename[MG_NENTITIES][MG_MAXELEN] = MG_NAMELIST;

			/* Handler routines for each entity */

int	(*mg_ehand[MG_NENTITIES])();

			/* error messages */

char	*mg_err[MG_NERRS] = MG_ERRLIST;

MG_FCTXT	*mg_file;	/* current file context pointer */

int	mg_nqcdivs = MG_NQCD;	/* number of divisions per quarter circle */

/*
 * The idea with this parser is to compensate for any missing entries in
 * mg_ehand with alternate handlers that express these entities in terms
 * of others that the calling program can handle.
 * 
 * In some cases, no alternate handler is possible because the entity
 * has no approximate equivalent.  These entities are simply discarded.
 *
 * Certain entities are dependent on others, and mg_init() will fail
 * if the supported entities are not consistent.
 *
 * Some alternate entity handlers require that earlier entities be
 * noted in some fashion, and we therefore keep another array of
 * parallel support handlers to assist in this effort.
 */

/* temporary settings for testing */
#define e_ies e_any_toss
#define e_cmix e_any_toss
#define e_cspec e_any_toss
				/* alternate handler routines */

static int	e_any_toss(),		/* discard unneeded entity */
		e_ies(),		/* IES luminaire file */
		e_include(),		/* include file */
		e_sph(),		/* sphere */
		e_cmix(),		/* color mixtures */
		e_cspec();		/* color spectra */
		e_cyl(),		/* cylinder */
		e_cone(),		/* cone */
		e_prism(),		/* prism */
		e_ring(),		/* ring */
		e_torus();		/* torus */

				/* alternate handler support functions */

static int	(*e_supp[MG_NENTITIES])();

static char	FLTFMT[] = "%.12g";

static int	warpconends;		/* hack for generating good normals */


void
mg_init()			/* initialize alternate entity handlers */
{
	unsigned long	ineed = 0, uneed = 0;
	register int	i;
					/* pick up slack */
	if (mg_ehand[MG_E_IES] == NULL)
		mg_ehand[MG_E_IES] = e_ies;
	if (mg_ehand[MG_E_INCLUDE] == NULL)
		mg_ehand[MG_E_INCLUDE] = e_include;
	if (mg_ehand[MG_E_SPH] == NULL) {
		mg_ehand[MG_E_SPH] = e_sph;
		ineed |= 1<<MG_E_POINT|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_CYL] == NULL) {
		mg_ehand[MG_E_CYL] = e_cyl;
		ineed |= 1<<MG_E_POINT|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_CONE] == NULL) {
		mg_ehand[MG_E_CONE] = e_cone;
		ineed |= 1<<MG_E_POINT|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_RING] == NULL) {
		mg_ehand[MG_E_RING] = e_ring;
		ineed |= 1<<MG_E_POINT|1<<MG_E_NORMAL|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_NORMAL|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_PRISM] == NULL) {
		mg_ehand[MG_E_PRISM] = e_prism;
		ineed |= 1<<MG_E_POINT|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_TORUS] == NULL) {
		mg_ehand[MG_E_TORUS] = e_torus;
		ineed |= 1<<MG_E_POINT|1<<MG_E_NORMAL|1<<MG_E_VERTEX;
	} else
		uneed |= 1<<MG_E_POINT|1<<MG_E_NORMAL|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_COLOR] != NULL) {
		if (mg_ehand[MG_E_CMIX] == NULL)
			mg_ehand[MG_E_CMIX] = e_cmix;
		if (mg_ehand[MG_E_CSPEC] == NULL)
			mg_ehand[MG_E_CSPEC] = e_cspec;
	}
					/* check for consistency */
	if (mg_ehand[MG_E_FACE] != NULL)
		uneed |= 1<<MG_E_POINT|1<<MG_E_VERTEX|1<<MG_E_XF;
	if (mg_ehand[MG_E_CXY] != NULL)
		uneed |= 1<<MG_E_COLOR;
	if (mg_ehand[MG_E_RD] != NULL || mg_ehand[MG_E_TD] != NULL ||
			mg_ehand[MG_E_ED] != NULL || 
			mg_ehand[MG_E_RS] != NULL ||
			mg_ehand[MG_E_TS] != NULL)
		uneed |= 1<<MG_E_MATERIAL;
	for (i = 0; i < MG_NENTITIES; i++)
		if (uneed & 1<<i && mg_ehand[i] == NULL) {
			fprintf(stderr, "Missing support for \"%s\" entity\n",
					mg_ename[i]);
			exit(1);
		}
					/* add support as needed */
	if (ineed & 1<<MG_E_VERTEX && mg_ehand[MG_E_VERTEX] != c_hvertex)
		e_supp[MG_E_VERTEX] = c_hvertex;
	if (ineed & 1<<MG_E_POINT && mg_ehand[MG_E_POINT] != c_hvertex)
		e_supp[MG_E_POINT] = c_hvertex;
	if (ineed & 1<<MG_E_NORMAL && mg_ehand[MG_E_NORMAL] != c_hvertex)
		e_supp[MG_E_NORMAL] = c_hvertex;
					/* discard remaining entities */
	for (i = 0; i < MG_NENTITIES; i++)
		if (mg_ehand[i] == NULL)
			mg_ehand[i] = e_any_toss;
}



int
mg_entity(name)			/* get entity number from its name */
char	*name;
{
	static LUTAB	ent_tab;	/* entity lookup table */
	register char	*cp;

	if (!ent_tab.tsiz) {		/* initialize hash table */
		if (!lu_init(&ent_tab, MG_NENTITIES))
			return(-1);		/* what to do? */
		for (cp = mg_ename[MG_NENTITIES-1]; cp >= mg_ename[0];
				cp -= sizeof(mg_ename[0]))
			lu_find(&ent_tab, cp)->key = cp;
	}
	cp = lu_find(&ent_tab, name)->key;
	if (cp == NULL)
		return(-1);
	return((cp - mg_ename[0])/sizeof(mg_ename[0]));
}


static int
handle_it(en, ac, av)		/* pass entity to appropriate handler */
register int	en;
int	ac;
char	**av;
{
	int	rv;

	if (en < 0 && (en = mg_entity(av[0])) < 0)
		return(MG_EUNK);
	if (e_supp[en] != NULL) {
		if ((rv = (*e_supp[en])(ac, av)) != MG_OK)
			return(rv);
	}
	return((*mg_ehand[en])(ac, av));
}


int
mg_open(ctx, fn)			/* open new input file */
register MG_FCTXT	*ctx;
char	*fn;
{
	int	olen;
	register char	*cp;

	ctx->lineno = 0;
	if (fn == NULL) {
		ctx->fname = "<stdin>";
		ctx->fp = stdin;
		ctx->prev = mg_file;
		mg_file = ctx;
		return(MG_OK);
	}
					/* get name relative to this context */
	if (mg_file != NULL &&
			(cp = strrchr(mg_file->fname, '/')) != NULL)
		olen = cp - mg_file->fname + 1;
	else
		olen = 0;
	ctx->fname = (char *)malloc(olen+strlen(fn)+1);
	if (ctx->fname == NULL)
		return(MG_EMEM);
	if (olen)
		strcpy(ctx->fname, mg_file->fname);
	strcpy(ctx->fname+olen, fn);
	ctx->fp = fopen(ctx->fname, "r");
	if (ctx->fp == NULL) {
		free((MEM_PTR)ctx->fname);
		return(MG_ENOFILE);
	}
	ctx->prev = mg_file;		/* establish new context */
	mg_file = ctx;
	return(MG_OK);
}


void
mg_close()			/* close input file */
{
	register MG_FCTXT	*ctx = mg_file;

	mg_file = ctx->prev;		/* restore enclosing context */
	if (ctx->fp == stdin)
		return;			/* don't close standard input */
	fclose(ctx->fp);
	free((MEM_PTR)ctx->fname);
}


int
mg_rewind()			/* rewind input file */
{
	if (mg_file->lineno == 0)
		return(MG_OK);
	if (mg_file->fp == stdin)
		return(MG_ESEEK);	/* cannot seek on standard input */
	if (fseek(mg_file->fp, 0L, 0) == EOF)
		return(MG_ESEEK);
	mg_file->lineno = 0;
	return(MG_OK);
}


int
mg_read()			/* read next line from file */
{
	register int	len = 0;

	do {
		if (fgets(mg_file->inpline+len,
				MG_MAXLINE-len, mg_file->fp) == NULL)
			return(len);
		mg_file->lineno++;
		len += strlen(mg_file->inpline+len);
		if (len > 1 && mg_file->inpline[len-2] == '\\')
			mg_file->inpline[--len-1] = ' ';
	} while (mg_file->inpline[len]);

	return(len);
}


int
mg_parse()			/* parse current input line */
{
	char	abuf[MG_MAXLINE];
	char	*argv[MG_MAXARGC];
	int	en;
	register char	*cp, **ap;

	strcpy(cp=abuf, mg_file->inpline);
	ap = argv;			/* break into words */
	for ( ; ; ) {
		while (isspace(*cp))
			*cp++ = '\0';
		if (!*cp)
			break;
		if (ap - argv >= MG_MAXARGC-1)
			return(MG_EARGC);
		*ap++ = cp;
		while (*++cp && !isspace(*cp))
			;
	}
	if (ap == argv)
		return(MG_OK);		/* no words in line */
	*ap = NULL;
					/* else handle it */
	return(handle_it(-1, ap-argv, argv));
}


int
mg_load(fn)			/* load an MGF file */
char	*fn;
{
	MG_FCTXT	cntxt;
	int	rval;

	if ((rval = mg_open(&cntxt, fn)) != MG_OK) {
		fprintf(stderr, "%s: %s\n", fn, mg_err[rval]);
		return(rval);
	}
	while (mg_read())		/* parse each line */
		if ((rval = mg_parse()) != MG_OK) {
			fprintf(stderr, "%s: %d: %s:\n%s", cntxt.fname,
					cntxt.lineno, mg_err[rval],
					cntxt.inpline);
			break;
		}
	mg_close();
	return(rval);
}


void
mg_clear()			/* clear parser history */
{
	c_clearall();			/* clear context tables */
	mg_file = NULL;			/* reset our context */
}


int
mg_iterate(ac, av, f)		/* iterate on statement */
int	ac;
register char	**av;
int	(*f)();
{
	int	niter, rval;
	register int	i, j;
	char	*argv[MG_MAXARGC];
	char	cntbuf[10];
					/* build partial transformation */
	for (i = 0; i < ac; i++) {
		if (av[i][0] == '-' && av[i][1] == 'a' && av[i][2] == '\0')
			break;
		argv[i+1] = av[i];
	}
	argv[i+1] = NULL;
	if (i) {			/* handle transformation */
		argv[0] = mg_ename[MG_E_XF];
		if ((rval = handle_it(MG_E_XF, i+1, argv)) != MG_OK)
			return(rval);
	}
	if (i < ac) {			/* run array */
		if (i+1 >= ac || !isint(av[i+1]))
			return(MG_ETYPE);
		niter = atoi(av[i+1]);
		argv[0] = mg_ename[MG_E_OBJECT];
		argv[1] = cntbuf;
		for (j = 2; j+i < ac; j++)
			argv[j] = av[j+i];
		argv[j] = NULL;
		for (j = 0; j < niter; j++) {
			sprintf(cntbuf, "%d", j);
			if ((rval = handle_it(MG_E_OBJECT, 2, argv)) != MG_OK)
				return(rval);
			argv[0] = "-i";
			if ((rval = mg_iterate(ac-i, argv, f)) != MG_OK)
				return(rval);
			argv[0] = mg_ename[MG_E_OBJECT];
			if ((rval = handle_it(MG_E_OBJECT, 1, argv)) != MG_OK)
				return(rval);
		}
	} else if ((rval = (*f)()) != MG_OK)	/* else do this instance */
			return(rval);
	if (i) {			/* reset the transform */
		argv[0] = mg_ename[MG_E_XF];
		argv[1] = NULL;
		(void)handle_it(MG_E_XF, 1, argv);
	}
	return(MG_OK);
}


/****************************************************************************
 *	The following routines handle unsupported entities
 */


static int
e_any_toss(ac, av)		/* discard an unwanted entity */
int	ac;
char	**av;
{
	return(MG_OK);
}


static int
reload_file()			/* reload current MGF file */
{
	register int	rval;

	if ((rval = mg_rewind()) != MG_OK)
		return(rval);
	while (mg_read())
		if ((rval = mg_parse()) != MG_OK)
			return(rval);
	return(MG_OK);
}


static int
e_include(ac, av)		/* include file */
int	ac;
char	**av;
{
	MG_FCTXT	ictx;
	int	rv;

	if (ac < 2)
		return(MG_EARGC);
	if ((rv = mg_open(&ictx, av[1])) != MG_OK)
		return(rv);
	if ((rv = mg_iterate(ac-2, av+2, reload_file)) != MG_OK) {
		fprintf(stderr, "%s: %d: %s:\n%s", ictx.fname,
				ictx.lineno, mg_err[rv], ictx.inpline);
		mg_close();
		return(MG_EINCL);
	}
	mg_close();
	return(MG_OK);
}


static void
make_axes(u, v, w)		/* compute u and v given w (normalized) */
FVECT	u, v, w;
{
	register int	i;

	v[0] = v[1] = v[2] = 0.;
	for (i = 0; i < 3; i++)
		if (w[i] < .6 && w[i] > -.6)
			break;
	v[i] = 1.;
	fcross(u, v, w);
	normalize(u);
	fcross(v, w, u);
}


static int
e_sph(ac, av)			/* expand a sphere into cones */
int	ac;
char	**av;
{
	static char	p2x[24], p2y[24], p2z[24], r1[24], r2[24];
	static char	*v1ent[5] = {mg_ename[MG_E_VERTEX],"_sv1","=","_sv2"};
	static char	*v2ent[4] = {mg_ename[MG_E_VERTEX],"_sv2","="};
	static char	*p2ent[5] = {mg_ename[MG_E_POINT],p2x,p2y,p2z};
	static char	*conent[6] = {mg_ename[MG_E_CONE],"_sv1",r1,"_sv2",r2};
	register C_VERTEX	*cv;
	register int	i;
	int	rval;
	double	rad;
	double	theta;

	if (ac != 3)
		return(MG_EARGC);
	if ((cv = c_getvert(av[1])) == NULL)
		return(MG_EUNDEF);
	if (!isflt(av[2]))
		return(MG_ETYPE);
	rad = atof(av[2]);
					/* initialize */
	warpconends = 1;
	if ((rval = handle_it(MG_E_VERTEX, 3, v2ent)) != MG_OK)
		return(rval);
	sprintf(p2x, FLTFMT, cv->p[0]);
	sprintf(p2y, FLTFMT, cv->p[1]);
	sprintf(p2z, FLTFMT, cv->p[2]+rad);
	if ((rval = handle_it(MG_E_POINT, 4, p2ent)) != MG_OK)
		return(rval);
	r2[0] = '0'; r2[1] = '\0';
	for (i = 1; i <= 2*mg_nqcdivs; i++) {
		theta = i*(PI/2)/mg_nqcdivs;
		if ((rval = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
			return(rval);
		sprintf(p2z, FLTFMT, cv->p[2]+rad*cos(theta));
		if ((rval = handle_it(MG_E_VERTEX, 2, v2ent)) != MG_OK)
			return(rval);
		if ((rval = handle_it(MG_E_POINT, 4, p2ent)) != MG_OK)
			return(rval);
		strcpy(r1, r2);
		sprintf(r2, FLTFMT, rad*sin(theta));
		if ((rval = handle_it(MG_E_CONE, 5, conent)) != MG_OK)
			return(rval);
	}
	warpconends = 0;
	return(MG_OK);
}


static int
e_torus(ac, av)			/* expand a torus into cones */
int	ac;
char	**av;
{
	static char	p2[3][24], r1[24], r2[24];
	static char	*v1ent[5] = {mg_ename[MG_E_VERTEX],"_tv1","=","_tv2"};
	static char	*v2ent[5] = {mg_ename[MG_E_VERTEX],"_tv2","="};
	static char	*p2ent[5] = {mg_ename[MG_E_POINT],p2[0],p2[1],p2[2]};
	static char	*conent[6] = {mg_ename[MG_E_CONE],"_tv1",r1,"_tv2",r2};
	register C_VERTEX	*cv;
	register int	i, j;
	int	rval;
	int	sgn;
	double	minrad, maxrad, avgrad;
	double	theta;

	if (ac != 4)
		return(MG_EARGC);
	if ((cv = c_getvert(av[1])) == NULL)
		return(MG_EUNDEF);
	if (is0vect(cv->n))
		return(MG_EILL);
	if (!isflt(av[2]) || !isflt(av[3]))
		return(MG_ETYPE);
	minrad = atof(av[2]);
	round0(minrad);
	maxrad = atof(av[3]);
					/* check orientation */
	if (minrad > 0.)
		sgn = 1;
	else if (minrad < 0.)
		sgn = -1;
	else if (maxrad > 0.)
		sgn = 1;
	else if (maxrad < 0.)
		sgn = -1;
	else
		return(MG_EILL);
	if (sgn*(maxrad-minrad) <= 0.)
		return(MG_EILL);
					/* initialize */
	warpconends = 1;
	v2ent[3] = av[1];
	for (j = 0; j < 3; j++)
		sprintf(p2[j], FLTFMT, cv->p[j] +
				.5*sgn*(maxrad-minrad)*cv->n[j]);
	if ((rval = handle_it(MG_E_VERTEX, 4, v2ent)) != MG_OK)
		return(rval);
	if ((rval = handle_it(MG_E_POINT, 4, p2ent)) != MG_OK)
		return(rval);
	sprintf(r2, FLTFMT, avgrad=.5*(minrad+maxrad));
					/* run outer section */
	for (i = 1; i <= 2*mg_nqcdivs; i++) {
		theta = i*(PI/2)/mg_nqcdivs;
		if ((rval = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
			return(rval);
		for (j = 0; j < 3; j++)
			sprintf(p2[j], FLTFMT, cv->p[j] +
				.5*sgn*(maxrad-minrad)*cos(theta)*cv->n[j]);
		if ((rval = handle_it(MG_E_VERTEX, 2, v2ent)) != MG_OK)
			return(rval);
		if ((rval = handle_it(MG_E_POINT, 4, p2ent)) != MG_OK)
			return(rval);
		strcpy(r1, r2);
		sprintf(r2, FLTFMT, avgrad + .5*(maxrad-minrad)*sin(theta));
		if ((rval = handle_it(MG_E_CONE, 5, conent)) != MG_OK)
			return(rval);
	}
					/* run inner section */
	sprintf(r2, FLTFMT, -.5*(minrad+maxrad));
	for ( ; i <= 4*mg_nqcdivs; i++) {
		theta = i*(PI/2)/mg_nqcdivs;
		for (j = 0; j < 3; j++)
			sprintf(p2[j], FLTFMT, cv->p[j] +
				.5*sgn*(maxrad-minrad)*cos(theta)*cv->n[j]);
		if ((rval = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
			return(rval);
		if ((rval = handle_it(MG_E_VERTEX, 2, v2ent)) != MG_OK)
			return(rval);
		if ((rval = handle_it(MG_E_POINT, 4, p2ent)) != MG_OK)
			return(rval);
		strcpy(r1, r2);
		sprintf(r2, FLTFMT, -avgrad - .5*(maxrad-minrad)*sin(theta));
		if ((rval = handle_it(MG_E_CONE, 5, conent)) != MG_OK)
			return(rval);
	}
	warpconends = 0;
	return(MG_OK);
}


static int
e_cyl(ac, av)			/* replace a cylinder with equivalent cone */
int	ac;
char	**av;
{
	static char	*avnew[6] = {mg_ename[MG_E_CONE]};

	if (ac != 4)
		return(MG_EARGC);
	avnew[1] = av[1];
	avnew[2] = av[2];
	avnew[3] = av[3];
	avnew[4] = av[2];
	return(handle_it(MG_E_CONE, 5, avnew));
}


static int
e_ring(ac, av)			/* turn a ring into polygons */
int	ac;
char	**av;
{
	static char	p3[3][24], p4[3][24];
	static char	*nzent[5] = {mg_ename[MG_E_NORMAL],"0","0","0"};
	static char	*v1ent[5] = {mg_ename[MG_E_VERTEX],"_rv1","="};
	static char	*v2ent[5] = {mg_ename[MG_E_VERTEX],"_rv2","=","_rv3"};
	static char	*v3ent[4] = {mg_ename[MG_E_VERTEX],"_rv3","="};
	static char	*p3ent[5] = {mg_ename[MG_E_POINT],p3[0],p3[1],p3[2]};
	static char	*v4ent[4] = {mg_ename[MG_E_VERTEX],"_rv4","="};
	static char	*p4ent[5] = {mg_ename[MG_E_POINT],p4[0],p4[1],p4[2]};
	static char	*fent[6] = {mg_ename[MG_E_FACE],"_rv1","_rv2","_rv3","_rv4"};
	register C_VERTEX	*cv;
	register int	i, j;
	FVECT	u, v;
	double	minrad, maxrad;
	int	rv;
	double	theta, d;

	if (ac != 4)
		return(MG_EARGC);
	if ((cv = c_getvert(av[1])) == NULL)
		return(MG_EUNDEF);
	if (is0vect(cv->n))
		return(MG_EILL);
	if (!isflt(av[2]) || !isflt(av[3]))
		return(MG_ETYPE);
	minrad = atof(av[2]);
	round0(minrad);
	maxrad = atof(av[3]);
	if (minrad < 0. || maxrad <= minrad)
		return(MG_EILL);
					/* initialize */
	make_axes(u, v, cv->n);
	for (j = 0; j < 3; j++)
		sprintf(p3[j], FLTFMT, cv->p[j] + maxrad*u[j]);
	if ((rv = handle_it(MG_E_VERTEX, 3, v3ent)) != MG_OK)
		return(rv);
	if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
		return(rv);
	if (minrad == 0.) {		/* closed */
		v1ent[3] = av[1];
		if ((rv = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
			return(rv);
		if ((rv = handle_it(MG_E_NORMAL, 4, nzent)) != MG_OK)
			return(rv);
		for (i = 1; i <= 4*mg_nqcdivs; i++) {
			theta = i*(PI/2)/mg_nqcdivs;
			if ((rv = handle_it(MG_E_VERTEX, 4, v2ent)) != MG_OK)
				return(rv);
			for (j = 0; j < 3; j++)
				sprintf(p3[j], FLTFMT, cv->p[j] +
						maxrad*u[j]*cos(theta) +
						maxrad*v[j]*sin(theta));
			if ((rv = handle_it(MG_E_VERTEX, 2, v3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_FACE, 4, fent)) != MG_OK)
				return(rv);
		}
	} else {			/* open */
		if ((rv = handle_it(MG_E_VERTEX, 3, v4ent)) != MG_OK)
			return(rv);
		for (j = 0; j < 3; j++)
			sprintf(p4[j], FLTFMT, cv->p[j] + minrad*u[j]);
		if ((rv = handle_it(MG_E_POINT, 4, p4ent)) != MG_OK)
			return(rv);
		v1ent[3] = "_rv4";
		for (i = 1; i <= 4*mg_nqcdivs; i++) {
			theta = i*(PI/2)/mg_nqcdivs;
			if ((rv = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_VERTEX, 4, v2ent)) != MG_OK)
				return(rv);
			for (j = 0; j < 3; j++) {
				d = u[j]*cos(theta) + v[j]*sin(theta);
				sprintf(p3[j], FLTFMT, cv->p[j] + maxrad*d);
				sprintf(p4[j], FLTFMT, cv->p[j] + minrad*d);
			}
			if ((rv = handle_it(MG_E_VERTEX, 2, v3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_VERTEX, 2, v4ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p4ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_FACE, 5, fent)) != MG_OK)
				return(rv);
		}
	}
	return(MG_OK);
}


static int
e_cone(ac, av)			/* turn a cone into polygons */
int	ac;
char	**av;
{
	static char	p3[3][24], p4[3][24], n3[3][24], n4[3][24];
	static char	*v1ent[5] = {mg_ename[MG_E_VERTEX],"_cv1","="};
	static char	*v2ent[5] = {mg_ename[MG_E_VERTEX],"_cv2","=","_cv3"};
	static char	*v3ent[4] = {mg_ename[MG_E_VERTEX],"_cv3","="};
	static char	*p3ent[5] = {mg_ename[MG_E_POINT],p3[0],p3[1],p3[2]};
	static char	*n3ent[5] = {mg_ename[MG_E_NORMAL],n3[0],n3[1],n3[2]};
	static char	*v4ent[4] = {mg_ename[MG_E_VERTEX],"_cv4","="};
	static char	*p4ent[5] = {mg_ename[MG_E_POINT],p4[0],p4[1],p4[2]};
	static char	*n4ent[5] = {mg_ename[MG_E_NORMAL],n4[0],n4[1],n4[2]};
	static char	*fent[6] = {mg_ename[MG_E_FACE],"_cv1","_cv2","_cv3","_cv4"};
	register C_VERTEX	*cv1, *cv2;
	register int	i, j;
	FVECT	u, v, w;
	double	rad1, rad2;
	int	sgn;
	double	n1off, n2off;
	double	d;
	int	rv;
	double	theta;

	if (ac != 5)
		return(MG_EARGC);
	if ((cv1 = c_getvert(av[1])) == NULL ||
			(cv2 = c_getvert(av[3])) == NULL)
		return(MG_EUNDEF);
	if (!isflt(av[2]) || !isflt(av[4]))
		return(MG_ETYPE);
	rad1 = atof(av[2]);
	round0(rad1);
	rad2 = atof(av[4]);
	round0(rad2);
	if (rad1 == 0.) {
		if (rad2 == 0.)
			return(MG_EILL);
	} else if (rad2 != 0.) {
		if (rad1 < 0. ^ rad2 < 0.)
			return(MG_EILL);
	} else {			/* swap */
		C_VERTEX	*cv;

		cv = cv1;
		cv1 = cv2;
		cv2 = cv;
		d = rad1;
		rad1 = rad2;
		rad2 = d;
	}
	sgn = rad2 < 0. ? -1 : 1;
					/* initialize */
	for (j = 0; j < 3; j++)
		w[j] = cv1->p[j] - cv2->p[j];
	if ((d = normalize(w)) == 0.)
		return(MG_EILL);
	n1off = n2off = (rad2 - rad1)/d;
	if (warpconends) {		/* hack for e_sph and e_torus */
		d = atan(n2off) - (PI/4)/mg_nqcdivs;
		if (d <= -PI/2+FTINY)
			n2off = -FHUGE;
		else
			n2off = tan(d);
	}
	make_axes(u, v, w);
	for (j = 0; j < 3; j++) {
		sprintf(p3[j], FLTFMT, cv2->p[j] + rad2*u[j]);
		if (n2off <= -FHUGE)
			sprintf(n3[j], FLTFMT, -w[j]);
		else
			sprintf(n3[j], FLTFMT, u[j] + w[j]*n2off);
	}
	if ((rv = handle_it(MG_E_VERTEX, 3, v3ent)) != MG_OK)
		return(rv);
	if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
		return(rv);
	if ((rv = handle_it(MG_E_NORMAL, 4, n3ent)) != MG_OK)
		return(rv);
	if (rad1 == 0.) {		/* triangles */
		v1ent[3] = av[1];
		if ((rv = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
			return(rv);
		for (j = 0; j < 3; j++)
			sprintf(n4[j], FLTFMT, w[j]);
		if ((rv = handle_it(MG_E_NORMAL, 4, n4ent)) != MG_OK)
			return(rv);
		for (i = 1; i <= 4*mg_nqcdivs; i++) {
			theta = sgn*i*(PI/2)/mg_nqcdivs;
			if ((rv = handle_it(MG_E_VERTEX, 4, v2ent)) != MG_OK)
				return(rv);
			for (j = 0; j < 3; j++) {
				d = u[j]*cos(theta) + v[j]*sin(theta);
				sprintf(p3[j], FLTFMT, cv2->p[j] + rad2*d);
				if (n2off > -FHUGE)
					sprintf(n3[j], FLTFMT, d + w[j]*n2off);
			}
			if ((rv = handle_it(MG_E_VERTEX, 2, v3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
				return(rv);
			if (n2off > -FHUGE &&
			(rv = handle_it(MG_E_NORMAL, 4, n3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_FACE, 4, fent)) != MG_OK)
				return(rv);
		}
	} else {			/* quads */
		v1ent[3] = "_cv4";
		if (warpconends) {		/* hack for e_sph and e_torus */
			d = atan(n1off) + (PI/4)/mg_nqcdivs;
			if (d >= PI/2-FTINY)
				n1off = FHUGE;
			else
				n1off = tan(atan(n1off)+(PI/4)/mg_nqcdivs);
		}
		for (j = 0; j < 3; j++) {
			sprintf(p4[j], FLTFMT, cv1->p[j] + rad1*u[j]);
			if (n1off >= FHUGE)
				sprintf(n4[j], FLTFMT, w[j]);
			else
				sprintf(n4[j], FLTFMT, u[j] + w[j]*n1off);
		}
		if ((rv = handle_it(MG_E_VERTEX, 3, v4ent)) != MG_OK)
			return(rv);
		if ((rv = handle_it(MG_E_POINT, 4, p4ent)) != MG_OK)
			return(rv);
		if ((rv = handle_it(MG_E_NORMAL, 4, n4ent)) != MG_OK)
			return(rv);
		for (i = 1; i <= 4*mg_nqcdivs; i++) {
			theta = sgn*i*(PI/2)/mg_nqcdivs;
			if ((rv = handle_it(MG_E_VERTEX, 4, v1ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_VERTEX, 4, v2ent)) != MG_OK)
				return(rv);
			for (j = 0; j < 3; j++) {
				d = u[j]*cos(theta) + v[j]*sin(theta);
				sprintf(p3[j], FLTFMT, cv2->p[j] + rad2*d);
				if (n2off > -FHUGE)
					sprintf(n3[j], FLTFMT, d + w[j]*n2off);
				sprintf(p4[j], FLTFMT, cv1->p[j] + rad1*d);
				if (n1off < FHUGE)
					sprintf(n4[j], FLTFMT, d + w[j]*n1off);
			}
			if ((rv = handle_it(MG_E_VERTEX, 2, v3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p3ent)) != MG_OK)
				return(rv);
			if (n2off > -FHUGE &&
			(rv = handle_it(MG_E_NORMAL, 4, n3ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_VERTEX, 2, v4ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_POINT, 4, p4ent)) != MG_OK)
				return(rv);
			if (n1off < FHUGE &&
			(rv = handle_it(MG_E_NORMAL, 4, n4ent)) != MG_OK)
				return(rv);
			if ((rv = handle_it(MG_E_FACE, 5, fent)) != MG_OK)
				return(rv);
		}
	}
	return(MG_OK);
}


static int
e_prism(ac, av)			/* turn a prism into polygons */
int	ac;
char	**av;
{
	static char	p[3][24];
	static char	*vent[4] = {mg_ename[MG_E_VERTEX],NULL,"="};
	static char	*pent[5] = {mg_ename[MG_E_POINT],p[0],p[1],p[2]};
	char	*newav[MG_MAXARGC], nvn[MG_MAXARGC-1][8];
	double	length;
	FVECT	v1, v2, v3, norm;
	register C_VERTEX	*cv;
	C_VERTEX	*cv0;
	int	rv;
	register int	i, j;

	if (ac < 5)
		return(MG_EARGC);
	if (!isflt(av[1]))
		return(MG_ETYPE);
	length = atof(av[1]);
	if (length <= FTINY && length >= -FTINY)
		return(MG_EILL);
					/* do bottom face */
	newav[0] = mg_ename[MG_E_FACE];
	for (i = 1; i < ac-1; i++)
		newav[i] = av[i+1];
	newav[i] = NULL;
	if ((rv = handle_it(MG_E_FACE, i, newav)) != MG_OK)
		return(rv);
					/* compute face normal */
	if ((cv0 = c_getvert(av[2])) == NULL)
		return(MG_EUNDEF);
	norm[0] = norm[1] = norm[2] = 0.;
	v1[0] = v1[1] = v1[2] = 0.;
	for (i = 2; i < ac-1; i++) {
		if ((cv = c_getvert(av[i+1])) == NULL)
			return(MG_EUNDEF);
		v2[0] = cv->p[0] - cv0->p[0];
		v2[1] = cv->p[1] - cv0->p[1];
		v2[2] = cv->p[2] - cv0->p[2];
		fcross(v3, v1, v2);
		norm[0] += v3[0];
		norm[1] += v3[1];
		norm[2] += v3[2];
		VCOPY(v1, v2);
	}
	if (normalize(norm) == 0.)
		return(MG_EILL);
					/* create moved vertices */
	for (i = 1; i < ac-1; i++) {
		sprintf(nvn[i-1], "_pv%d", i);
		vent[1] = nvn[i-1];
		if ((rv = handle_it(MG_E_VERTEX, 3, vent)) != MG_OK)
			return(rv);
		cv = c_getvert(av[i+1]);	/* checked above */
		for (j = 0; j < 3; j++)
			sprintf(p[j], FLTFMT, cv->p[j] - length*norm[j]);
		if ((rv = handle_it(MG_E_POINT, 4, pent)) != MG_OK)
			return(rv);
		newav[ac-1-i] = nvn[i-1];	/* reverse */
	}
						/* do top face */
	if ((rv = handle_it(MG_E_FACE, ac-1, newav)) != MG_OK)
		return(rv);
						/* do the side faces */
	newav[5] = NULL;
	newav[3] = av[ac-1];
	newav[4] = nvn[ac-3];
	for (i = 1; i < ac-1; i++) {
		newav[1] = nvn[i-1];
		newav[2] = av[i+1];
		if ((rv = handle_it(MG_E_FACE, 5, newav)) != MG_OK)
			return(rv);
		newav[3] = newav[2];
		newav[4] = newav[1];
	}
	return(MG_OK);
}
