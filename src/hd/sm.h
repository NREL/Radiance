/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm.h
 */

#ifndef _SM_H_
#define _SM_H_

#include "rhd_sample.h"
#include "sm_stree.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define P_REPLACE_EPS 0.5
#define SQRT3_2 0.8660254

#define MAX_EDGES 200

#define SM_INVALID  -1
#define SM_DEFAULT   0
#define SM_EXTRA_POINTS 5
#define SM_EXTRA_VERTS  SM_EXTRA_POINTS
#define	SM_POINTSIZ	(3*sizeof(float)+ sizeof(int4))
#define SM_POINT_SPACE SM_EXTRA_POINTS * SM_POINTSIZ

#define SM_INC_PERCENT 0.60
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
#define eNew_edge() (((Ecnt) < MAX_EDGES)?(++Ecnt):SM_INVALID)
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
#define T_NEXT_FREE(t) ((t)->verts[0])
#define T_VALID_FLAG(t) ((t)->verts[1])
#define T_IS_VALID(t)  (T_VALID_FLAG(t)!=-1)
#define T_FLAGS 4
#define T_FLAG_BYTES T_FLAGS/8.0
#define T_TOTAL_FLAG_BYTES(c)  ((((c)+31)>>5)*sizeof(int4))

typedef struct _SM {
    FVECT view_center;    /* Canonical view center defining unit sphere */
    RSAMP *samples;       /* Sample point information */
    STREE locator;      /* spherical quadtree for point/triangle location */
    int max_tris;         /* Maximum number of triangles */
    int tri_cnt;          /* Total number of tris ever alloc at one time*/
    int num_tris;         /* Current number of sample (nonbase)triangles */
    int free_tris;        /* pointer to free_list */
    int max_verts;        /* Maximum number of vertices in the mesh */
    TRI *tris;            /* Pointer to list of triangle structs */
    VERT *verts;          /* List of vertices */
    int4 *flags[T_FLAGS]; /* Bit 0 set if active(in current frustum) */
                             /* Bit 1 set if not rendered since created */
                             /* Bit 2 set if base triangle */
                             /* Bit 3 set used for LRU replacement */
    char *base;           /* Allocated space */
}SM;

#define T_ACTIVE_FLAG   0
#define T_NEW_FLAG      1
#define T_BASE_FLAG     2
#define T_LRU_FLAG      3

#define SM_VIEW_CENTER(m)             ((m)->view_center)
#define SM_SAMP(m)                    ((m)->samples)
#define SM_LOCATOR(m)                 (&((m)->locator))
#define SM_MAX_TRIS(m)                ((m)->max_tris)
#define SM_TRI_CNT(m)                 ((m)->tri_cnt)
#define SM_NUM_TRIS(m)                 ((m)->num_tris)
#define SM_FREE_TRIS(m)               ((m)->free_tris)
#define SM_MAX_VERTS(m)               ((m)->max_verts)
#define SM_TRIS(m)                    ((m)->tris)
#define SM_VERTS(m)                   ((m)->verts)
#define SM_NTH_FLAGS(m,n)             ((m)->flags[(n)])
#define SM_BASE(m)                    ((m)->base)

#define TF_OFFSET(n)  ((n) >> 5)
#define TF_BIT(n)     ((n) & 0x1f)
#define SM_IS_NTH_T_FLAG(sm,n,f)  (SM_NTH_FLAGS(sm,f)[TF_OFFSET(n)] & (0x1 <<TF_BIT(n)))
#define SM_SET_NTH_T_FLAG(sm,n,f) (SM_NTH_FLAGS(sm,f)[TF_OFFSET(n)] |= (0x1<<TF_BIT(n)))
#define SM_CLEAR_NTH_T_FLAG(sm,n,f) (SM_NTH_FLAGS(sm,f)[TF_OFFSET(n)] &= ~(0x1<<TF_BIT(n)))

#define SM_IS_NTH_T_ACTIVE(sm,n)     SM_IS_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_IS_NTH_T_BASE(sm,n)       SM_IS_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_IS_NTH_T_NEW(sm,n)        SM_IS_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_IS_NTH_T_LRU(sm,n)        SM_IS_NTH_T_FLAG(sm,n,T_LRU_FLAG)

#define SM_SET_NTH_T_ACTIVE(sm,n)    SM_SET_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_SET_NTH_T_BASE(sm,n)      SM_SET_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_SET_NTH_T_NEW(sm,n)    SM_SET_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_SET_NTH_T_LRU(sm,n)       SM_SET_NTH_T_FLAG(sm,n,T_LRU_FLAG)

#define SM_CLEAR_NTH_T_ACTIVE(sm,n)  SM_CLEAR_NTH_T_FLAG(sm,n,T_ACTIVE_FLAG)
#define SM_CLEAR_NTH_T_BASE(sm,n)    SM_CLEAR_NTH_T_FLAG(sm,n,T_BASE_FLAG)
#define SM_CLEAR_NTH_T_NEW(sm,n)  SM_CLEAR_NTH_T_FLAG(sm,n,T_NEW_FLAG)
#define SM_CLEAR_NTH_T_LRU(sm,n)     SM_CLEAR_NTH_T_FLAG(sm,n,T_LRU_FLAG)

#define SM_NTH_TRI(m,n)               (&(SM_TRIS(m)[(n)]))
#define SM_NTH_VERT(m,n)              (SM_VERTS(m)[(n)])
#define SM_FREE_TRI_ID(m,n) (n = SM_FREE_TRIS(m)==-1? \
    (SM_TRI_CNT(m)<SM_MAX_TRIS(m)?SM_TRI_CNT(m)++:-1):(n=SM_FREE_TRIS(m), \
     SM_FREE_TRIS(m)=T_NEXT_FREE(SM_NTH_TRI(m,SM_FREE_TRIS(m))),n))
 
#define SM_SAMP_BASE(m)               RS_BASE(SM_SAMP(m))
#define SM_SAMP_FREE(m)               RS_FREE(SM_SAMP(m))
#define SM_SAMP_EOL(m)                RS_EOL(SM_SAMP(m))
#define SM_MAX_SAMP(m)                RS_MAX_SAMP(SM_SAMP(m))
#define SM_MAX_AUX_PT(m)              RS_MAX_AUX_PT(SM_SAMP(m))
#define SM_NTH_WV(m,i)                RS_NTH_W_PT(SM_SAMP(m),i)
#define SM_NTH_W_DIR(m,i)             RS_NTH_W_DIR(SM_SAMP(m),i)
#define SM_NTH_RGB(m,i)               RS_NTH_RGB(SM_SAMP(m),i)
#define SM_RGB(m)                    RS_RGB(SM_SAMP(m))
#define SM_BRT(m)                     RS_BRT(SM_SAMP(m))
#define SM_NTH_BRT(m,i)               RS_NTH_BRT(SM_SAMP(m),i)
#define SM_CHR(m)                     RS_CHR(SM_SAMP(m))
#define SM_NTH_CHR(m,i)               RS_NTH_CHR(SM_SAMP(m),i)
#define SM_NUM_SAMP(m)               RS_NUM_SAMP(SM_SAMP(m))
#define SM_TONE_MAP(m)               RS_TONE_MAP(SM_SAMP(m))

#define SM_ALLOWED_VIEW_CHANGE(m) (SM_NUM_SAMP(m)/smDist_sum*SM_VIEW_FRAC)

#define SM_FOR_ALL_FLAGGED_TRIS(m,i,w,b) for(i=0,i=smNext_tri_flag_set(m,i,w,b);i < SM_TRI_CNT(m);i++,i=smNext_tri_flag_set(m,i,w,b))

#define SM_FOR_ALL_ACTIVE_TRIS(m,i) SM_FOR_ALL_FLAGGED_TRIS(m,i,T_ACTIVE_FLAG,0)
#define SM_FOR_ALL_NEW_TRIS(m,i) SM_FOR_ALL_FLAGGED_TRIS(m,i,T_NEW_FLAG,0)
#define SM_FOR_ALL_BASE_TRIS(m,i) SM_FOR_ALL_FLAGGED_TRIS(m,i,T_BASE_FLAG,0)
#define SM_FOR_ALL_VALID_TRIS(m,i) for((i)=0,(i)=smNext_valid_tri(m,i);(i)< \
SM_TRI_CNT(m); (i)++,(i)= smNext_valid_tri(m,i))

#define SM_FOR_ALL_ACTIVE_FG_TRIS(m,i) SM_FOR_ALL_FLAGGED_TRIS(m,i,T_ACTIVE_FLAG,1)

#define SM_FOR_ALL_ACTIVE_BG_TRIS(m,i) SM_FOR_ALL_FLAGGED_TRIS(m,i,T_ACTIVE_FLAG,2)


#define SM_FOR_ALL_ADJACENT_TRIS(sm,id,t) for(t=smTri_next_ccw_nbr(sm,t,id); \
t!=SM_NTH_TRI(sm,SM_NTH_VERT(sm,id)); t=smTri_next_ccw_nbr(sm,t,id))

#define SM_INVALID_SAMP_ID(sm,id)  (((id) < 0) || ((id) >= SM_MAX_SAMP(sm)))
#define SM_INVALID_POINT_ID(sm,id)  (((id) < 0) || ((id) >= SM_MAX_AUX_PT(sm)))
#define SM_T_CENTROID(sm,t,c)  tri_centroid(SM_NTH_WV(sm,T_NTH_V(t,0)),SM_NTH_WV(sm,T_NTH_V(t,1)),SM_NTH_WV(sm,T_NTH_V(t,2)),c)
#define SM_T_NTH_WV(sm,t,i)  (SM_NTH_WV(sm,T_NTH_V(t,i)))

#define SM_BASE_ID(s,i)  \
 ((i) >= RS_MAX_SAMP(SM_SAMP(s)) && (i) < RS_MAX_AUX_PT(SM_SAMP(s)))

#define SM_BG_SAMPLE(sm,i)   (SM_NTH_W_DIR(sm,i)==-1)

#define SM_BG_TRI(sm,i)  (SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),0)) || \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),1)) || \
			  SM_BG_SAMPLE(sm,T_NTH_V(SM_NTH_TRI(sm,i),2)))

#define SM_FOR_ALL_SAMPLES(sm,i) for((i)=0;i < SM_NUM_SAMP(sm);(i)++)

typedef struct _T_DEPTH {
  int tri;
  double depth;
}T_DEPTH;


extern SM *smMesh;
extern int smNew_tri_cnt;
extern double smDist_sum;

#ifdef TEST_DRIVER
extern VIEW View;
extern VIEW Current_View;
extern int Pick_tri,Picking;
extern FVECT Pick_point[500],Pick_origin,Pick_dir;
extern FVECT Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern FVECT P0,P1,P2;
extern int Pick_cnt;
extern FVECT FrustumNear[4],FrustumFar[4];
#endif

#ifdef DEBUG
extern int Malloc_cnt;
#endif
#endif








