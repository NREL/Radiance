/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm_stree.h - header file for spherical quadtree code:
 *             
 */

#include "sm_qtree.h"

#define SQRT3_INV 0.5773502692


typedef struct _STREE {
    QUADTREE   root;       /* quadtree triangulation of sphere */
    FVECT    center;   /* sphere center */
    FVECT    base[4];  /* 4 vertices on sphere that define base triangulation
			of 4 triangles: assume cover sphere and triangles
			are base verts (0,1,2),(0,2,3),(0,3,1), and (1,3,2)
			*/
}STREE;

#define ST_ROOT(s)             ((s)->root)
#define ST_ROOT_PTR(s)             (&(s)->root)
#define ST_NTH_ROOT(s,n)       QT_NTH_CHILD(ST_ROOT(s),n)
#define ST_NTH_ROOT_PTR(s,n)   QT_NTH_CHILD_PTR(ST_ROOT(s),n)
#define ST_CLEAR_ROOT(s)       QT_CLEAR_CHILDREN(ST_ROOT(s))

#define ST_CENTER(s)           ((s)->center)
#define ST_SET_CENTER(s,b)     VCOPY(ST_CENTER(s),b)
#define ST_BASE(s)             ((s)->base)
#define ST_NTH_BASE(s,n)       ((s)->base[(n)])
#define ST_SET_NTH_BASE(s,n,b) VCOPY(ST_NTH_BASE(s,n),b)
#define ST_SET_BASE(s,b)       (VCOPY(ST_NTH_BASE(s,0),(b)[0]), \
				VCOPY(ST_NTH_BASE(s,1),(b)[1]), \
				VCOPY(ST_NTH_BASE(s,2),(b)[2]), \
				VCOPY(ST_NTH_BASE(s,3),(b)[3]))
#define ST_COORD(s,p,r)         VSUB(r,p,ST_CENTER(s))
#define ST_CLEAR_FLAGS(s)       qtClearAllFlags()
/* STREE functions


   STREE *stInit(STREE *st)
           Initialize STREE: if st = NULL, allocate a new one, else clear
	   return pointer to initialized structure

   QUADTREE *stPoint_locate(STREE *st,FVECT pt)
           Find stree node that projection of pt on sphere falls in 

   stInsert_tri()
          for every quadtree tri in the base- find node all leaf nodes that
	  tri overlaps and add tri to set. If this causes any of the nodes
	  to be over threshhold- split
   stDelete_tri()
          for every quadtree tri in the base- find node all leaf nodes that
	  tri overlaps. If this causes any of the nodes to be under
	  threshold- merge
*/
extern int   stTri_verts[4][3];
extern int   stTri_nbrs[4][3];
extern FVECT stDefault_base[4];


extern STREE *stAlloc();
extern QUADTREE *stPoint_locate_cell();






