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
#define MG_E_CMIX	3
#define MG_E_CSPEC	4
#define MG_E_CXY	5
#define MG_E_CYL	6
#define MG_E_ED		7
#define MG_E_FACE	8
#define MG_E_INCLUDE	9
#define MG_E_IES	10
#define MG_E_MATERIAL	11
#define MG_E_NORMAL	12
#define MG_E_OBJECT	13
#define MG_E_POINT	14
#define MG_E_PRISM	15
#define MG_E_RD		16
#define MG_E_RING	17
#define MG_E_RS		18
#define MG_E_SPH	19
#define MG_E_TD		20
#define MG_E_TORUS	21
#define MG_E_TS		22
#define MG_E_VERTEX	23
#define MG_E_XF		24

#define MG_NENTITIES	25

#define MG_NAMELIST	{"#","c","cone","cmix","cspec","cxy","cyl","ed","f",\
			"i","ies","m","n","o","p","prism","rd","ring","rs",\
			"sph","td","torus","ts","v","xf"}

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
#define MG_EBADMAT	10		/* bad material specification */

#define MG_NERRS	11

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
 * To pass an entity of your own construction to the parser, use
 * the mg_handle function rather than the mg_ehand routines directly.
 * (The first argument to mg_handle is the entity #, or -1.)
 * To free any data structures and clear the parser, use mg_clear.
 * If there is an error, mg_load, mg_open, mg_parse, and mg_rewind
 * will return an error from the list above.  In addition, mg_load
 * will report the error to stderr.  The mg_read routine returns 0
 * when the end of file has been reached.
 */

#define MG_MAXLINE	512		/* maximum input line length */
#define MG_MAXARGC	(MG_MAXLINE/4)	/* maximum argument count */

typedef struct mg_fctxt {
	char	fname[96];			/* file name */
	FILE	*fp;				/* stream pointer */
	int	fid;				/* unique file context id */
	char	inpline[MG_MAXLINE];		/* input line */
	int	lineno;				/* line number */
	struct mg_fctxt	*prev;			/* previous context */
} MG_FCTXT;

typedef struct {
	int	fid;				/* file this position is for */
	int	lineno;				/* line number in file */
	long	offset;				/* offset from beginning */
} MG_FPOS;

extern MG_FCTXT	*mg_file;		/* current file context */

#ifdef NOPROTO
extern void	mg_init();		/* fill in mg_ehand array */
extern int	mg_load();		/* parse a file */
extern int	mg_open();		/* open new input file */
extern int	mg_read();		/* read next line */
extern int	mg_parse();		/* parse current line */
extern void	mg_fgetpos();		/* get position on input file */
extern int	mg_fgoto();		/* go to position on input file */
extern void	mg_close();		/* close input file */
extern void	mg_clear();		/* clear parser */
extern int	mg_handle();		/* handle an entity */
#else
extern void	mg_init(void);		/* fill in mg_ehand array */
extern int	mg_load(char *);	/* parse a file */
extern int	mg_open(MG_FCTXT *, char *);	/* open new input file */
extern int	mg_read(void);		/* read next line */
extern int	mg_parse(void);		/* parse current line */
extern void	mg_fgetpos(MG_FPOS *);	/* get position on input file */
extern int	mg_fgoto(MG_FPOS *);	/* go to position on input file */
extern void	mg_close(void);		/* close input file */
extern void	mg_clear(void);		/* clear parser */
extern int	mg_handle(int, int, char **);	/* handle an entity */
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

#ifdef  SMLFLT
#define  FLOAT		float
#define  FTINY		(1e-3)
#else
#define  FLOAT		double
#define  FTINY		(1e-6)
#endif
#define  FHUGE		(1e10)

typedef FLOAT  FVECT[3];

#define  VCOPY(v1,v2)	((v1)[0]=(v2)[0],(v1)[1]=(v2)[1],(v1)[2]=(v2)[2])
#define  DOT(v1,v2)	((v1)[0]*(v2)[0]+(v1)[1]*(v2)[1]+(v1)[2]*(v2)[2])
#define  VSUM(vr,v1,v2,f)	((vr)[0]=(v1)[0]+(f)*(v2)[0], \
				(vr)[1]=(v1)[1]+(f)*(v2)[1], \
				(vr)[2]=(v1)[2]+(f)*(v2)[2])

#define is0vect(v)	(DOT(v,v) <= FTINY*FTINY)

#define round0(x)	if (x <= FTINY && x >= -FTINY) x = 0

#ifdef NOPROTO
extern double	normalize();		/* normalize a vector */
#else
extern double	normalize(FVECT);	/* normalize a vector */
#endif

/************************************************************************
 *	Definitions for context handling routines
 *	(materials, colors, vectors)
 */

#define C_CMINWL	380		/* minimum wavelength */
#define C_CMAXWL	780		/* maximum wavelength */
#define C_CNSS		41		/* number of spectral samples */
#define C_CWLI		((C_CMAXWL-C_CMINWL)/(C_CNSS-1))
#define C_CMAXV		10000		/* nominal maximum sample value */

#define C_CSSPEC	01		/* flag if spectrum is set */
#define C_CDSPEC	02		/* flag if defined w/ spectrum */
#define C_CSXY		04		/* flag if xy is set */
#define C_CDXY		010		/* flag if defined w/ xy */

typedef struct {
	char	*name;			/* material name */
	int	clock;			/* incremented each change */
	short	flags;			/* what's been set */
	short	ssamp[C_CNSS];		/* spectral samples, min wl to max */
	long	ssum;			/* straight sum of spectral values */
	float	cx, cy;			/* xy chromaticity value */
} C_COLOR;

#define C_DEFCOLOR	{ NULL, 0, C_CDXY|C_CSXY|C_CSSPEC,\
			{C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,\
			C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV,C_CMAXV},\
			(long)C_CNSS*C_CMAXV, 1./3., 1./3. }

#define C_CIEX		{ "_cie_x", 0, C_CDSPEC|C_CSSPEC|C_CSXY,\
			{14,42,143,435,1344,2839,3483,3362,2908,1954,956,\
			320,49,93,633,1655,2904,4334,5945,7621,9163,10263,\
			10622,10026,8544,6424,4479,2835,1649,874,468,227,\
			114,58,29,14,7,3,2,1,0}, 106836L, .735, .265 }

#define C_CIEY		{ "_cie_y", 0, C_CDSPEC|C_CSSPEC|C_CSXY,\
			{0,1,4,12,40,116,230,380,600,910,1390,2080,3230,\
			5030,7100,8620,9540,9950,9950,9520,8700,7570,6310,\
			5030,3810,2650,1750,1070,610,320,170,82,41,21,10,\
			5,2,1,1,0,0}, 106856L, .274, .717 }

#define C_CIEZ		{ "_cie_z", 0, C_CDSPEC|C_CSSPEC|C_CSXY,\
			{65,201,679,2074,6456,13856,17471,17721,16692,\
			12876,8130,4652,2720,1582,782,422,203,87,39,21,17,\
			11,8,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},\
			106770L, .167, .009 }

#define c_cval(c,l)	((double)(c)->ssamp[((l)-C_MINWL)/C_CWLI] / (c)->sum)

typedef struct {
	char	*name;		/* material name */
	int	clock;		/* incremented each change -- resettable */
	float	rd;		/* diffuse reflectance */
	C_COLOR	rd_c;		/* diffuse reflectance color */
	float	td;		/* diffuse transmittance */
	C_COLOR	td_c;		/* diffuse transmittance color */
	float	ed;		/* diffuse emittance */
	C_COLOR	ed_c;		/* diffuse emittance color */
	float	rs;		/* specular reflectance */
	C_COLOR	rs_c;		/* specular reflectance color */
	float	rs_a;		/* specular reflectance roughness */
	float	ts;		/* specular transmittance */
	C_COLOR	ts_c;		/* specular transmittance color */
	float	ts_a;		/* specular transmittance roughness */
} C_MATERIAL;		/* material context */

typedef struct {
	char	*name;		/* vector name */
	int	clock;		/* incremented each change -- resettable */
	FVECT	p, n;		/* point and normal */
} C_VERTEX;		/* vertex context */

#define C_DEFMATERIAL	{NULL,0,0.,C_DEFCOLOR,0.,C_DEFCOLOR,0.,C_DEFCOLOR,\
					0.,C_DEFCOLOR,0.,0.,C_DEFCOLOR,0.}
#define C_DEFVERTEX	{NULL,0,{0.,0.,0.},{0.,0.,0.}}

extern C_COLOR		*c_ccolor;	/* the current color */
extern C_MATERIAL	*c_cmaterial;	/* the current material */
extern C_VERTEX		*c_cvertex;	/* the current vertex */

#ifdef NOPROTO
extern int	c_hcolor();			/* handle color entity */
extern int	c_hmaterial();			/* handle material entity */
extern int	c_hvertex();			/* handle vertex entity */
extern void	c_clearall();			/* clear context tables */
extern C_VERTEX	*c_getvert();			/* get a named vertex */
extern C_COLOR	*c_getcolor();			/* get a named color */
extern void	c_ccvt();			/* fix color representation */
extern int	c_isgrey();			/* check if color is grey */
#else
extern int	c_hcolor(int, char **);		/* handle color entity */
extern int	c_hmaterial(int, char **);	/* handle material entity */
extern int	c_hvertex(int, char **);	/* handle vertex entity */
extern void	c_clearall(void);		/* clear context tables */
extern C_VERTEX	*c_getvert(char *);		/* get a named vertex */
extern C_COLOR	*c_getcolor(char *);		/* get a named color */
extern void	c_ccvt(C_COLOR *, int);		/* fix color representation */
extern int	c_isgrey(C_COLOR *);		/* check if color is grey */
#endif

/*************************************************************************
 *	Definitions for hierarchical object name handler
 */

extern int	obj_nnames;		/* depth of name hierarchy */
extern char	**obj_name;		/* names in hierarchy */

#ifdef NOPROTO
extern int	obj_handler();			/* handle an object entity */
extern void	obj_clear();			/* clear object stack */
#else
extern int	obj_handler(int, char **);	/* handle an object entity */
extern void	obj_clear(void);		/* clear object stack */
#endif

/**************************************************************************
 *	Definitions for hierarchical transformation handler
 */

typedef FLOAT  MAT4[4][4];

#ifdef  BSD
#define  copymat4(m4a,m4b)	bcopy((char *)m4b,(char *)m4a,sizeof(MAT4))
#else
#define  copymat4(m4a,m4b)	(void)memcpy((char *)m4a,(char *)m4b,sizeof(MAT4))
#endif

#define  MAT4IDENT		{ {1.,0.,0.,0.}, {0.,1.,0.,0.}, \
				{0.,0.,1.,0.}, {0.,0.,0.,1.} }

extern MAT4  m4ident;

#define  setident4(m4)		copymat4(m4, m4ident)

				/* regular transformation */
typedef struct {
	MAT4  xfm;				/* transform matrix */
	FLOAT  sca;				/* scalefactor */
}  XF;

#define identxf(xp)		(void)(setident4((xp)->xfm),(xp)->sca=1.0)

#define XF_MAXDIM	8		/* maximum array dimensions */

struct xf_array {
	MG_FPOS	spos;			/* starting position on input */
	int	ndim;			/* number of array dimensions */
	struct {
		short	i, n;		/* current count and maximum */
		char	arg[8];		/* string argument value */
	} aarg[XF_MAXDIM];
};

typedef struct xf_spec {
	long	xid;			/* unique transform id */
	short	xav0;			/* zeroeth argument in xf_argv array */
	short	xac;			/* transform argument count */
	XF	xf;			/* cumulative transformation */
	struct xf_array	*xarr;		/* transformation array pointer */
	struct xf_spec	*prev;		/* previous transformation context */
} XF_SPEC;			/* followed by argument buffer */

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
extern void	xf_clear();		/* clear xf stack */

/* The following are support routines you probably won't call directly */

extern void	multmat4();		/* m4a = m4b X m4c */
extern void	multv3();		/* v3a = v3b X m4 (vectors) */
extern void	multp3();		/* p3a = p3b X m4 (points) */
extern int	xf();			/* interpret transform spec. */

#else

extern int	xf_handler(int, char **);	/* handle xf entity */
extern void	xf_xfmpoint(FVECT, FVECT);	/* transform point */
extern void	xf_xfmvect(FVECT, FVECT);	/* transform vector */
extern void	xf_rotvect(FVECT, FVECT);	/* rotate vector */
extern double	xf_scale(double);		/* scale a value */
extern void	xf_clear(void);			/* clear xf stack */

/* The following are support routines you probably won't call directly */

extern void	multmat4(MAT4, MAT4, MAT4);	/* m4a = m4b X m4c */
extern void	multv3(FVECT, FVECT, MAT4);	/* v3a = v3b X m4 (vectors) */
extern void	multp3(FVECT, FVECT, MAT4);	/* p3a = p3b X m4 (points) */
extern int	xf(XF *, int, char **);		/* interpret transform spec. */

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
extern void	free();
