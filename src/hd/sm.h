/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm.h
 */

#ifndef _SM_H_
#define _SM_H_

#include "rhd_sample.h"
#define NEWSETS


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif


#define ON_V 1
#define ON_P 2
#define ON_E 3
#define IN_T 4

#define S_REPLACE_EPS 0.04      /* if (distance on sphere between sample
				     and a base point) < S_REPLACE_EPS,
				     replace base: 
				     */
#define S_REPLACE_SCALE (5.*5.)   /* if (distance to new point squared) is
				     > (triangle edge length squared*
				     S_REPLACE_SCALE):for all edges/triangle
				     vertices: new point is puncture
				     point: dont add
				   */
#define S_REPLACE_TRI 2e-8		/* .052 radians to the sixth power */

#define SQRT3_2 0.8660254

#define SM_DEFAULT 0
#define SM_EXTRA_POINTS 162
#define SM_BASE_TRIS 320
#define SM_EXTRA_VERTS  SM_EXTRA_POINTS

#define SM_INC_PERCENT 0.60            /* If number of new triangles added 
					  since last full redraw is > 
					  (SM_INC_PERCENT * total triangles)
					  do full redraw instead of incremental
					  */

#define SM_VIEW_FRAC   0.1


typedef int VERT;  /* One triangle that vertex belongs to- the rest
		      are derived by traversing neighbors */

typedef struct _EDGE {
    int verts[2];
    int tris[2];
} EDGE;

#define E_NTH_VERT(e,i)  ((e>0)?Edges[(e)].verts[(i)]:Edges[-(e)].verts[(1-i)])
#define SET_E_NTH_VERT(e,i,v) if(e>0) Edges[(e)].verts[(i)]=(v); \
			       else Edges[-(e)].verts[(1-i)]=(v)
#define E_NTH_TRI(e,i)  ((e>0)?Edges[(e)].tris[(i)]:Edges[-(e)].tris[(1-i)])
#define SET_E_NTH_TRI(e,i,v) if(e>0) Edges[(e)].tris[(i)]=(v); \
                              else Edges[-(e)].tris[(1-i)]=(v)
#define eClear_edges() (Ecnt = 0)

#define FOR_ALL_EDGES(i) for((i)=1; (i) <= Ecnt; i++)
#define FOR_ALL_EDGES_FROM(e,i) for((i)=++e; (i) <= Ecnt; i++)


typedef struct _TRI {
  int verts[3];         /* Ids into sample and vertex array for each vertex*/
  int nbrs[3]; /* Ids for neighboring triangles: -1 if invalid */
}TRI;


#define T_NTH_NBR(t,i) ((t)->nbrs[(i)])
#define T_CLEAR_NBRS(t) (T_NTH_NBR(t,0)=T_NTH_NBR(t,1)=T_NTH_NBR(t,2)=-1)
#define T_NTH_NBR_PTR(t,n) \
         (T_NTH_NBR(n,0)==(t)?0:T_NTH_NBR(n,1)==(t)?1:T_NTH_NBR(n,2)==(t)?2:-1)
#define T_NTH_V(t,i)  ((t)->verts[(i)])
#define T_WHICH_V(t,i)     \
         (T_NTH_V(t,0)==(i)?0:T_NTH_V(t,1)==(i)?1:T_NTH_V(t,2)==(i)?2:-1)
#define T_NEXT_FREE(t) ((t)->nbrs[0])
#define T_NEXT_AVAILABLE(t) ((t)->nbrs[0])
#define T_VALID_FLAG(t) ((t)->nbrs[1])
#define T_IS_VALID(t)  (T_VALID_FLAG(t)!=-1)
#define T_FLAGS 4

typedef struct _SM {
    FVECT view_center;    /* Canonical view center defining unit sphere */
    SAMP *samples;        /* Sample point information */
    STREE locator;        /* spherical quadtree for point/triangle location */
    int max_tris;         /* Maximum number of triangles */
    int num_tri;          /* Current number of triangles */
    int sample_tris;      /* Current number of non-base triangles*/
    int free_tris;        /* pointer to free_list */
    int available_tris;   /* pointer to available_list */
    int max_verts;        /* Maximum number of vertices in the mesh */
    TRI *tris;            /* Pointer to list of triangle structs */
    VERT *verts;          /* List of vertices */
    int4 *flags[T_FLAGS]; /* Bit 0 set if active(in current frustum) */
                          /* Bit 1 set if not rendered since created */
                          /* Bit 2 set if base triangle */
}SM;

#define T_ACTIVE_FLAG   0
#define T_NEW_FLAG      1
#define T_BASE_FLAG     2
#define T_BG_FLAG       3

#define SM_VIEW_CENTER(m)             ((m)->view_center)
#define SM_SAMP(m)                    ((m)->samples)
#define SM_LOCATOR(m)                 (&((m)->locator))
#define SM_MAX_TRIS(m)                ((m)->max_tris)
#define SM_NUM_TRI(m)                 ((m)->num_tri)
#define SM_SAMPLE_TRIS(m)                 ((m)->sample_tris)
#define SM_FREE_TRIS(m)               ((m)->free_tris)
#define SM_AVAILABLE_TRIS(m)          ((m)->available_tris)
#define SM_MAX_VERTS(m)               ((m)->max_verts)
#define SM_TRIS(m)                    ((m)->tris)
#define SM_VERTS(m)                   ((m)->verts)
#define SM_NTH_FLAGS(m,n)             ((m)->flags[(n)])
#define SM_FLAGS(m)                   ((m)->flags)


#define SM_IS_NTH_T_FLAG(sm,n,f)  IS_FLAG(SM_NTH_FLAGS(sm,f),n)
#define SM_SET_NTH_T_FLAG(sm,n,f) SET_FLAG(SM_NTH_FLAGS(sm,f),n)
#define SM_CLR_NTH_T_FLAG(sm,n,f) CLR_FLAG(SM_NTH_FLAGS(sm,f),n)

#define SM_IS_NTH_T_ACTIVE(sm,n)     SM_IS_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_IS_NTH_T_BASE(sm,n)       SM_IS_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_IS_NTH_T_NEW(sm,n)        SM_IS_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_IS_NTH_T_BG(sm,n)        SM_IS_NTH_T_FLAG(sm,n,T_BG_FLAG)

#define SM_SET_NTH_T_ACTIVE(sm,n)    SM_SET_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_SET_NTH_T_BASE(sm,n)      SM_SET_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_SET_NTH_T_NEW(sm,n)       SM_SET_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_SET_NTH_T_BG(sm,n)        SM_SET_NTH_T_FLAG(sm,n,T_BG_FLAG)

#define SM_CLR_NTH_T_ACTIVE(sm,n)   SM_CLR_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_CLR_NTH_T_BASE(sm,n)     SM_CLR_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_CLR_NTH_T_NEW(sm,n)      SM_CLR_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_CLR_NTH_T_BG(sm,n)       SM_CLR_NTH_T_FLAG(sm,n,T_BG_FLAG)

#define SM_NTH_TRI(m,n)               (&(SM_TRIS(m)[(n)]))
#define SM_NTH_VERT(m,n)              (SM_VERTS(m)[(n)])

#define SM_T_ID_VALID(s,t_id) T_IS_VALID(SM_NTH_TRI(s,t_id))
 

#define SM_MAX_SAMP(m)                S_MAX_SAMP(SM_SAMP(m))
#define SM_MAX_POINTS(m)              S_MAX_POINTS(SM_SAMP(m))
#define SM_SAMP_BASE(m)               S_BASE(SM_SAMP(m))
#define SM_NTH_WV(m,i)                S_NTH_W_PT(SM_SAMP(m),i)
#define SM_NTH_W_DIR(m,i)             S_NTH_W_DIR(SM_SAMP(m),i)
#define SM_DIR_ID(m,i)            (!SM_BASE_ID(m,i) && SM_NTH_W_DIR(m,i)==-1)
#define SM_NTH_RGB(m,i)               S_NTH_RGB(SM_SAMP(m),i)
#define SM_RGB(m)                    S_RGB(SM_SAMP(m))
#define SM_WP(m)                    S_W_PT(SM_SAMP(m))
#define SM_BRT(m)                     S_BRT(SM_SAMP(m))
#define SM_NTH_BRT(m,i)               S_NTH_BRT(SM_SAMP(m),i)
#define SM_CHR(m)                     S_CHR(SM_SAMP(m))
#define SM_NTH_CHR(m,i)               S_NTH_CHR(SM_SAMP(m),i)
#define SM_NUM_SAMP(m)               S_NUM_SAMP(SM_SAMP(m))
#define SM_TONE_MAP(m)               S_TONE_MAP(SM_SAMP(m))

#define SM_ALLOWED_VIEW_CHANGE(m) (SM_NUM_SAMP(m)/smDist_sum*SM_VIEW_FRAC)

#define SM_FOR_ALL_ADJACENT_TRIS(sm,id,t) for(t=smTri_next_ccw_nbr(sm,t,id); \
t!=SM_NTH_TRI(sm,SM_NTH_VERT(sm,id)); t=smTri_next_ccw_nbr(sm,t,id))

#define SM_INVALID_SAMP_ID(sm,id)  (((id) < 0) || ((id) >= SM_MAX_SAMP(sm)))
#define SM_INVALID_POINT_ID(sm,id)  (((id) < 0) || ((id) >= SM_MAX_POINTS(sm)))
#define SM_T_NTH_WV(sm,t,i)  (SM_NTH_WV(sm,T_NTH_V(t,i)))

#define SM_BASE_ID(s,i)  \
 ((i) >= S_MAX_SAMP(SM_SAMP(s)) && (i) < S_MAX_BASE_PT(SM_SAMP(s)))

#define SM_BG_SAMPLE(sm,i)   (SM_NTH_W_DIR(sm,i)==-1)

#define SM_BG_TRI(sm,i)  (SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),0)) && \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),1)) && \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),2)))
#define SM_MIXED_TRI(sm,i)  (SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),0)) || \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),1)) || \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),2)))

#define SM_FOR_ALL_SAMPLES(sm,i) for((i)=0;i < SM_NUM_SAMP(sm);(i)++)

#define smInit_locator(sm,c)    (stInit(SM_LOCATOR(sm)), \
				 ST_SET_CENTER(SM_LOCATOR(sm),c))
#define smClear_locator(sm)    stClear(SM_LOCATOR(sm))
#define smAlloc_locator(sm)   stAlloc(SM_LOCATOR(sm))
#define smFree_locator(sm)   stClear(SM_LOCATOR(sm))
#define smPointLocateCell(sm,pt)  stPoint_locate(SM_LOCATOR(sm),pt)
#define smUnalloc_samp(sm,id)  sUnalloc_samp(SM_SAMP(sm),id)
#define smFree_samples(sm)     sFree(SM_SAMP(sm))
#define smClear_samples(sm)    sClear(SM_SAMP(sm))
#define smInit_samples(sm)     sInit(SM_SAMP(sm))


#define smClear_vert(sm,id)    (SM_NTH_VERT(sm,id) = INVALID)

typedef struct _RT_ARGS_{
  FVECT orig,dir;
  int t_id;
  OBJECT *os;
}RT_ARGS;


typedef struct _ADD_ARGS {
  int t_id;
  OBJECT *del_set;
}ADD_ARGS;

extern SM *smMesh;
extern int smNew_tri_cnt;
extern double smDist_sum;


#ifdef TEST_DRIVER
extern VIEW View;
extern VIEW Current_View;
extern int Pick_tri,Picking,Pick_samp;
extern FVECT Pick_point[500],Pick_origin,Pick_dir;
extern FVECT Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern int Pick_q[500];
extern FVECT P0,P1,P2;
extern int Pick_cnt;
extern FVECT FrustumNear[4],FrustumFar[4];
#endif


/*
 * int
 * smInit(n)		: Initialize/clear data structures for n entries
 * int	n;
 *
 * Initialize sampL and other data structures for at least n samples.
 * If n is 0, then free data structures.  Return number actually allocated.
 *
 *
 * int
 * smNewSamp(c, p, v)	: register new sample point and return index
 * COLR	c;		: pixel color (RGBE)
 * FVECT	p;	: world intersection point
 * FVECT	v;	: ray direction vector
 *
 * Add new sample point to data structures, removing old values as necessary.
 * New sample representation will be output in next call to smUpdate().
 *
 *
 * int
 * smFindSamp(orig, dir): intersect ray with 3D rep. and find closest sample
 * FVECT	orig, dir;
 *
 * Find the closest sample to the given ray.  Return -1 on failure.
 *
 *
 * smClean()		: display has been wiped clean
 *
 * Called after display has been effectively cleared, meaning that all
 * geometry must be resent down the pipeline in the next call to smUpdate().
 *
 *
 * smUpdate(vp, qua)	: update OpenGL output geometry for view vp
 * VIEW	*vp;		: desired view
 * int	qua;		: quality level (percentage on linear time scale)
 *
 * Draw new geometric representation using OpenGL calls.  Assume that the
 * view has already been set up and the correct frame buffer has been
 * selected for drawing.  The quality level is on a linear scale, where 100%
 * is full (final) quality.  It is not necessary to redraw geometry that has
 * been output since the last call to smClean().  (The last view drawn will
 * be vp==&odev.v each time.)
 */
#endif








