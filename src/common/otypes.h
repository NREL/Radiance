/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  otypes.h - defines for object types.
 *
 *     1/28/86
 */

typedef struct {
	char  *funame;			/* function name */
	int  (*funp)();			/* pointer to function */
}  FUN;
				/* surface types */
#define  OBJ_MIN	0
#define  OBJ_SOURCE	(OBJ_MIN+0)	/* distant source */
#define  OBJ_SPHERE	(OBJ_MIN+1)	/* sphere */
#define  OBJ_BUBBLE	(OBJ_MIN+2)	/* inverted sphere */
#define  OBJ_FACE	(OBJ_MIN+3)	/* polygon */
#define  OBJ_CONE	(OBJ_MIN+4)	/* cone */
#define  OBJ_CUP	(OBJ_MIN+5)	/* inverted cone */
#define  OBJ_CYLINDER	(OBJ_MIN+6)	/* cylinder */
#define  OBJ_TUBE	(OBJ_MIN+7)	/* inverted cylinder */
#define  OBJ_RING	(OBJ_MIN+8)	/* disk */
#define  OBJ_INSTANCE	(OBJ_MIN+9)	/* octree instance */
#define  OBJ_CNT	10
				/* material types */
#define  MOD_MIN	MAT_MIN
#define  MAT_MIN	(OBJ_MIN+OBJ_CNT)
#define  MAT_LIGHT	(MAT_MIN+0)	/* primary light source */
#define  MAT_ILLUM	(MAT_MIN+1)	/* secondary light source */
#define  MAT_GLOW	(MAT_MIN+2)	/* proximity light source */
#define  MAT_SPOT	(MAT_MIN+3)	/* spot light source */
#define  MAT_PLASTIC	(MAT_MIN+4)	/* plastic surface */
#define  MAT_METAL	(MAT_MIN+5)	/* metal surface */
#define  MAT_TRANS	(MAT_MIN+6)	/* translucent material */
#define  MAT_DIELECTRIC	(MAT_MIN+7)	/* dielectric material */
#define  MAT_INTERFACE	(MAT_MIN+8)	/* dielectric interface */
#define  MAT_GLASS	(MAT_MIN+9)	/* thin glass surface */
#define  MAT_CLIP	(MAT_MIN+10)	/* clipping surface */
#define  MAT_CNT	11
				/* textures and patterns */
#define  TP_MIN		(MAT_MIN+MAT_CNT)
#define  TEX_FUNC	(TP_MIN+0)	/* surface texture function */
#define  TEX_DATA	(TP_MIN+1)	/* surface texture data */
#define  PAT_CFUNC	(TP_MIN+2)	/* color function */
#define  PAT_BFUNC	(TP_MIN+3)	/* brightness function */
#define  PAT_CPICT	(TP_MIN+4)	/* color picture */
#define  PAT_CDATA	(TP_MIN+5)	/* color data */
#define  PAT_BDATA	(TP_MIN+6)	/* brightness data */
#define  PAT_CTEXT	(TP_MIN+7)	/* colored text */
#define  PAT_BTEXT	(TP_MIN+8)	/* monochromatic text */
#define  MIX_FUNC	(TP_MIN+9)	/* mixing function */
#define  MIX_DATA	(TP_MIN+10)	/* mixing data */
#define  MIX_TEXT	(TP_MIN+11)	/* mixing text */
#define  TP_CNT		12
#define  MOD_CNT	(MAT_CNT+TP_CNT)
				/* number of object types */
#define  NUMOTYPE	(OBJ_CNT+MAT_CNT+TP_CNT)

#define  issurface(t)	((t) >= OBJ_MIN && (t) < OBJ_MIN+OBJ_CNT)
#define  ismodifier(t)	((t) >= MOD_MIN && (t) < MOD_MIN+MOD_CNT)
#define  ismaterial(t)	((t) >= MAT_MIN && (t) < MAT_MIN+MAT_CNT)
#define  istexture(t)	((t) >= TP_MIN && (t) < TP_MIN+TP_CNT)

extern int  o_source();
extern int  o_sphere();
extern int  o_face();
extern int  o_cone();
extern int  o_instance();
extern int  m_light();
extern int  m_normal();
extern int  m_dielectric();
extern int  m_glass();
extern int  m_clip();
extern int  t_func(), t_data();
extern int  p_cfunc(), p_bfunc();
extern int  p_pdata(), p_cdata(), p_bdata();
extern int  mx_func(), mx_data();
extern int  text();

#define  INIT_OTYPE	{ { "source", o_source }, \
			{ "sphere", o_sphere }, \
			{ "bubble", o_sphere }, \
			{ "polygon", o_face }, \
			{ "cone", o_cone }, \
			{ "cup", o_cone }, \
			{ "cylinder", o_cone }, \
			{ "tube", o_cone }, \
			{ "ring", o_cone }, \
			{ "instance", o_instance }, \
			{ "light", m_light }, \
			{ "illum", m_light }, \
			{ "glow", m_light }, \
			{ "spotlight", m_light }, \
			{ "plastic", m_normal }, \
			{ "metal", m_normal }, \
			{ "trans", m_normal }, \
			{ "dielectric", m_dielectric }, \
			{ "interface", m_dielectric }, \
			{ "glass", m_glass }, \
			{ "antimatter", m_clip }, \
			{ "texfunc", t_func }, \
			{ "texdata", t_data }, \
			{ "colorfunc", p_cfunc }, \
			{ "brightfunc", p_bfunc }, \
			{ "colorpict", p_pdata }, \
			{ "colordata", p_cdata }, \
			{ "brightdata", p_bdata }, \
			{ "colortext", text }, \
			{ "brighttext", text }, \
			{ "mixfunc", mx_func }, \
			{ "mixdata", mx_data }, \
			{ "mixtext", text } }

#define  ALIASID	"alias"		/* alias type identifier */

extern FUN  ofun[];
