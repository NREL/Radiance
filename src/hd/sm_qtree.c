/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
*  sm_qtree.c: adapted from octree.c from radiance code
*/
/*
 *  octree.c - routines dealing with octrees and cubes.
 *
 *     7/28/85
 */

#include "standard.h"

#include "sm_geom.h"
#include "sm_qtree.h"

QUADTREE  *quad_block[QT_MAX_BLK];		/* our quadtree */
static QUADTREE  quad_free_list = EMPTY;  /* freed octree nodes */
static QUADTREE  treetop = 0;		/* next free node */
int4 *quad_flag= NULL;

#ifdef TEST_DRIVER
extern FVECT Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern int Pick_cnt,Pick_tri,Pick_samp;
extern FVECT Pick_point[500];
#endif

QUADTREE
qtAlloc()			/* allocate a quadtree */
{
    register QUADTREE  freet;

    if ((freet = quad_free_list) != EMPTY)
    {
	quad_free_list = QT_NTH_CHILD(freet, 0);
	return(freet);
    }
    freet = treetop;
    if (QT_BLOCK_INDEX(freet) == 0)
    {
	if (QT_BLOCK(freet) >= QT_MAX_BLK)
	   return(EMPTY);
	if ((quad_block[QT_BLOCK(freet)] = (QUADTREE *)malloc(
			QT_BLOCK_SIZE*4*sizeof(QUADTREE))) == NULL)
	   return(EMPTY);

	quad_flag = (int4 *)realloc((char *)quad_flag,
			(QT_BLOCK(freet)+1)*(QT_BLOCK_SIZE/8));
	if (quad_flag == NULL)
		return(EMPTY);
    }
    treetop += 4;
    return(freet);
}


qtClearAllFlags()		/* clear all quadtree branch flags */
{
	if (!treetop) return;
	bzero((char *)quad_flag, (QT_BLOCK(treetop-4)+1)*(QT_BLOCK_SIZE/8));
}


qtFree(qt)			/* free a quadtree */
   register QUADTREE  qt;
{
	register int  i;

	if (!QT_IS_TREE(qt))
	{
	  qtfreeleaf(qt);
	  return;
	}
	for (i = 0; i < 4; i++)
	   qtFree(QT_NTH_CHILD(qt, i));
	QT_NTH_CHILD(qt, 0) = quad_free_list;
	quad_free_list = qt;
}


qtDone()			/* free EVERYTHING */
{
	register int	i;

	qtfreeleaves();
	for (i = 0; i < QT_MAX_BLK; i++)
	{
	    if (quad_block[i] == NULL)
		break;
	    free((char *)quad_block[i]);
	    quad_block[i] = NULL;
	}
	if (i) free((char *)quad_flag);
	quad_flag = NULL;
	quad_free_list = EMPTY;
	treetop = 0;
}

QUADTREE
*qtLocate_leaf(qtptr,bcoord,t0,t1,t2)
QUADTREE *qtptr;
double bcoord[3];
FVECT t0,t1,t2;
{
  int i;
  QUADTREE *child;
  FVECT a,b,c;

    if(QT_IS_TREE(*qtptr))
    {  
      i = bary_child(bcoord);
#ifdef DEBUG_TEST_DRIVER
      qtSubdivide_tri(Pick_v0[Pick_cnt-1],Pick_v1[Pick_cnt-1],
		      Pick_v2[Pick_cnt-1],a,b,c);
      qtNth_child_tri(Pick_v0[Pick_cnt-1],Pick_v1[Pick_cnt-1],
			   Pick_v2[Pick_cnt-1],a,b,c,i,
			   Pick_v0[Pick_cnt],Pick_v1[Pick_cnt],
			   Pick_v2[Pick_cnt]);
	   Pick_cnt++;
#endif

      child = QT_NTH_CHILD_PTR(*qtptr,i);
      if(t0)
      {
	qtSubdivide_tri(t0,t1,t2,a,b,c);
	qtNth_child_tri(t0,t1,t2,a,b,c,i,t0,t1,t2);
      }
      return(qtLocate_leaf(child,bcoord,t0,t1,t2));
    }
    else
      return(qtptr);
}


QUADTREE
*qtRoot_point_locate(qtptr,v0,v1,v2,pt,t0,t1,t2)
QUADTREE *qtptr;
FVECT v0,v1,v2;
FVECT pt;
FVECT t0,t1,t2;
{
    int d;
    int i,x,y;
    QUADTREE *child;
    FVECT n,i_pt,a,b,c;
    double pd,bcoord[3];

    /* Determine if point lies within pyramid (and therefore
       inside a spherical quadtree cell):GT_INTERIOR, on one of the 
       pyramid sides (and on cell edge):GT_EDGE(1,2 or 3), 
       or on pyramid vertex (and on cell vertex):GT_VERTEX(1,2, or 3).
       For each triangle edge: compare the
       point against the plane formed by the edge and the view center
    */
    d = point_in_stri(v0,v1,v2,pt);  

    
    /* Not in this triangle */
    if(!d)
      return(NULL);

    /* Will return lowest level triangle containing point: It the
       point is on an edge or vertex: will return first associated
       triangle encountered in the child traversal- the others can
       be derived using triangle adjacency information
    */
    if(QT_IS_TREE(*qtptr))
    {  
      /* Find the intersection point */
      tri_plane_equation(v0,v1,v2,n,&pd,FALSE);
      intersect_vector_plane(pt,n,pd,NULL,i_pt);

      i = max_index(n,NULL);
      x = (i+1)%3;
      y = (i+2)%3;
      /* Calculate barycentric coordinates of i_pt */
      bary2d(v0[x],v0[y],v1[x],v1[y],v2[x],v2[y],i_pt[x],i_pt[y],bcoord);

      i = bary_child(bcoord);
      child = QT_NTH_CHILD_PTR(*qtptr,i);
#ifdef DEBUG_TEST_DRIVER
      Pick_cnt = 0;
      VCOPY(Pick_v0[0],v0);
      VCOPY(Pick_v1[0],v1);
      VCOPY(Pick_v2[0],v2);
      Pick_cnt++;
      qtSubdivide_tri(Pick_v0[Pick_cnt-1],Pick_v1[Pick_cnt-1],
		      Pick_v2[Pick_cnt-1],a,b,c);
      qtNth_child_tri(Pick_v0[Pick_cnt-1],Pick_v1[Pick_cnt-1],
		      Pick_v2[Pick_cnt-1],a,b,c,i,
			     Pick_v0[Pick_cnt],Pick_v1[Pick_cnt],
			     Pick_v2[Pick_cnt]);
	   Pick_cnt++;
#endif
      if(t0)
      {
	qtSubdivide_tri(v0,v1,v2,a,b,c);
	qtNth_child_tri(v0,v1,v2,a,b,c,i,t0,t1,t2);
      }
      return(qtLocate_leaf(child,bcoord,t0,t1,t2));
    }
    else
    {
	if(t0)
        {
	    VCOPY(t0,v0);
	    VCOPY(t1,v1);
	    VCOPY(t2,v2);
	}
	return(qtptr);
    }
}


QUADTREE
qtSubdivide(qtptr)
QUADTREE *qtptr;
{
  QUADTREE node;
  node = qtAlloc();
  QT_CLEAR_CHILDREN(node);
  *qtptr = node;
  return(node);
}


QUADTREE
qtSubdivide_nth_child(qt,n)
QUADTREE qt;
int n;
{
  QUADTREE node;

  node = qtSubdivide(&(QT_NTH_CHILD(qt,n)));
  
  return(node);
}

/* for triangle v0-v1-v2- returns a,b,c: children are:

	v2                     0: v0,a,c
        /\                     1: a,v1,b                  
       /2 \                    2: c,b,v2
     c/____\b                  3: b,c,a
     /\    /\  
    /0 \3 /1 \
  v0____\/____\v1
        a
 */

qtSubdivide_tri(v0,v1,v2,a,b,c)
FVECT v0,v1,v2;
FVECT a,b,c;
{
    EDGE_MIDPOINT_VEC3(a,v0,v1);
    EDGE_MIDPOINT_VEC3(b,v1,v2);
    EDGE_MIDPOINT_VEC3(c,v2,v0);
}

qtNth_child_tri(v0,v1,v2,a,b,c,i,r0,r1,r2)
FVECT v0,v1,v2;
FVECT a,b,c;
int i;
FVECT r0,r1,r2;
{

  switch(i){
  case 0:  
  VCOPY(r0,v0); VCOPY(r1,a);    VCOPY(r2,c);
    break;
  case 1:  
  VCOPY(r0,a); VCOPY(r1,v1);    VCOPY(r2,b);
    break;
  case 2:  
  VCOPY(r0,c); VCOPY(r1,b);    VCOPY(r2,v2);
    break;
  case 3:  
  VCOPY(r0,b); VCOPY(r1,c);    VCOPY(r2,a);
    break;
  }
}

/* Add triangle "id" to all leaf level cells that are children of 
quadtree pointed to by "qtptr" with cell vertices "t1,t2,t3"
that it overlaps (vertex and edge adjacencies do not count
as overlapping). If the addition of the triangle causes the cell to go over
threshold- the cell is split- and the triangle must be recursively inserted
into the new child cells: it is assumed that "v1,v2,v3" are normalized
*/

int
qtRoot_add_tri(qtptr,q0,q1,q2,t0,t1,t2,id,n)
QUADTREE *qtptr;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int id;
int n;
{
  int test;
  int found;

  test = stri_intersect(q0,q1,q2,t0,t1,t2);
  if(!test)
    return(FALSE);
  
  found = qtAdd_tri(qtptr,q0,q1,q2,t0,t1,t2,id,n);
  
  return(found);
}

int
qtAdd_tri(qtptr,q0,q1,q2,t0,t1,t2,id,n)
QUADTREE *qtptr;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int id;
int n;
{
  int i,index,test,found;
  FVECT a,b,c;
  OBJECT os[QT_MAXSET+1],*optr;
  QUADTREE qt;
  FVECT r0,r1,r2;

  found = 0;
  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    n++;
    qtSubdivide_tri(q0,q1,q2,a,b,c);
    test = stri_intersect(t0,t1,t2,q0,a,c);
    if(test)
      found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,0),q0,a,c,t0,t1,t2,id,n);
    test = stri_intersect(t0,t1,t2,a,q1,b);
    if(test)
      found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,1),a,q1,b,t0,t1,t2,id,n);
    test = stri_intersect(t0,t1,t2,c,b,q2);
    if(test)
      found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,2),c,b,q2,t0,t1,t2,id,n);
    test = stri_intersect(t0,t1,t2,b,c,a);
    if(test)
      found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,3),b,c,a,t0,t1,t2,id,n);
  }
  else
  {
      /* If this leave node emptry- create a new set */
      if(QT_IS_EMPTY(*qtptr))
	*qtptr = qtaddelem(*qtptr,id);
      else
      {
	  /* If the set is too large: subdivide */
	optr = qtqueryset(*qtptr);

	if(QT_SET_CNT(optr) < QT_SET_THRESHOLD)
	  *qtptr = qtaddelem(*qtptr,id);
	  else
	  {
	    if (n < QT_MAX_LEVELS)
            {
		 /* If set size exceeds threshold: subdivide cell and
		    reinsert set tris into cell
		    */
	      qtgetset(os,*qtptr);	

	      n++;
	      qtfreeleaf(*qtptr);
	      qtSubdivide(qtptr);
	      found = qtAdd_tri(qtptr,q0,q1,q2,t0,t1,t2,id,n);

	      for(optr = QT_SET_PTR(os),i = QT_SET_CNT(os); i > 0; i--)
		{
		  id = QT_SET_NEXT_ELEM(optr);
		  qtTri_from_id(id,r0,r1,r2,NULL,NULL,NULL,NULL,NULL,NULL);
		  found=qtAdd_tri(qtptr,q0,q1,q2,r0,r1,r2,id,n);
#ifdef DEBUG
		  if(!found)
		    eputs("qtAdd_tri():Reinsert-in parent but not children\n");
#endif
		}
	    }
	    else
	      if(QT_SET_CNT(optr) < QT_MAXSET)
		  *qtptr = qtaddelem(*qtptr,id);
	      else
		{
#ifdef DEBUG
		    eputs("qtAdd_tri():two many levels\n");
#endif
		    return(FALSE);
		}
	  }
      }
  }
  return(TRUE);
}


int
qtApply_to_tri_cells(qtptr,t0,t1,t2,v0,v1,v2,func,arg)
QUADTREE *qtptr;
FVECT t0,t1,t2;
FVECT v0,v1,v2;
int (*func)();
int *arg;
{
  int test;
  FVECT a,b,c;

  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  test = stri_intersect(t0,t1,t2,v0,v1,v2);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);

  /* if this is tree: recurse */
  func(qtptr,arg);

  if(QT_IS_TREE(*qtptr))
  {
     QT_SET_FLAG(*qtptr);
     qtSubdivide_tri(v0,v1,v2,a,b,c);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,0),t0,t1,t2,v0,a,c,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,1),t0,t1,t2,a,v1,b,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,2),t0,t1,t2,c,b,v2,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,3),t0,t1,t2,b,c,a,func,arg);
  }
}

int
qtRemove_tri(qtptr,id,t0,t1,t2,v0,v1,v2)
QUADTREE *qtptr;
int id;
FVECT t0,t1,t2;
FVECT v0,v1,v2;
{
  
  int test;
  int i;
  FVECT a,b,c;
  OBJECT os[QT_MAXSET+1];
  
  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  test = stri_intersect(t0,t1,t2,v0,v1,v2);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    qtSubdivide_tri(v0,v1,v2,a,b,c);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,0),id,t0,t1,t2,v0,a,c);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,1),id,t0,t1,t2,a,v1,b);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,2),id,t0,t1,t2,c,b,v2);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,3),id,t0,t1,t2,b,c,a);
  }
  else
  {
      if(QT_IS_EMPTY(*qtptr))
      {
#ifdef DEBUG	 
	  eputs("qtRemove_tri(): triangle not found\n");
#endif
      }
      /* remove id from set */
      else
      {
	  if(!qtinset(*qtptr,id))
	  {
#ifdef DEBUG	      
	      eputs("qtRemove_tri(): tri not in set\n");
#endif
	  }
	  else
	  {
	    *qtptr = qtdelelem(*qtptr,id);
	}
      }
  }
  return(TRUE);
}


int
move_to_nbr(b,db0,db1,db2,tptr)
double b[3],db0,db1,db2;
double *tptr;
{
  double t,dt;
  int nbr;

  nbr = -1;
  /* Advance to next node */
  if(!ZERO(db0) && db0 < 0.0)
   {
     t = -b[0]/db0;
     nbr = 0;
   }
  else
    t = FHUGE;
  if(!ZERO(db1) && db1 < 0.0 )
  {
    dt = -b[1]/db1;
    if( dt < t)
    {
      t = dt;
      nbr = 1;
    }
  }
  if(!ZERO(db2) && db2 < 0.0 )
    {
      dt = -b[2]/db2;
      if( dt < t)
      {
	t = dt;
	nbr = 2;
      }
    }
  *tptr = t;
  return(nbr);
}

int
qtTrace_ray(qtptr,b,db0,db1,db2,orig,dir,func,arg1,arg2)
   QUADTREE *qtptr;
   double b[3],db0,db1,db2;
   FVECT orig,dir;
   int (*func)();
   int *arg1,arg2;
{

    int i,found;
    QUADTREE *child;
    int nbr,next;
    double t;
#ifdef DEBUG_TEST_DRIVER
    
    FVECT a1,b1,c1;
    int Pick_parent = Pick_cnt-1;
    qtSubdivide_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1);

#endif
    if(QT_IS_TREE(*qtptr))
    {
	/* Find the appropriate child and reset the coord */
	i = bary_child(b);

	QT_SET_FLAG(*qtptr);

	for(;;)
        {
	   child = QT_NTH_CHILD_PTR(*qtptr,i);

	   if(i != 3)
	      nbr = qtTrace_ray(child,b,db0,db1,db2,orig,dir,func,arg1,arg2);
	   else
	       /* If the center cell- must flip direction signs */
	      nbr =qtTrace_ray(child,b,-db0,-db1,-db2,orig,dir,func,arg1,arg2);
	   if(nbr == QT_DONE)
	      return(nbr);

	   /* If in same block: traverse */
	   if(i==3)
	      next = nbr;
	     else
		if(nbr == i)
		   next = 3;
	     else
	       {
		 /* reset the barycentric coordinates in the parents*/
		 bary_parent(b,i);
		 /* Else pop up to parent and traverse from there */
		 return(nbr);
	       }
	     bary_from_child(b,i,next);
	     i = next;
	 }
    }
    else
   {
#ifdef DEBUG_TEST_DRIVER
	   qtNth_child_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1,i,
			   Pick_v0[Pick_cnt],Pick_v1[Pick_cnt],
			   Pick_v2[Pick_cnt]);
	   Pick_cnt++;
#endif

       if(func(qtptr,orig,dir,arg1,arg2) == QT_DONE)
	  return(QT_DONE);

       /* Advance to next node */
       /* NOTE: Optimize: should only have to check 1/2 */
       nbr = move_to_nbr(b,db0,db1,db2,&t);

       if(nbr != -1)
       {
	 b[0] += t * db0;
	 b[1] += t * db1;
	 b[2] += t * db2;
       }
       return(nbr);
   }
    
}

int
qtRoot_trace_ray(qtptr,q0,q1,q2,orig,dir,func,arg1,arg2)
   QUADTREE *qtptr;
   FVECT q0,q1,q2;
   FVECT orig,dir;
   int (*func)();
   int *arg1,arg2;
{
  int i,x,y,nbr;
  QUADTREE *child;
  FVECT n,c,i_pt,d;
  double pd,b[3],db[3],t;
    /* Determine if point lies within pyramid (and therefore
       inside a spherical quadtree cell):GT_INTERIOR, on one of the 
       pyramid sides (and on cell edge):GT_EDGE(1,2 or 3), 
       or on pyramid vertex (and on cell vertex):GT_VERTEX(1,2, or 3).
       For each triangle edge: compare the
       point against the plane formed by the edge and the view center
    */
  i = point_in_stri(q0,q1,q2,orig);  
    
  /* Not in this triangle */
  if(!i)
     return(INVALID);
  /* Project the origin onto the root node plane */

  /* Find the intersection point of the origin */
  tri_plane_equation(q0,q1,q2,n,&pd,FALSE);
  intersect_vector_plane(orig,n,pd,NULL,i_pt);
  /* project the dir as well */
  VADD(c,orig,dir);
  intersect_vector_plane(c,n,pd,&t,c);

    /* map to 2d by dropping maximum magnitude component of normal */
  i = max_index(n,NULL);
  x = (i+1)%3;
  y = (i+2)%3;
  /* Calculate barycentric coordinates of orig */
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b);
  /* Calculate barycentric coordinates of dir */
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],c[x],c[y],db);
  if(t < 0.0)
     VSUB(db,b,db);
  else
     VSUB(db,db,b);


#ifdef DEBUG_TEST_DRIVER
    VCOPY(Pick_v0[Pick_cnt],q0);
    VCOPY(Pick_v1[Pick_cnt],q1);
    VCOPY(Pick_v2[Pick_cnt],q2);
    Pick_cnt++;
#endif

      /* trace the ray starting with this node */
    nbr = qtTrace_ray(qtptr,b,db[0],db[1],db[2],orig,dir,func,arg1,arg2);
    return(nbr);
    
}

qtVisit_tri_interior(qtptr,q0,q1,q2,t0,t1,t2,n,func,arg1,arg2,arg3)
   QUADTREE *qtptr;
   FVECT q0,q1,q2;
   FVECT t0,t1,t2;
   int n;
   int (*func)();
   int *arg1,arg2,*arg3;
{
    int i,found,test;
    QUADTREE *child;
    FVECT c0,c1,c2,a,b,c;
    OBJECT os[QT_MAXSET+1],*optr;
    int w;
    
    /* If qt Flag set, or qt vertices interior to t0t1t2-descend */
tree_modified:

    if(QT_IS_TREE(*qtptr))
    {  
	if(QT_IS_FLAG(*qtptr) ||  point_in_stri(t0,t1,t2,q0))
	{
	    QT_SET_FLAG(*qtptr);
	    qtSubdivide_tri(q0,q1,q2,a,b,c);
	    /* descend to children */
	    for(i=0;i < 4; i++)
	    {
		child = QT_NTH_CHILD_PTR(*qtptr,i);
		qtNth_child_tri(q0,q1,q2,a,b,c,i,c0,c1,c2);
		qtVisit_tri_interior(child,c0,c1,c2,t0,t1,t2,n+1,
				     func,arg1,arg2,arg3);
	    }
	}
    }
    else
    {
      /* NOTE THIS IN TRI TEST Could be replaced by a flag */
      if(!QT_IS_EMPTY(*qtptr))
      {
	 if(qtinset(*qtptr,arg2))
	   if(func(qtptr,q0,q1,q2,t0,t1,t2,n,arg1,arg2,arg3)==QT_MODIFIED)
	     goto tree_modified;
	   else
	     return;
      }
      if(point_in_stri(t0,t1,t2,q0) )
	  if(func(qtptr,q0,q1,q2,t0,t1,t2,n,arg1,arg2,arg3)==QT_MODIFIED)
	    goto tree_modified;
    }
}






/* NOTE: SINCE DIR could be unit: then we could use integer math */
int
qtVisit_tri_edges(qtptr,b,db0,db1,db2,
		   db,wptr,t,sign,sfactor,func,arg1,arg2,arg3)
   QUADTREE *qtptr;
   double b[3],db0,db1,db2,db[3][3];
   int *wptr;
   double t[3];
   int sign;
   double sfactor;
   int (*func)();
   int *arg1,arg2,*arg3;
{
    int i,found;
    QUADTREE *child;
    int nbr,next,w;
    double t_l,t_g;
#ifdef DEBUG_TEST_DRIVER
    FVECT a1,b1,c1;
    int Pick_parent = Pick_cnt-1;
    qtSubdivide_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1);
#endif
    if(QT_IS_TREE(*qtptr))
    {
	/* Find the appropriate child and reset the coord */
	i = bary_child(b);

	QT_SET_FLAG(*qtptr);

	for(;;)
        {
	  w = *wptr;
	   child = QT_NTH_CHILD_PTR(*qtptr,i);

	   if(i != 3)
	       nbr = qtVisit_tri_edges(child,b,db0,db1,db2,
					db,wptr,t,sign,
					sfactor*2.0,func,arg1,arg2,arg3);
	   else
	     /* If the center cell- must flip direction signs */
	     nbr = qtVisit_tri_edges(child,b,-db0,-db1,-db2,
				     db,wptr,t,1-sign,
				     sfactor*2.0,func,arg1,arg2,arg3);

	   if(nbr == QT_DONE)
	     return(nbr);
	   if(*wptr != w)
	   {
	     w = *wptr;
	     db0 = db[w][0];db1 = db[w][1];db2 = db[w][2];
	     if(sign)
	       {  db0 *= -1.0;db1 *= -1.0; db2 *= -1.0;}
	   }
	   /* If in same block: traverse */
	   if(i==3)
	      next = nbr;
	     else
		if(nbr == i)
		   next = 3;
	     else
	       {
		 /* reset the barycentric coordinates in the parents*/
		 bary_parent(b,i);
		 /* Else pop up to parent and traverse from there */
		 return(nbr);
	       }
	   bary_from_child(b,i,next);
	   i = next;
	}
    }
    else
   {
#ifdef DEBUG_TEST_DRIVER
	   qtNth_child_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1,i,Pick_v0[Pick_cnt],
			   Pick_v1[Pick_cnt],Pick_v2[Pick_cnt]);
	   Pick_cnt++;
#endif

       if(func(qtptr,arg1,arg2,arg3) == QT_DONE)
	  return(QT_DONE);

       /* Advance to next node */
       w = *wptr;
       while(1)
       {
	 nbr = move_to_nbr(b,db0,db1,db2,&t_l);

	 t_g = t_l/sfactor;
#ifdef DEBUG
	 if(t[w] <= 0.0)
	   eputs("qtVisit_tri_edges():negative t\n");
#endif
	 if(t_g >= t[w])
	 {
	   if(w == 2)
	     return(QT_DONE);

	   b[0] += (t[w])*sfactor*db0;
	   b[1] += (t[w])*sfactor*db1;
	   b[2] += (t[w])*sfactor*db2;
	   w++;
	   db0 = db[w][0];
	   db1 = db[w][1];
	   db2 = db[w][2];
	   if(sign)
	  {  db0 *= -1.0;db1 *= -1.0; db2 *= -1.0;}
	 }
       else
	 if(nbr != INVALID)
         {
	   b[0] += t_l * db0;
	   b[1] += t_l * db1;
	   b[2] += t_l * db2;

	   t[w] -= t_g;
	   *wptr = w;
	   return(nbr);
	 }
	 else
	   return(INVALID);
       }
   }
    
}


int
qtRoot_visit_tri_edges(qtptr,q0,q1,q2,tri,i_pt,wptr,func,arg1,arg2,arg3)
   QUADTREE *qtptr;
   FVECT q0,q1,q2;
   FVECT tri[3],i_pt;
   int *wptr;
   int (*func)();
   int *arg1,arg2,*arg3;
{
  int x,y,z,nbr,w,i,j;
  QUADTREE *child;
  FVECT n,c,d,v[3];
  double pd,b[4][3],db[3][3],et[3],t[3],exit_pt;
    
  w = *wptr;

  /* Project the origin onto the root node plane */

  /* Find the intersection point of the origin */
  tri_plane_equation(q0,q1,q2,n,&pd,FALSE);
  /* map to 2d by dropping maximum magnitude component of normal */
  z = max_index(n,NULL);
  x = (z+1)%3;
  y = (z+2)%3;
  /* Calculate barycentric coordinates for current vertex */
  if(w != -1)
  {
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b[3]);  
    intersect_vector_plane(tri[w],n,pd,&(et[w]),v[w]);
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[w][x],v[w][y],b[w]);  
  }
  else
  /* Just starting: b[0] is the origin point: guaranteed to be valid b*/
  {
    w = 0;
    intersect_vector_plane(tri[0],n,pd,&(et[0]),v[0]);
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[0][x],v[0][y],b[0]);  
    VCOPY(b[3],b[0]);
  }


  j = (w+1)%3;
  intersect_vector_plane(tri[j],n,pd,&(et[j]),v[j]);
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[j][x],v[j][y],b[j]);  
  if(et[j] < 0.0)
  {
      VSUB(db[w],b[3],b[j]);
      t[w] = FHUGE;
  }
  else
 {
   /* NOTE: for stability: do not increment with ipt- use full dir and
      calculate t: but for wrap around case: could get same problem? 
      */
   VSUB(db[w],b[j],b[3]);
   t[w] = 1.0;
   move_to_nbr(b[3],db[w][0],db[w][1],db[w][2],&exit_pt);
   if(exit_pt >= 1.0)
   {
     for(;j < 3;j++)
      {
	  i = (j+1)%3;
	  if(i!= w)
	    {
	      intersect_vector_plane(tri[i],n,pd,&(et[i]),v[i]);
	      bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[i][x],
		     v[i][y],b[i]);
	    }
	  if(et[i] < 0.0)
	     {
		 VSUB(db[j],b[j],b[i]);
		 t[j] = FHUGE;
		 break;
	     }
	  else
	     {
		 VSUB(db[j],b[i],b[j]);
		 t[j] = 1.0;
	     }
	  move_to_nbr(b[j],db[j][0],db[j][1],db[j][2],&exit_pt);
	  if(exit_pt < 1.0)
	     break;
      }
   }
 }
  *wptr = w;
  /* trace the ray starting with this node */
    nbr = qtVisit_tri_edges(qtptr,b[3],db[w][0],db[w][1],db[w][2],
			     db,wptr,t,0,1.0,func,arg1,arg2,arg3);
    if(nbr != INVALID && nbr != QT_DONE)
    {
      i_pt[x] = b[3][0]*q0[x] + b[3][1]*q1[x] + b[3][2]*q2[x];
      i_pt[y] = b[3][0]*q0[y] + b[3][1]*q1[y] + b[3][2]*q2[y];
      i_pt[z] = (-n[x]*i_pt[x] - n[y]*i_pt[y] -pd)/n[z]; 
    }
    return(nbr);
    
}

int
move_to_nbri(b,db0,db1,db2,tptr)
BCOORD b[3];
BDIR db0,db1,db2;
TINT *tptr;
{
  TINT t,dt;
  BCOORD bc;
  int nbr;
  
  nbr = -1;
  /* Advance to next node */
  if(db0 < 0)
   {
     bc = MAXBCOORD*b[0];
     t = bc/-db0;
     nbr = 0;
   }
  else
    t = HUGET;
  if(db1 < 0)
  {
     bc = MAXBCOORD*b[1];
     dt = bc/-db1;
    if( dt < t)
    {
      t = dt;
      nbr = 1;
    }
  }
  if(db2 < 0)
  {
     bc = MAXBCOORD*b[2];
     dt = bc/-db2;
       if( dt < t)
      {
	t = dt;
	nbr = 2;
      }
    }
  *tptr = t;
  return(nbr);
}
/* NOTE: SINCE DIR could be unit: then we could use integer math */
int
qtVisit_tri_edgesi(qtptr,b,db0,db1,db2,
		   db,wptr,t,sign,sfactor,func,arg1,arg2,arg3)
   QUADTREE *qtptr;
   BCOORD b[3];
   BDIR db0,db1,db2,db[3][3];
   int *wptr;
   TINT t[3];
   int sign;
   int sfactor;
   int (*func)();
   int *arg1,arg2,*arg3;
{
    int i,found;
    QUADTREE *child;
    int nbr,next,w;
    TINT t_g,t_l,t_i;
#ifdef DEBUG_TEST_DRIVER
    FVECT a1,b1,c1;
    int Pick_parent = Pick_cnt-1;
    qtSubdivide_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1);
#endif
    if(QT_IS_TREE(*qtptr))
    {
	/* Find the appropriate child and reset the coord */
	i = baryi_child(b);

	QT_SET_FLAG(*qtptr);

	for(;;)
        {
	  w = *wptr;
	   child = QT_NTH_CHILD_PTR(*qtptr,i);

	   if(i != 3)
	       nbr = qtVisit_tri_edgesi(child,b,db0,db1,db2,
					db,wptr,t,sign,
					sfactor+1,func,arg1,arg2,arg3);
	   else
	     /* If the center cell- must flip direction signs */
	     nbr = qtVisit_tri_edgesi(child,b,-db0,-db1,-db2,
				     db,wptr,t,1-sign,
				     sfactor+1,func,arg1,arg2,arg3);

	   if(nbr == QT_DONE)
	     return(nbr);
	   if(*wptr != w)
	   {
	     w = *wptr;
	     db0 = db[w][0];db1 = db[w][1];db2 = db[w][2];
	     if(sign)
	       {  db0 *= -1;db1 *= -1; db2 *= -1;}
	   }
	   /* If in same block: traverse */
	   if(i==3)
	      next = nbr;
	     else
		if(nbr == i)
		   next = 3;
	     else
	       {
		 /* reset the barycentric coordinates in the parents*/
		 baryi_parent(b,i);
		 /* Else pop up to parent and traverse from there */
		 return(nbr);
	       }
	   baryi_from_child(b,i,next);
	   i = next;
	}
    }
    else
   {
#ifdef DEBUG_TEST_DRIVER
	   qtNth_child_tri(Pick_v0[Pick_parent],Pick_v1[Pick_parent],
			   Pick_v2[Pick_parent],a1,b1,c1,i,Pick_v0[Pick_cnt],
			   Pick_v1[Pick_cnt],Pick_v2[Pick_cnt]);
	   Pick_cnt++;
#endif

       if(func(qtptr,arg1,arg2,arg3) == QT_DONE)
	  return(QT_DONE);

       /* Advance to next node */
       w = *wptr;
       while(1)
       {
	   nbr = move_to_nbri(b,db0,db1,db2,&t_i);

	 t_g = t_i >> sfactor;
	 	 
	 if(t_g >= t[w])
	 {
	   if(w == 2)
	     return(QT_DONE);

	   /* The edge fits in the cell- therefore the result of shifting db by
	      sfactor is guaranteed to be less than MAXBCOORD
	    */
	   /* Caution: (t[w]*db) must occur before divide by MAXBCOORD
	      since t[w] will always be < MAXBCOORD
	   */
	   b[0] = b[0] + (t[w]*db0*(1 << sfactor))/MAXBCOORD;
	   b[1] = b[1] + (t[w]*db1*(1 << sfactor))/MAXBCOORD;
	   b[2] = b[2] + (t[w]*db2*(1 << sfactor))/MAXBCOORD;
	   w++;
	   db0 = db[w][0]; db1 = db[w][1]; db2 = db[w][2];
	   if(sign)
	   {  db0 *= -1;db1 *= -1; db2 *= -1;}
	 }
       else
	 if(nbr != INVALID)
         {
	   /* Caution: (t_i*db) must occur before divide by MAXBCOORD
	      since t_i will always be < MAXBCOORD
	   */
	   b[0] = b[0] + (t_i *db0) / MAXBCOORD;
	   b[1] = b[1] + (t_i *db1) / MAXBCOORD;
	   b[2] = b[2] + (t_i *db2) / MAXBCOORD;

	   t[w] -= t_g;
	   *wptr = w;
	   return(nbr);
	 }
	 else
	   return(INVALID);
       }
   }    
}


int
qtRoot_visit_tri_edgesi(qtptr,q0,q1,q2,tri,i_pt,wptr,func,arg1,arg2,arg3)
   QUADTREE *qtptr;
   FVECT q0,q1,q2;
   FVECT tri[3],i_pt;
   int *wptr;
   int (*func)();
   int *arg1,arg2,*arg3;
{
  int x,y,z,nbr,w,i,j;
  QUADTREE *child;
  FVECT n,c,d,v[3];
  double pd,b[4][3],db[3][3],et[3],exit_pt;
  BCOORD bi[3];
  TINT t[3];
  BDIR dbi[3][3];
  w = *wptr;

  /* Project the origin onto the root node plane */

  t[0] = t[1] = t[2] = 0;
  /* Find the intersection point of the origin */
  tri_plane_equation(q0,q1,q2,n,&pd,FALSE);
  /* map to 2d by dropping maximum magnitude component of normal */
  z = max_index(n,NULL);
  x = (z+1)%3;
  y = (z+2)%3;
  /* Calculate barycentric coordinates for current vertex */
  if(w != -1)
  {
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b[3]);  
    intersect_vector_plane(tri[w],n,pd,&(et[w]),v[w]);
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[w][x],v[w][y],b[w]);  
  }
  else
  /* Just starting: b[0] is the origin point: guaranteed to be valid b*/
  {
    w = 0;
    intersect_vector_plane(tri[0],n,pd,&(et[0]),v[0]);
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[0][x],v[0][y],b[0]);  
    VCOPY(b[3],b[0]);
  }


  j = (w+1)%3;
  intersect_vector_plane(tri[j],n,pd,&(et[j]),v[j]);
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[j][x],v[j][y],b[j]);  
  if(et[j] < 0.0)
  {
      VSUB(db[w],b[3],b[j]);
      t[w] = HUGET;
  }
  else
 {
   /* NOTE: for stability: do not increment with ipt- use full dir and
      calculate t: but for wrap around case: could get same problem? 
      */
   VSUB(db[w],b[j],b[3]);
   /* Check if the point is out side of the triangle: if so t[w] =HUGET */
   if((fabs(b[j][0])>(FTINY+1.0)) ||(fabs(b[j][1])>(FTINY+1.0)) ||
      (fabs(b[j][2])>(FTINY+1.0)))
      t[w] = HUGET;
   else
   {
       /* The next point is in the triangle- so db values will be in valid
	  range and t[w]= MAXT
	*/  
       t[w] = MAXT;
       if(j != 0)
	  for(;j < 3;j++)
	     {
		 i = (j+1)%3;
		 if(i!= w)
	         {
		     intersect_vector_plane(tri[i],n,pd,&(et[i]),v[i]);
		     bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[i][x],
			    v[i][y],b[i]);
		 }
		 if(et[i] < 0.0)
	         {
		     VSUB(db[j],b[j],b[i]);
		     t[j] = HUGET;
		     break;
		 }
		 else
		 {
		     VSUB(db[j],b[i],b[j]);
		     if((fabs(b[j][0])>(FTINY+1.0)) ||
			(fabs(b[j][1])>(FTINY+1.0)) ||
			(fabs(b[j][2])>(FTINY+1.0)))
			{
			    t[j] = HUGET;
			    break;
			}
		     else
			t[j] = MAXT;
		 }
	     }
   }
}
  *wptr = w;
  bary_dtol(b[3],db,bi,dbi,t);

  /* trace the ray starting with this node */
    nbr = qtVisit_tri_edgesi(qtptr,bi,dbi[w][0],dbi[w][1],dbi[w][2],
			     dbi,wptr,t,0,0,func,arg1,arg2,arg3);
    if(nbr != INVALID && nbr != QT_DONE)
    {
	b[3][0] = (double)bi[0]/(double)MAXBCOORD;
	b[3][1] = (double)bi[1]/(double)MAXBCOORD;
	b[3][2] = (double)bi[2]/(double)MAXBCOORD;
	i_pt[x] = b[3][0]*q0[x] + b[3][1]*q1[x] + b[3][2]*q2[x];
	i_pt[y] = b[3][0]*q0[y] + b[3][1]*q1[y] + b[3][2]*q2[y];
	i_pt[z] = (-n[x]*i_pt[x] - n[y]*i_pt[y] -pd)/n[z]; 
    }
    return(nbr);
    
}
















