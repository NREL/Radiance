/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  otypes.h - defines for object types.
 *
 *     1/28/86
 */

typedef struct {
	char  *funame;			/* function name */
	int  flags;			/* type flags */
	int  (*funp)();			/* pointer to function */
}  FUN;
				/* object types in decreasing frequency */
#define  OBJ_FACE	0		/* polygon */
#define  OBJ_CONE	1		/* cone */
#define  OBJ_SPHERE	2		/* sphere */
#define  TEX_FUNC	3		/* surface texture function */
#define  OBJ_RING	4		/* disk */
#define  OBJ_CYLINDER	5		/* cylinder */
#define  OBJ_INSTANCE	6		/* octree instance */
#define  OBJ_CUP	7		/* inverted cone */
#define  OBJ_BUBBLE	8		/* inverted sphere */
#define  OBJ_TUBE	9		/* inverted cylinder */
#define  MAT_PLASTIC	10		/* plastic surface */
#define  MAT_METAL	11		/* metal surface */
#define  MAT_GLASS	12		/* thin glass surface */
#define  MAT_TRANS	13		/* translucent material */
#define  MAT_DIELECTRIC	14		/* dielectric material */
#define  MAT_INTERFACE	15		/* dielectric interface */
#define  MAT_PFUNC	16		/* plastic brdf function */
#define  MAT_MFUNC	17		/* metal brdf function */
#define  PAT_BFUNC	18		/* brightness function */
#define  PAT_BDATA	19		/* brightness data */
#define  PAT_BTEXT	20		/* monochromatic text */
#define  PAT_CPICT	21		/* color picture */
#define  MAT_GLOW	22		/* proximity light source */
#define  OBJ_SOURCE	23		/* distant source */
#define  MAT_LIGHT	24		/* primary light source */
#define  MAT_ILLUM	25		/* secondary light source */
#define  MAT_SPOT	26		/* spot light source */
#define  MAT_MIRROR	27		/* mirror (secondary source) */
#define  MAT_TFUNC	28		/* trans brdf function */
#define  MAT_BRTDF	29		/* brtd function */
#define  MAT_PDATA	30		/* plastic brdf data */
#define  MAT_MDATA	31		/* metal brdf data */
#define  MAT_TDATA	32		/* trans brdf data */
#define  PAT_CFUNC	33		/* color function */
#define  MAT_CLIP	34		/* clipping surface */
#define  PAT_CDATA	35		/* color data */
#define  PAT_CTEXT	36		/* colored text */
#define  TEX_DATA	37		/* surface texture data */
#define  MIX_FUNC	38		/* mixing function */
#define  MIX_DATA	39		/* mixing data */
#define  MIX_TEXT	40		/* mixing text */
#define  MAT_DIRECT1	41		/* unidirecting material */
#define  MAT_DIRECT2	42		/* bidirecting material */
				/* number of object types */
#define  NUMOTYPE	43
				/* type flags */
#define  T_S		01		/* surface (object) */
#define  T_M		02		/* material */
#define  T_P		04		/* pattern */
#define  T_T		010		/* texture */
#define  T_X		020		/* mixture */
#define  T_V		040		/* volume */
#define  T_L		0100		/* light source modifier */
#define  T_LV		0200		/* virtual light source modifier */
#define  T_F		0400		/* function */
#define  T_D		01000		/* data */
#define  T_I		02000		/* picture */
#define  T_E		04000		/* text */
				/* user-defined types */
#define  T_SP1		010000
#define  T_SP2		020000
#define  T_SP3		040000

extern FUN  ofun[];			/* our type list */

#define  issurface(t)	(ofun[t].flags & T_S)
#define  isvolume(t)	(ofun[t].flags & T_V)
#define  ismodifier(t)	(!issurface(t))
#define  ismaterial(t)	(ofun[t].flags & T_M)
#define  istexture(t)	(ofun[t].flags & (T_P|T_T|T_X))
#define  islight(t)	(ofun[t].flags & T_L)
#define  isvlight(t)	(ofun[t].flags & T_LV)
#define  hasdata(t)	(ofun[t].flags & (T_D|T_I))
#define  hasfunc(t)	(ofun[t].flags & (T_F|T_D|T_I))
#define  hastext(t)	(ofun[t].flags & T_E)

extern int  o_default();
					/* type list initialization */
#define  INIT_OTYPE	{	{ "polygon",	T_S,		o_default }, \
				{ "cone",	T_S,		o_default }, \
				{ "sphere",	T_S,		o_default }, \
				{ "texfunc",	T_T|T_F,	o_default }, \
				{ "ring",	T_S,		o_default }, \
				{ "cylinder",	T_S,		o_default }, \
				{ "instance",	T_S|T_V,	o_default }, \
				{ "cup",	T_S,		o_default }, \
				{ "bubble",	T_S,		o_default }, \
				{ "tube",	T_S,		o_default }, \
				{ "plastic",	T_M,		o_default }, \
				{ "metal",	T_M,		o_default }, \
				{ "glass",	T_M,		o_default }, \
				{ "trans",	T_M,		o_default }, \
				{ "dielectric",	T_M,		o_default }, \
				{ "interface",	T_M,		o_default }, \
				{ "plasfunc",	T_M|T_F,	o_default }, \
				{ "metfunc",	T_M|T_F,	o_default }, \
				{ "brightfunc",	T_P|T_F,	o_default }, \
				{ "brightdata",	T_P|T_D,	o_default }, \
				{ "brighttext",	T_P|T_E,	o_default }, \
				{ "colorpict",	T_P|T_I,	o_default }, \
				{ "glow",	T_M|T_L,	o_default }, \
				{ "source",	T_S,		o_default }, \
				{ "light",	T_M|T_L,	o_default }, \
				{ "illum",	T_M|T_L,	o_default }, \
				{ "spotlight",	T_M|T_L,	o_default }, \
				{ "mirror",	T_M|T_LV,	o_default }, \
				{ "transfunc",	T_M|T_F,	o_default }, \
				{ "BRTDfunc",	T_M|T_F,	o_default }, \
				{ "plasdata",	T_M|T_D,	o_default }, \
				{ "metdata",	T_M|T_D,	o_default }, \
				{ "transdata",	T_M|T_D,	o_default }, \
				{ "colorfunc",	T_P|T_F,	o_default }, \
				{ "antimatter",	T_M,		o_default }, \
				{ "colordata",	T_P|T_D,	o_default }, \
				{ "colortext",	T_P|T_E,	o_default }, \
				{ "texdata",	T_T|T_D,	o_default }, \
				{ "mixfunc",	T_X|T_F,	o_default }, \
				{ "mixdata",	T_X|T_D,	o_default }, \
				{ "mixtext",	T_X|T_E,	o_default }, \
				{ "prism1",	T_M|T_F|T_LV,	o_default }, \
				{ "prism2",	T_M|T_F|T_LV,	o_default }, \
			}

#define  ALIASID	"alias"		/* alias type identifier */
