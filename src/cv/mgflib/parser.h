/* Copyright (c) 1994 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for MGF interpreter
 */

/* must include stdio.h before us */

			/* Entities (order doesn't really matter) */
#define MG_E_COMMENT	0
#define MG_E_COLOR	1
#define MG_E_CONE	2
#define MG_E_CXY	3
#define MG_E_CYL	4
#define MG_E_ED		5
#define MG_E_FACE	6
#define MG_E_INCLUDE	7
#define MG_E_IES	8
#define MG_E_MATERIAL	9
#define MG_E_NORMAL	10
#define MG_E_OBJECT	11
#define MG_E_POINT	12
#define MG_E_RD		13
#define MG_E_RING	14
#define MG_E_RS		15
#define MG_E_SPH	16
#define MG_E_TD		17
#define MG_E_TORUS	18
#define MG_E_TS		19
#define MG_E_VERTEX	20
#define MG_E_XF		21

#define MG_NENTITIES	22

#define MG_NAMELIST	{"#","c","cone","cxy","cyl","ed","f","i","ies",\
	"m","n","o","p","rd","ring","rs","sph","td","torus","ts","v","xf"}

#define MG_MAXELEN	6

extern char	mg_ename[MG_NENTITIES][MG_MAXELEN];

			/* Handler routines for each entity */

#ifdef NOPROTO
extern int	(*mg_ehand[MG_NENTITIES])();
#else
extern int	(*mg_ehand[MG_NENTITIES])(int argc, char **argv);
#endif

			/* Error codes */
#define MG_OK		0		/* normal return value */
#define MG_EUNK		1		/* unknown entity */
#define MG_EARGC	2		/* wrong number of arguments */
#define MG_ETYPE	3		/* argument type error */
#define MG_EILL		4		/* illegal argument value */
#define MG_EUNDEF	5		/* undefined reference */
#define MG_ENOFILE	6		/* cannot open input file */
#define MG_EINCL	7		/* error in included file */
#define MG_EMEM		8		/* out of memory */
#define MG_ESEEK	9		/* file seek error */

#define MG_NERRS	10

extern char	*mg_err[MG_NERRS];

/*
 * The general process for running the parser is to fill in the mg_ehand
 * array with handlers for each entity you know how to handle.
 * Then, call mg_init to fill in the rest.  This function will report
 * an error and quit if you try to support an inconsistent set of entities.
 * For each file you want to parse, call mg_load with the file name.
 * To read from standard input, use NULL as the file name.
 * For additional control over error reporting and file management,
 * use mg_open, mg_read, mg_parse and mg_close instead of mg_load.
 * To free any data structures and clear the parser, use mg_clear.
 * If there is an error, mg_load, mg_open, mg_parse, and mg_rewind
 * will return an error from the list above.  In addition, mg_load
 * will report the error to stderr.  The mg_read routine returns 0
 * when the end of file has been reached.
 */

#define MG_MAXLINE	512		/* maximum input line length */
#define MG_MAXARGC	(MG_MAXLINE/4)	/* maximum argument count */

typedef struct mg_fctxt {
	char	*fname;				/* file name */
	FILE	*fp;				/* stream pointer */
	char	inpline[MG_MAXLINE];		/* input line */
	int	lineno;				/* line number */
	struct mg_fctxt	*prev;			/* previous context */
} MG_FCTXT;

extern MG_FCTXT	*mg_file;		/* current file context */

#ifdef NOPROTO
extern void	mg_init();		/* fill in mg_ehand array */
extern int	mg_load();		/* parse a file */
extern int	mg_open();		/* open new input file */
extern int	mg_read();		/* read next line */
extern int	mg_parse();		/* parse current line */
extern int	mg_rewind();		/* rewind input file */
extern void	mg_close();		/* close input file */
extern void	mg_clear();		/* clear parser */
extern int	mg_iterate();
#else
extern void	mg_init(void);		/* fill in mg_ehand array */
extern int	mg_load(char *);	/* parse a file */
extern int	mg_open(MG_FCTXT *, char *);	/* open new input file */
extern int	mg_read(void);		/* read next line */
extern int	mg_parse(void);		/* parse current line */
extern int	mg_rewind(void);	/* rewind input file */
extern void	mg_close(void);		/* close input file */
extern void	mg_clear(void);		/* clear parser */
extern int	mg_iterate(int, char **, int (*)(void));
#endif

#ifndef MG_NQCD
#define MG_NQCD		5		/* default number of divisions */
#endif

extern int	mg_nqcdivs;		/* divisions per quarter circle */

/*
 * The following library routines are included for your convenience:
 */

#ifdef NOPROTO
extern int mg_entity();			/* get entity number from its name */
extern int isint();			/* non-zero if integer format */
extern int isflt();			/* non-zero if floating point format */
#else
extern int mg_entity(char *);		/* get entity number from its name */
extern int isint(char *);		/* non-zero if integer format */
extern int isflt(char *);		/* non-zero if floating point format */
#endif

/************************************************************************
 *	Definitions for 3-d vector manipulation functions
 */

typedef double  FVECT[3];

#ifdef NOPROTO
extern double	normalize();		/* normalize a vector */
#else
extern double	normalize(FVECT);	/* normalize a vector */
#endif

/************************************************************************
 *	Definitions for context handling routines
 *	(materials, colors, vectors)
 */

typedef struct {
	double	cx, cy;		/* XY chromaticity coordinates */
} C_COLOR;		/* color context */

typedef struct {
	double	rd;		/* diffuse reflectance */
	C_COLOR	rd_c;		/* diffuse reflectance color */
	double	td;		/* diffuse transmittance */
	C_COLOR	td_c;		/* diffuse transmittance color */
	double	ed;		/* diffuse emittance */
	C_COLOR	ed_c;		/* diffuse emittance color */
	double	rs;		/* specular reflectance */
	C_COLOR	rs_c;		/* specular reflectance color */
	double	rs_a;		/* specular reflectance roughness */
	double	ts;		/* specular transmittance */
	C_COLOR	ts_c;		/* specular transmittance color */
	double	ts_a;		/* specular transmittance roughness */
} C_MATERIAL;		/* material context */

typedef struct {
	FVECT	p, n;		/* point and normal */
} C_VERTEX;		/* vertex context */

#define C_DEFCOLOR	{.333,.333}
#define C_DEFMATERIAL	{0.,C_DEFCOLOR,0.,C_DEFCOLOR,0.,C_DEFCOLOR,\
			0.,C_DEFCOLOR,0.,0.,C_DEFCOLOR,0.}
#define C_DEFVERTEX	{{0.,0.,0.},{0.,0.,0.}}

extern C_COLOR		*c_ccolor;	/* the current color */
extern C_MATERIAL	*c_cmaterial;	/* the current material */
extern C_VERTEX		*c_cvertex;	/* the current vertex */

#ifdef NOPROTO
extern int	c_hcolor();			/* handle color entity */
extern int	c_hmaterial();			/* handle material entity */
extern int	c_hvertex();			/* handle vertex entity */
extern void	c_clearall();			/* clear context tables */
extern C_VERTEX	*c_getvert();			/* get a named vertex */
#else
extern int	c_hcolor(int, char **);		/* handle color entity */
extern int	c_hmaterial(int, char **);	/* handle material entity */
extern int	c_hvertex(int, char **);	/* handle vertex entity */
extern void	c_clearall(void);		/* clear context tables */
extern C_VERTEX	*c_getvert(char *);		/* get a named vertex */
#endif

/*************************************************************************
 *	Definitions for hierarchical object name handler
 */

extern int	obj_nnames;		/* depth of name hierarchy */
extern char	**obj_name;		/* names in hierarchy */

#ifdef NOPROTO
extern int	obj_handler();			/* handle an object entity */
#else
extern int	obj_handler(int, char **);	/* handle an object entity */
#endif

/**************************************************************************
 *	Definitions for hierarchical transformation handler
 */

typedef double  MAT4[4][4];

#ifdef  BSD
#define  copymat4(m4a,m4b)	bcopy((char *)m4b,(char *)m4a,sizeof(MAT4))
#else
#define  copymat4(m4a,m4b)	(void)memcpy((char *)m4a,(char *)m4b,sizeof(MAT4))
extern char  *memcpy();
#endif

#define  MAT4IDENT		{ {1.,0.,0.,0.}, {0.,1.,0.,0.}, \
				{0.,0.,1.,0.}, {0.,0.,0.,1.} }

extern MAT4  m4ident;

#define  setident4(m4)		copymat4(m4, m4ident)

				/* regular transformation */
typedef struct {
	MAT4  xfm;				/* transform matrix */
	double  sca;				/* scalefactor */
}  XF;

#define identxf(xp)		(void)(setident4((xp)->xfm),(xp)->sca=1.0)

typedef struct xf_spec {
	short	xac;		/* transform argument count */
	short	xav0;		/* zeroeth argument in xf_argv array */
	XF	xf;		/* cumulative transformation */
	struct xf_spec	*prev;	/* previous transformation context */
} XF_SPEC;

extern int	xf_argc;			/* total # transform args. */
extern char	**xf_argv;			/* transform arguments */
extern XF_SPEC	*xf_context;			/* current context */

/*
 * The transformation handler should do most of the work that needs
 * doing.  Just pass it any xf entities, then use the associated
 * functions to transform and translate points, transform vectors
 * (without translation), rotate vectors (without scaling) and scale
 * values appropriately.
 *
 * The routines xf_xfmpoint, xf_xfmvect and xf_rotvect take two
 * 3-D vectors (which may be identical), transforms the second and
 * puts the result into the first.
 */

#ifdef NOPROTO

extern int	xf_handler();		/* handle xf entity */
extern void	xf_xfmpoint();		/* transform point */
extern void	xf_xfmvect();		/* transform vector */
extern void	xf_rotvect();		/* rotate vector */
extern double	xf_scale();		/* scale a value */

/* The following are support routines you probably won't call directly */

extern void	multmat4();		/* m4a = m4b X m4c */
extern void	multv3();		/* v3a = v3b X m4 (vectors) */
extern void	multp3();		/* p3a = p3b X m4 (points) */
extern int	xf();			/* interpret transform spec. */

#else

extern int	xf_handler();		/* handle xf entity */
extern void	xf_xfmpoint();		/* transform point */
extern void	xf_xfmvect();		/* transform vector */
extern void	xf_rotvect();		/* rotate vector */
extern double	xf_scale();		/* scale a value */

/* The following are support routines you probably won't call directly */

extern void	multmat4(MAT4, MAT4, MAT4);	/* m4a = m4b X m4c */
extern void	multv3(FVECT, FVECT, MAT4);	/* v3a = v3b X m4 (vectors) */
extern void	multp3(FVECT, FVECT, MAT4);	/* p3a = p3b X m4 (points) */
extern int	xf(XF, int, char **);		/* interpret transform spec. */

#endif

/************************************************************************
 *	Miscellaneous definitions
 */

#ifdef	M_PI
#define	 PI		M_PI
#else
#define	 PI		3.14159265358979323846
#endif

#ifdef DCL_ATOF
extern double	atof();
#endif

#ifndef MEM_PTR
#define MEM_PTR		void *
#endif

extern MEM_PTR	malloc();
extern MEM_PTR	calloc();
extern MEM_PTR	realloc();
