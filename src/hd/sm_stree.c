/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * sm_stree.c
 */
#include "standard.h"
#include "sm_geom.h"
#include "sm_stree.h"

/* Define 4 vertices on the sphere to create a tetrahedralization on 
   the sphere: triangles are as follows:
        (2,1,0),(3,2,0), (1,3,0), (2,3,1)
*/

#ifdef TEST_DRIVER
extern FVECT Pick_point[500],Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern int Pick_cnt;
#endif
FVECT stDefault_base[4] = { {SQRT3_INV, SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, -SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, SQRT3_INV, -SQRT3_INV},
			  {SQRT3_INV, -SQRT3_INV, -SQRT3_INV}};
int stTri_verts[4][3] = { {2,1,0},{3,2,0},{1,3,0},{2,3,1}};
int stTri_nbrs[4][3] =  { {2,1,3},{0,2,3},{1,0,3},{2,0,1}};

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
stPoint_locate(st,npt)
    STREE *st;
    FVECT npt;
{
    int i,d,j,id;
    QUADTREE *rootptr,*qtptr;
    FVECT v1,v2,v3;
    OBJECT os[QT_MAXSET+1],*optr;
    FVECT p0,p1,p2;

    /* Test each of the root triangles against point id */
    for(i=0; i < 4; i++)
     {
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,v1,v2,v3);
	 /* Return tri that p falls in */
	 qtptr = qtRoot_point_locate(rootptr,v1,v2,v3,npt,NULL,NULL,NULL);
	 if(!qtptr || QT_IS_EMPTY(*qtptr))
	    continue;
	 /* Get the set */
	 optr = qtqueryset(*qtptr);
	 for (j = QT_SET_CNT(optr),optr = QT_SET_PTR(optr);j > 0; j--)
         {
	     /* Find the first triangle that pt falls */
	     id = QT_SET_NEXT_ELEM(optr);
	     qtTri_from_id(id,p0,p1,p2,NULL,NULL,NULL,NULL,NULL,NULL);
	     d = point_in_stri(p0,p1,p2,npt);  
	     if(d)
	       return(id);
	 }
     }
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
	 /* Return quadtree tri that p falls in */
	 qtptr = qtRoot_point_locate(rootptr,v0,v1,v2,p,t0,t1,t2);
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
    found |= qtRoot_add_tri(rootptr,t0,t1,t2,v0,v1,v2,id,0);
  }
  return(found);
}

int
stApply_to_tri_cells(st,v0,v1,v2,func,arg)
STREE *st;
FVECT v0,v1,v2;
int (*func)();
int *arg;
{
  int i,found;
  QUADTREE *rootptr;
  FVECT t0,t1,t2;

  found = 0;
  func(ST_ROOT_PTR(st),arg);
  QT_SET_FLAG(ST_ROOT(st));
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

int
stVisit_tri_edges(st,t0,t1,t2,func,arg1,arg2,arg3)
   STREE *st;
   FVECT t0,t1,t2;
   int (*func)();
   int *arg1,arg2,*arg3;
{
    int id,i,w;
    QUADTREE *rootptr;
    FVECT q0,q1,q2,v[3],i_pt;

    VCOPY(v[0],t0); VCOPY(v[1],t1); VCOPY(v[2],t2);
    w = -1;
    QT_SET_FLAG(ST_ROOT(st));
    for(i=0; i < 4; i++)
     {
#ifdef TEST_DRIVER 
Pick_cnt = 0;
#endif
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,q0,q1,q2);
	 /* Return quadtree tri that p falls in */
	 if(!point_in_stri(q0,q1,q2,v[0]))  
	   continue;
#ifdef TEST_DRIVER
	 id = qtRoot_visit_tri_edges(rootptr,q0,q1,q2,v,i_pt,&w,
				      func,arg1,arg2,arg3);
#else
	 id = qtRoot_visit_tri_edgesi(rootptr,q0,q1,q2,v,i_pt,&w,
				      func,arg1,arg2,arg3);
#endif
	 if(id == INVALID)
	 {
#ifdef DEBUG
	   eputs("stVisit_tri_edges(): Unable to trace edges\n");
#endif
	   return(INVALID);
	 }
	 if(id == QT_DONE)
	    return(*arg1);
	
	 /* Crossed over to next cell: id = nbr */
	 while(1) 
	 {
	     /* test if ray crosses plane between this quadtree triangle and
		its neighbor- if it does then find intersection point with 
		ray and plane- this is the new origin
		*/
	   i = stTri_nbrs[i][id];
	   rootptr = ST_NTH_ROOT_PTR(st,i);
	   stNth_base_verts(st,i,q0,q1,q2);
#ifdef TEST_DRIVER
	   id=qtRoot_visit_tri_edges(rootptr,q0,q1,q2,v,i_pt,&w,
				      func,arg1,arg2,arg3);
#else
	   id=qtRoot_visit_tri_edgesi(rootptr,q0,q1,q2,v,i_pt,&w,
				      func,arg1,arg2,arg3);
#endif
	   if(id == QT_DONE)
	     return(*arg1);
	   if(id == INVALID)
	     {
#ifdef DEBUG
	     eputs("stVisit_tri_edges(): point not found\n");
#endif	
	     return(INVALID);
	     }
	   
       	 }
     }    /* Point not found */
    return(INVALID); 
}

int
stTrace_ray(st,orig,dir,func,arg1,arg2)
   STREE *st;
   FVECT orig,dir;
   int (*func)();
   int *arg1,arg2;
{
    int id,i;
    QUADTREE *rootptr;
    FVECT q0,q1,q2,o,n;
    double pd,t;

    VCOPY(o,orig);
    for(i=0; i < 4; i++)
     {
#ifdef TEST_DRIVER 
Pick_cnt = 0;
#endif
	 rootptr = ST_NTH_ROOT_PTR(st,i);
	 stNth_base_verts(st,i,q0,q1,q2);
	 /* Return quadtree tri that p falls in */
	 id= qtRoot_trace_ray(rootptr,q0,q1,q2,o,dir,func,arg1,arg2);
	 if(id == INVALID)
	   continue;
	 if(id == QT_DONE)
	    return(*arg1);
	
	 /* Crossed over to next cell: id = nbr */
	 while(1) 
	 {
	     /* test if ray crosses plane between this quadtree triangle and
		its neighbor- if it does then find intersection point with 
		ray and plane- this is the new origin
		*/
	   if(id==0)
	     VCROSS(n,q1,q2);
	   else
	     if(id==1)
	       VCROSS(n,q2,q0);
	   else
	     VCROSS(n,q0,q1);

	   /* Ray does not cross into next cell: done and tri not found*/
	   if(!intersect_ray_plane(orig,dir,n,0.0,NULL,o))
	     return(INVALID);

	   VSUM(o,o,dir,10*FTINY);
	   i = stTri_nbrs[i][id];
	   rootptr = ST_NTH_ROOT_PTR(st,i);
	   stNth_base_verts(st,i,q0,q1,q2);
	   id = qtRoot_trace_ray(rootptr,q0,q1,q2,o,dir,func,arg1,arg2);
	   if(id == QT_DONE)
	     return(*arg1);
	   if(id == INVALID)
	     return(INVALID);
	   
       	 }
     }    /* Point not found */
    return(INVALID);
}



stVisit_tri_interior(st,t0,t1,t2,func,arg1,arg2,arg3)
   STREE *st;
   FVECT t0,t1,t2;
   int (*func)();
   int *arg1,arg2,*arg3;
{
  int i;
  QUADTREE *rootptr;
  FVECT q0,q1,q2;

  for(i=0; i < 4; i++)
  {
    rootptr = ST_NTH_ROOT_PTR(st,i);
    stNth_base_verts(st,i,q0,q1,q2); 
    qtVisit_tri_interior(rootptr,q0,q1,q2,t0,t1,t2,0,func,arg1,arg2,arg3);
  }
}


int
stApply_to_tri(st,t0,t1,t2,edge_func,interior_func,arg1,arg2)
   STREE *st;
   FVECT t0,t1,t2;
   int (*edge_func)(),(*interior_func)();
   int arg1,*arg2;
{
    int f;
    FVECT dir;
    
  /* First add all of the leaf cells lying on the triangle perimeter:
     mark all cells seen on the way
   */
    f = 0;
    /* Visit cells along edges of the tri */

    stVisit_tri_edges(st,t0,t1,t2,edge_func,&f,arg1,arg2);

    /* Now visit interior */
    if(QT_FLAG_FILL_TRI(f) || QT_FLAG_UPDATE(f))
       stVisit_tri_interior(st,t0,t1,t2,interior_func,&f,arg1,arg2);
}





