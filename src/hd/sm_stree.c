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
    QUADTREE *rootptr,qt;
    FVECT v1,v2,v3;
    OBJECT os[MAXSET+1],*optr;
    FVECT p0,p1,p2;
    char w;
    
    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v1,v2,v3);
	 /* Return tri that p falls in */
	 qt = qtPoint_locate(rootptr,v1,v2,v3,npt,type,which,p0,p1,p2);
	 if(QT_IS_EMPTY(qt))
	    continue;
	 /* Get the set */
	 qtgetset(os,qt);
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

int
stPoint_locate_cell(st,p,p0,p1,p2,type,which)
    STREE *st;
    FVECT p,p0,p1,p2;
    char *type,*which;
{
    int i,d;
    QUADTREE *rootptr,qt;
    FVECT v0,v1,v2;

    
    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v0,v1,v2);
	 /* Return tri that p falls in */
	 qt = qtPoint_locate(rootptr,v0,v1,v2,p,type,which,p0,p1,p2);
	 /* NOTE: For now return only one triangle */
	 if(!QT_IS_EMPTY(qt))
	    return(qt);
     }    /* Point not found */
    if(which)
      *which = 0;
    if(type)
      *type = 0;
    return(EMPTY);
}

int
stAdd_tri(st,id,v1,v2,v3)
STREE *st;
int id;
FVECT v1,v2,v3;
{
  
  int i,found;
  QUADTREE *rootptr;
  FVECT t1,t2,t3;
  
  found = 0;
  for(i=0; i < 4; i++)
  {
    rootptr = ST_NTH_ROOT_PTR(st,i);
    stNth_base_verts(st,i,t1,t2,t3);
    found |= qtAdd_tri(rootptr,id,v1,v2,v3,t1,t2,t3,0);
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





