/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * sm_stree.c
 *  An stree (spherical quadtree) is defined by an octahedron in 
 *  canonical form,and a world center point. Each face of the 
 *  octahedron is adaptively subdivided as a planar triangular quadtree.
 *  World space geometry is projected onto the quadtree faces from the
 *  sphere center.
 */
#include "standard.h"
#include "sm_flag.h"
#include "sm_geom.h"
#include "sm_qtree.h"
#include "sm_stree.h"

#ifdef TEST_DRIVER
extern FVECT Pick_point[500],Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern int Pick_cnt;
#endif
/* octahedron coordinates */
FVECT stDefault_base[6] = {  {1.,0.,0.},{0.,1.,0.}, {0.,0.,1.},
			    {-1.,0.,0.},{0.,-1.,0.},{0.,0.,-1.}};
/* octahedron triangle vertices */
int stBase_verts[8][3] = { {2,1,0},{1,5,0},{5,1,3},{1,2,3},
			  {4,2,0},{4,0,5},{3,4,5},{4,3,2}};
/* octahedron triangle nbrs ; nbr i is the face opposite vertex i*/
int stBase_nbrs[8][3] =  { {1,4,3},{5,0,2},{3,6,1},{7,2,0},
			  {0,5,7},{1,6,4},{5,2,7},{3,4,6}};
/* look up table for octahedron point location */
int stlocatetbl[8] = {6,7,2,3,5,4,1,0};


/* Initializes an stree structure with origin 'center': 
   Frees existing quadtrees hanging off of the roots
*/
stInit(st)
STREE *st;
{
    ST_TOP_ROOT(st) = qtAlloc();
    ST_BOTTOM_ROOT(st) = qtAlloc();
    ST_INIT_ROOT(st);
}

/* Frees the children of the 2 quadtrees rooted at st,
   Does not free root nodes: just clears
*/
stClear(st)
   STREE *st;
{
    qtDone();
    stInit(st);
}

/* Allocates a stree structure  and creates octahedron base */
STREE
*stAlloc(st)
STREE *st;
{
  int i,m;
  FVECT v0,v1,v2;
  FVECT n;
  
  if(!st)
    if(!(st = (STREE *)malloc(sizeof(STREE))))
       error(SYSTEM,"stAlloc(): Unable to allocate memory\n");

  /* Allocate the top and bottom quadtree root nodes */
  stInit(st);
  
  /* Set the octahedron base */
  ST_SET_BASE(st,stDefault_base);

  /* Calculate octahedron face and edge normals */
  for(i=0; i < ST_NUM_ROOT_NODES; i++)
  {
      VCOPY(v0,ST_NTH_V(st,i,0));
      VCOPY(v1,ST_NTH_V(st,i,1));
      VCOPY(v2,ST_NTH_V(st,i,2));
      tri_plane_equation(v0,v1,v2, &ST_NTH_PLANE(st,i),FALSE);
      m = max_index(FP_N(ST_NTH_PLANE(st,i)),NULL);
      FP_X(ST_NTH_PLANE(st,i)) = (m+1)%3; 
      FP_Y(ST_NTH_PLANE(st,i)) = (m+2)%3;
      FP_Z(ST_NTH_PLANE(st,i)) = m;
      VCROSS(ST_EDGE_NORM(st,i,0),v0,v1);
      VCROSS(ST_EDGE_NORM(st,i,1),v1,v2);
      VCROSS(ST_EDGE_NORM(st,i,2),v2,v0);
  }
  return(st);
}


/* Return quadtree leaf node containing point 'p'*/
QUADTREE
stPoint_locate(st,p)
    STREE *st;
    FVECT p;
{
    int i;
    QUADTREE root,qt;

    /* Find root quadtree that contains p */
    i = stPoint_in_root(p);
    root = ST_NTH_ROOT(st,i);
    
    /* Traverse quadtree to leaf level */
    qt = qtRoot_point_locate(root,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
			ST_NTH_V(st,i,2),ST_NTH_PLANE(st,i),p);
    return(qt);
}

/* Add triangle 'id' with coordinates 't0,t1,t2' to the stree: returns
   FALSE on error, TRUE otherwise
*/

stAdd_tri(st,id,t0,t1,t2)
STREE *st;
int id;
FVECT t0,t1,t2;
{
  int i;
  QUADTREE root;

  for(i=0; i < ST_NUM_ROOT_NODES; i++)
  {
    root = ST_NTH_ROOT(st,i);
    ST_NTH_ROOT(st,i) = qtRoot_add_tri(root,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
			    ST_NTH_V(st,i,2),t0,t1,t2,id,0);
  }
}

/* Remove triangle 'id' with coordinates 't0,t1,t2' to the stree: returns
   FALSE on error, TRUE otherwise
*/

stRemove_tri(st,id,t0,t1,t2)
STREE *st;
int id;
FVECT t0,t1,t2;
{
  int i;
  QUADTREE root;

  for(i=0; i < ST_NUM_ROOT_NODES; i++)
  {
    root = ST_NTH_ROOT(st,i);
    ST_NTH_ROOT(st,i)=qtRoot_remove_tri(root,id,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
			  ST_NTH_V(st,i,2),t0,t1,t2);
  }
}

/* Visit all nodes that are intersected by the edges of triangle 't0,t1,t2'
   and apply 'func'
*/

stVisit_tri_edges(st,t0,t1,t2,func,fptr,argptr)
   STREE *st;
   FVECT t0,t1,t2;
   int (*func)(),*fptr;
   int *argptr;
{
    int id,i,w,next;
    QUADTREE root;
    FVECT v[3],i_pt;

    VCOPY(v[0],t0); VCOPY(v[1],t1); VCOPY(v[2],t2);
    w = -1;

    /* Locate the root containing triangle vertex v0 */
    i = stPoint_in_root(v[0]);
    /* Mark the root node as visited */
    QT_SET_FLAG(ST_ROOT(st,i));
    root = ST_NTH_ROOT(st,i);
    
    ST_NTH_ROOT(st,i) = qtRoot_visit_tri_edges(root,ST_NTH_V(st,i,0),
       ST_NTH_V(st,i,1),ST_NTH_V(st,i,2),ST_NTH_PLANE(st,i),v,i_pt,&w,
					       &next,func,fptr,argptr);
    if(QT_FLAG_IS_DONE(*fptr))
      return;
	
    /* Crossed over to next node: id = nbr */
    while(1) 
    {
	/* test if ray crosses plane between this quadtree triangle and
	   its neighbor- if it does then find intersection point with 
	   ray and plane- this is the new start point
	   */
	i = stBase_nbrs[i][next];
	root = ST_NTH_ROOT(st,i);
	ST_NTH_ROOT(st,i) = 
	  qtRoot_visit_tri_edges(root,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
         ST_NTH_V(st,i,2),ST_NTH_PLANE(st,i),v,i_pt,&w,&next,func,fptr,argptr);
	if(QT_FLAG_IS_DONE(*fptr))
	  return;
    }
}

/* Trace ray 'orig-dir' through stree and apply 'func(qtptr,f,argptr)' at each
   node that it intersects
*/
int
stTrace_ray(st,orig,dir,func,argptr)
   STREE *st;
   FVECT orig,dir;
   int (*func)();
   int *argptr;
{
    int next,last,i,f=0;
    QUADTREE root;
    FVECT o,n,v;
    double pd,t,d;

    VCOPY(o,orig);
#ifdef TEST_DRIVER
       Pick_cnt=0;
#endif;
    /* Find the root node that o falls in */
    i = stPoint_in_root(o);
    root = ST_NTH_ROOT(st,i);
    
    ST_NTH_ROOT(st,i) = 
      qtRoot_trace_ray(root,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
	  ST_NTH_V(st,i,2),ST_NTH_PLANE(st,i),o,dir,&next,func,&f,argptr);

    if(QT_FLAG_IS_DONE(f))
      return(TRUE);
    
    d = DOT(orig,dir)/sqrt(DOT(orig,orig));
    VSUM(v,orig,dir,-d);
    /* Crossed over to next cell: id = nbr */
    while(1) 
      {
	/* test if ray crosses plane between this quadtree triangle and
	   its neighbor- if it does then find intersection point with 
	   ray and plane- this is the new origin
	   */
	if(next == INVALID)
	  return(FALSE);
#if 0
	if(!intersect_ray_oplane(o,dir,ST_EDGE_NORM(st,i,(next+1)%3),NULL,o))
#endif
	   if(DOT(o,v) < 0.0)
	   /* Ray does not cross into next cell: done and tri not found*/
	   return(FALSE);

	VSUM(o,o,dir,10*FTINY);
	i = stBase_nbrs[i][next];
	root = ST_NTH_ROOT(st,i);
	
	ST_NTH_ROOT(st,i) = 
	  qtRoot_trace_ray(root, ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),
	       ST_NTH_V(st,i,2),ST_NTH_PLANE(st,i),o,dir,&next,func,&f,argptr);
	if(QT_FLAG_IS_DONE(f))
	  return(TRUE);
      }
}


/* Visit nodes intersected by tri 't0,t1,t2' and apply 'func(arg1,arg2,arg3):
   assumes that stVisit_tri_edges has already been called such that all nodes
   intersected by tri edges are already marked as visited
*/
stVisit_tri(st,t0,t1,t2,func,f,argptr)
   STREE *st;
   FVECT t0,t1,t2;
   int (*func)(),*f;
   int *argptr;
{
  int i;
  QUADTREE root;
  FVECT n0,n1,n2;

  /* Calcuate the edge normals for tri */
  VCROSS(n0,t0,t1);
  VCROSS(n1,t1,t2);
  VCROSS(n2,t2,t0);

  for(i=0; i < ST_NUM_ROOT_NODES; i++)
  {
    root = ST_NTH_ROOT(st,i);
    ST_NTH_ROOT(st,i) = qtVisit_tri_interior(root,ST_NTH_V(st,i,0),
	ST_NTH_V(st,i,1),ST_NTH_V(st,i,2),t0,t1,t2,n0,n1,n2,0,func,f,argptr);

  }
}

/* Visit nodes intersected by tri 't0,t1,t2'.Apply 'edge_func(arg1,arg2,arg3)',
   to those nodes intersected by edges, and interior_func to ALL nodes:
   ie some Nodes  will be visited more than once
 */
int
stApply_to_tri(st,t0,t1,t2,edge_func,tri_func,argptr)
   STREE *st;
   FVECT t0,t1,t2;
   int (*edge_func)(),(*tri_func)();
   int *argptr;
{
    int f;
    FVECT dir;

#ifdef TEST_DRIVER
    Pick_cnt=0;
#endif;
  /* First add all of the leaf cells lying on the triangle perimeter:
     mark all cells seen on the way
   */
    f = 0;
    /* Visit cells along edges of the tri */
    stVisit_tri_edges(st,t0,t1,t2,edge_func,&f,argptr);

    /* Now visit All cells interior */
    if(QT_FLAG_FILL_TRI(f) || QT_FLAG_UPDATE(f))
       stVisit_tri(st,t0,t1,t2,tri_func,&f,argptr);
}













