/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm_stree.h - header file for spherical quadtree code:
 *             
 */

#define STR_INDEX(s)  (stRoot_indices[(s)])
#define STR_NTH_INDEX(s,n)  (stRoot_indices[(s)][(n)])

#define ST_NUM_ROOT_NODES 8



/* The base is an octahedron: Each face contains a planar quadtree. At
 the root level, the "top" (positive y) four faces, and bottom four faces
 are stored together:forming two root quadtree nodes
 */

typedef struct _STREE {
  QUADTREE   qt[2];  /* root[0]= top four faces, root[1]=bottom 4 faces*/
  FVECT      center;   /* sphere center */

  /* will go **************************************************/
  FVECT      base[6];  /* 6 vertices on sphere that define base octahedron:
			  in canonical form: origin(0,0,0) points (1,0,0),
			  (0,1,0),(0,0,1),(-1,0,0),(0,-1,0),(0,0,-1) */
  FPEQ      fplane[8];     /* Face plane equations */
        
  FVECT enorms[8][3];  /* Edge normals: For plane through edge and origin*/
  /* gone ****************************************************************/

}STREE;


#define ST_BASEI(n)            ((n)>>2)     /* root index: top or bottom */
#define ST_INDEX(n)            ((n) & 0x3) /* which child in root */
#define ST_QT(s,i)           ((s)->qt[ST_BASEI(i)]) /* top or bottom root*/
#define ST_QT_PTR(s,i)       (&ST_QT(s,i)) /* ptr to top(0)/bottom(1)root*/
#define ST_TOP_QT(s)         ((s)->qt[0])  /* top root (y>0)*/
#define ST_BOTTOM_QT(s)      ((s)->qt[1])  /* bottom qt (y <= 0)*/
#define ST_TOP_QT_PTR(s)     (&ST_TOP_QT(s)) /* ptr to top qt */
#define ST_BOTTOM_QT_PTR(s)  (&ST_BOTTOM_QT(s)) /* ptr to bottom qt*/


#define ST_ROOT_QT(s,n)        QT_NTH_CHILD(ST_QT(s,n),ST_INDEX(n))

#define ST_CLEAR_QT(st)      (ST_TOP_QT(st)=EMPTY,ST_BOTTOM_QT(st)=EMPTY)
#define ST_INIT_QT(st)      (QT_CLEAR_CHILDREN(ST_TOP_QT(st)), \
				QT_CLEAR_CHILDREN(ST_BOTTOM_QT(st)))



     /* Will go *****************************************************/
#define ST_BASE(s)             ((s)->base)
#define ST_NTH_BASE(s,n)       ((s)->base[(n)])
#define ST_NTH_V(s,n,i)        ST_NTH_BASE(s,stBase_verts[(n)][(i)])
#define ST_SET_NTH_BASE(s,n,b) VCOPY(ST_NTH_BASE(s,n),b)
#define ST_SET_BASE(s,b)       (VCOPY(ST_NTH_BASE(s,0),(b)[0]), \
				VCOPY(ST_NTH_BASE(s,1),(b)[1]), \
				VCOPY(ST_NTH_BASE(s,2),(b)[2]), \
				VCOPY(ST_NTH_BASE(s,3),(b)[3]), \
				VCOPY(ST_NTH_BASE(s,4),(b)[4]), \
				VCOPY(ST_NTH_BASE(s,5),(b)[5]))
#define ST_NTH_PLANE(s,i)        ((s)->fplane[(i)])
#define ST_NTH_NORM(s,i)         (ST_NTH_PLANE(s,i).n)
#define ST_NTH_D(s,i)            (ST_NTH_PLANE(s,i).d)
#define ST_EDGE_NORM(s,i,n)      ((s)->enorms[(i)][(n)])
/* gone *****************************************************/

#define ST_CENTER(s)           ((s)->center)
#define ST_SET_CENTER(s,b)     VCOPY(ST_CENTER(s),b)

#define ST_COORD(s,p,r)         VSUB(r,p,ST_CENTER(s))
#define ST_CLEAR_FLAGS(s)       qtClearAllFlags()

/* Point location based on coordinate signs */ 
#define stLocate_root(p) (((p)[2]>0.0?0:4)|((p)[1]>0.0?0:2)|((p)[0]>0.0?0:1))

#define ST_CLIP_VERTS 16
/* STREE functions
void stInit(STREE *st,FVECT  center)
          Initializes an stree structure with origin 'center': 
          Frees existing quadtrees hanging off of the roots

STREE *stAlloc(STREE *st)
         Allocates a stree structure  and creates octahedron base 

void stClear(STREE *st)
         Frees any existing root children and clears roots
	 
QUADTREE stPoint_locate(STREE *st,FVECT p)
         Returns quadtree leaf node containing point 'p'. 

int stAdd_tri(STREE *st,int id,FVECT t0,t1,t2)
         Add triangle 'id' with coordinates 't0,t1,t2' to the stree: returns
         FALSE on error, TRUE otherwise
	 
int stRemove_tri(STREE *st,int id,FVECT t0,t1,t2)
         Removes triangle 'id' with coordinates 't0,t1,t2' from stree: returns
         FALSE on error, TRUE otherwise
	 
int stTrace_ray(STREE *st,FVECT orig,dir,int (*func)(),int *arg1,*arg2)
        Trace ray 'orig-dir' through stree and apply 'func(arg1,arg2)' at each
        node that it intersects

int stApply_to_tri(STREE *st,FVECT t0,t1,t2,int (*edge_func)(),
        (*tri_func)(),int arg1,*arg2)	
   Visit nodes intersected by tri 't0,t1,t2'.Apply 'edge_func(arg1,arg2,arg3)',
   to those nodes intersected by edges, and interior_func to ALL nodes:
   ie some Nodes  will be visited more than once
*/

extern int stBase_verts[8][3];
extern STREE *stAlloc();
extern QUADTREE stPoint_locate();





