/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * sm_stree.c
 */
#include "standard.h"
#include "object.h"

#include "sm_geom.h"
#include "sm_stree.h"


/* Define 4 vertices on the sphere to create a tetrahedralization on 
   the sphere: triangles are as follows:
        (0,1,2),(0,2,3), (0,3,1), (1,3,2)
*/

FVECT stDefault_base[4] = { {SQRT3_INV, SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, -SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, SQRT3_INV, -SQRT3_INV},
			  {SQRT3_INV, -SQRT3_INV, -SQRT3_INV}};
int stTri_verts[4][3] = { {2,1,0}, 
                          {3,2,0}, 
			  {1,3,0}, 
		          {2,3,1}};

stNth_base_verts(st,i,v1,v2,v3)
STREE *st;
int i;
FVECT v1,v2,v3;
{
  VCOPY(v1,ST_NTH_BASE(st,stTri_verts[i][0]));
  VCOPY(v2,ST_NTH_BASE(st,stTri_verts[i][1]));
  VCOPY(v3,ST_NTH_BASE(st,stTri_verts[i][2]));
}

/* Frees the 4 quadtrees rooted at st */
stClear(st)
STREE *st;
{
  int i;

  /* stree always has 4 children corresponding to the base tris
  */
  for (i = 0; i < 4; i++)
    qtFree(ST_NTH_ROOT(st, i));

  QT_CLEAR_CHILDREN(ST_ROOT(st));

}


STREE
*stInit(st,center,base)
STREE *st;
FVECT  center,base[4];
{

  if(base)
    ST_SET_BASE(st,base);
  else
    ST_SET_BASE(st,stDefault_base);

  ST_SET_CENTER(st,center);
  stClear(st);

  return(st);
}


/* "base" defines 4 vertices on the sphere to create a tetrahedralization on 
     the sphere: triangles are as follows:(0,1,2),(0,2,3), (0,3,1), (1,3,2)
     if base is null: does default.

 */
STREE
*stAlloc(st)
STREE *st;
{
  int i;

  if(!st)
    st = (STREE *)malloc(sizeof(STREE));

  ST_ROOT(st) = qtAlloc();
    
  QT_CLEAR_CHILDREN(ST_ROOT(st));

  return(st);
}


/* Find location of sample point in the DAG and return lowest level
   containing triangle. "type" indicates whether the point was found
   to be in interior to the triangle: GT_FACE, on one of its
   edges GT_EDGE or coinciding with one of its vertices GT_VERTEX.
   "which" specifies which vertex (0,1,2) or edge (0=v0v1, 1 = v1v2, 2 = v21) 
*/
int
stPoint_locate(st,npt,type,which)
    STREE *st;
    FVECT npt;
    char *type,*which;
{
    int i,d,j,id;
    QUADTREE *rootptr,*qtptr;
    FVECT v1,v2,v3;
    OBJECT os[MAXSET+1],*optr;
    char w;
    FVECT p0,p1,p2;

    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v1,v2,v3);
	 /* Return tri that p falls in */
	 qtptr = qtRoot_point_locate(rootptr,v1,v2,v3,npt,NULL,NULL,NULL);
	 if(!qtptr)
	    continue;
	 /* Get the set */
	 qtgetset(os,*qtptr);
	 for (j = QT_SET_CNT(os),optr = QT_SET_PTR(os); j > 0; j--)
         {
	     /* Find the first triangle that pt falls */
	     id = QT_SET_NEXT_ELEM(optr);
	     qtTri_verts_from_id(id,p0,p1,p2);
	     d = test_single_point_against_spherical_tri(p0,p1,p2,npt,&w);  
	     if(d)
		{
		    if(type)
		       *type = d;
		    if(which)
		       *which = w;
		    return(id);
		}
	 }
     }
    if(which)
      *which = 0;
    if(type)
      *type = 0;
    return(EMPTY);
}

QUADTREE
*stPoint_locate_cell(st,p,t0,t1,t2)
    STREE *st;
    FVECT p;
    FVECT t0,t1,t2;
{
    int i,d;
    QUADTREE *rootptr,*qtptr;
    FVECT v0,v1,v2;

    
    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v0,v1,v2);
	 /* Return tri that p falls in */
	 qtptr = qtRoot_point_locate(rootptr,v0,v1,v2,p,t0,t1,t2);
	 /* NOTE: For now return only one triangle */
	 if(qtptr)
	    return(qtptr);
     }    /* Point not found */
    return(NULL);
}


QUADTREE
*stAdd_tri_from_pt(st,p,t_id)
    STREE *st;
    FVECT p;
    int t_id;
{
    int i,d;
    QUADTREE *rootptr,*qtptr;
    FVECT v0,v1,v2;

    
    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v0,v1,v2);
	 /* Return tri that p falls in */
	 qtptr = qtRoot_add_tri_from_point(rootptr,v0,v1,v2,p,t_id);
	 /* NOTE: For now return only one triangle */
	 if(qtptr)
	    return(qtptr);
     }    /* Point not found */
    return(NULL);
}

int
stAdd_tri(st,id,v0,v1,v2)
STREE *st;
int id;
FVECT v0,v1,v2;
{
  
  int i,found;
  QUADTREE *rootptr;
  FVECT t0,t1,t2;

  found = 0;
  for(i=0; i < 4; i++)
  {
    rootptr = ST_NTH_ROOT_PTR(st,i);
    stNth_base_verts(st,i,t0,t1,t2);
    found |= qtRoot_add_tri(rootptr,id,v0,v1,v2,t0,t1,t2,0);
  }
  return(found);
}

int
stApply_to_tri_cells(st,v0,v1,v2,func,arg)
STREE *st;
FVECT v0,v1,v2;
int (*func)();
char *arg;
{
  int i,found;
  QUADTREE *rootptr;
  FVECT t0,t1,t2;

  found = 0;
  for(i=0; i < 4; i++)
  {
    rootptr = ST_NTH_ROOT_PTR(st,i);
    stNth_base_verts(st,i,t0,t1,t2); 
    found |= qtApply_to_tri_cells(rootptr,v0,v1,v2,t0,t1,t2,func,arg);
  }
  return(found);
}




int
stRemove_tri(st,id,v0,v1,v2)
STREE *st;
int id;
FVECT v0,v1,v2;
{
  
  int i,found;
  QUADTREE *rootptr;
  FVECT t0,t1,t2;

  found = 0;
  for(i=0; i < 4; i++)
  {
    rootptr = ST_NTH_ROOT_PTR(st,i);
    stNth_base_verts(st,i,t0,t1,t2); 
   found |= qtRemove_tri(rootptr,id,v0,v1,v2,t0,t1,t2);
  }
  return(found);
}





#if 0
int
stAdd_tri_opt(st,id,v0,v1,v2)
STREE *st;
int id;
FVECT v0,v1,v2;
{
  
  int i,found;
  QUADTREE *qtptr;
  FVECT pt,t0,t1,t2;

  /* First add all of the leaf cells lying on the triangle perimeter:
     mark all cells seen on the way
   */
  /* clear all of the flags */
  qtClearAllFlags();		/* clear all quadtree branch flags */

  /* Now trace each triangle edge-marking cells visited, and adding tri to
     the leafs
   */
  stAdd_tri_from_pt(st,v0,id,t0,t1,t2);
  /* Find next cell that projection of ray intersects */
  VCOPY(pt,v0);
  /* NOTE: Check if in same cell */
  while(traceEdge(pt,v1,t0,t1,t2,pt))
  {
      stAdd_tri_from_pt(st,pt,id,t0,t1,t2);
      traceEdge(pt,v1,t0,t1,t2,pt);
  }
  while(traceEdge(pt,v2,t0,t1,t2,pt))
  {
      stAdd_tri_from_pt(st,pt,id,t0,t1,t2);
      traceEdge(pt,v2,t0,t1,t2,pt);
  }
  while(traceEdge(pt,v0,t0,t1,t2,pt))
  {
      stAdd_tri_from_pt(st,pt,id,t0,t1,t2);
      traceEdge(pt,v2,t0,t1,t2,pt);
  }

  /* NOTE: Optimization: if <= 2 cells added: dont need to fill */
  /* Traverse: follow nodes with flag set or one vertex in triangle */
  
}

#endif
