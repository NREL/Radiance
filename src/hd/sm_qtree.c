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
#include "object.h"

QUADTREE  *quad_block[QT_MAX_BLK];		/* our quadtree */
static QUADTREE  quad_free_list = EMPTY;  /* freed octree nodes */
static QUADTREE  treetop = 0;		/* next free node */



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
		   (unsigned)QT_BLOCK_SIZE*4*sizeof(QUADTREE))) == NULL)
	   return(EMPTY);
    }
    treetop += 4;
    return(freet);
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

	for (i = 0; i < QT_MAX_BLK; i++)
	{
	    free((char *)quad_block[i],
		  (unsigned)QT_BLOCK_SIZE*4*sizeof(QUADTREE));
	    quad_block[i] = NULL;
	}
	quad_free_list = EMPTY;
	treetop = 0;
}

QUADTREE
qtCompress(qt)			/* recursively combine nodes */
register QUADTREE  qt;
{
    register int  i;
    register QUADTREE  qres;

    if (!QT_IS_TREE(qt))	/* not a tree */
       return(qt);
    qres = QT_NTH_CHILD(qt,0) = qtCompress(QT_NTH_CHILD(qt,0));
    for (i = 1; i < 4; i++)
       if((QT_NTH_CHILD(qt,i) = qtCompress(QT_NTH_CHILD(qt,i))) != qres)
	  qres = qt;
    if(!QT_IS_TREE(qres))
    {   /* all were identical leaves */
	QT_NTH_CHILD(qt,0) = quad_free_list;
	quad_free_list = qt;
    }
    return(qres);
}

QUADTREE
qtLocate_leaf(qtptr,bcoord,type,which)
QUADTREE *qtptr;
double bcoord[3];
char *type,*which;
{
  int i;
  QUADTREE *child;

    if(QT_IS_TREE(*qtptr))
    {  
      i = bary2d_child(bcoord);
      child = QT_NTH_CHILD_PTR(*qtptr,i);
      return(qtLocate_leaf(child,bcoord,type,which));
    }
    else
      return(*qtptr);
}




QUADTREE
qtRoot_point_locate(qtptr,v0,v1,v2,pt,type,which)
QUADTREE *qtptr;
FVECT v0,v1,v2;
FVECT pt;
char *type,*which;
{
    char d,w;
    int i,x,y;
    QUADTREE *child;
    QUADTREE qt;
    FVECT n,i_pt;
    double pd,bcoord[3];

    /* Determine if point lies within pyramid (and therefore
       inside a spherical quadtree cell):GT_INTERIOR, on one of the 
       pyramid sides (and on cell edge):GT_EDGE(1,2 or 3), 
       or on pyramid vertex (and on cell vertex):GT_VERTEX(1,2, or 3).
       For each triangle edge: compare the
       point against the plane formed by the edge and the view center
    */
    d = test_single_point_against_spherical_tri(v0,v1,v2,pt,&w);  

    /* Not in this triangle */
    if(!d)
    {
      if(which)
	*which = 0;
      return(EMPTY);
    }

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

      i = max_index(n);
      x = (i+1)%3;
      y = (i+2)%3;
      /* Calculate barycentric coordinates of i_pt */
      bary2d(v0[x],v0[y],v1[x],v1[y],v2[x],v2[y],i_pt[x],i_pt[y],bcoord);

      i = bary2d_child(bcoord);
      child = QT_NTH_CHILD_PTR(*qtptr,i);
      return(qtLocate_leaf(child,bcoord,type,which));
    }
    else
       if(!QT_IS_EMPTY(*qtptr))
       {  
	   /* map GT_VERTEX,GT_EDGE,GT_FACE GT_INTERIOR of pyramid to
	      spherical triangle primitives
	      */
	 if(type)
	   *type = d;
	 if(which)
	   *which = w;
	 return(*qtptr);
       }
    return(EMPTY);
}

int
qtPoint_in_tri(qtptr,v0,v1,v2,pt,type,which)
QUADTREE *qtptr;
FVECT v0,v1,v2;
FVECT pt;
char *type,*which;
{
    QUADTREE qt;
    OBJECT os[MAXSET+1],*optr;
    char d,w;
    int i,id;
    FVECT p0,p1,p2;

    qt = qtRoot_point_locate(qtptr,v0,v1,v2,pt,type,which);

    if(QT_IS_EMPTY(qt))
       return(EMPTY);
    
    /* Get the set */
    qtgetset(os,qt);
    for (i = QT_SET_CNT(os),optr = QT_SET_PTR(os); i > 0; i--)
    {
	/* Find the triangle that pt falls in (NOTE:FOR now return first 1) */
	id = QT_SET_NEXT_ELEM(optr);
	qtTri_verts_from_id(id,p0,p1,p2);
	d = test_single_point_against_spherical_tri(p0,p1,p2,pt,&w);  
	if(d)
	{
	  if(type)
	    *type = d;
	  if(which)
	    *which = w;
	  return(id);
	}
    }
    return(EMPTY);
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
qtAdd_tri(qtptr,id,t0,t1,t2,v0,v1,v2,n)
QUADTREE *qtptr;
int id;
FVECT t0,t1,t2;
FVECT v0,v1,v2;
int n;
{
  
  char test;
  int i,index;
  FVECT a,b,c;
  OBJECT os[MAXSET+1],*optr;
  QUADTREE qt;
  int found;
  FVECT r0,r1,r2;

  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  test = spherical_tri_intersect(t0,t1,t2,v0,v1,v2);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);
  found = 0;

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    n++;
    qtSubdivide_tri(v0,v1,v2,a,b,c);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,0),id,t0,t1,t2,v0,a,c,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,1),id,t0,t1,t2,a,v1,b,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,2),id,t0,t1,t2,c,b,v2,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,3),id,t0,t1,t2,b,c,a,n);

#if 0
    if(!found)
    {
#ifdef TEST_DRIVER     
      HANDLE_ERROR("qtAdd_tri():Found in parent but not children\n");
#else 
      eputs("qtAdd_tri():Found in parent but not children\n");
#endif
    }
#endif
  }
  else
  {
      /* If this leave node emptry- create a new set */
      if(QT_IS_EMPTY(*qtptr))
      {
	  *qtptr = qtaddelem(*qtptr,id);
#if 0
	  {
	      int k;
	      qtgetset(os,*qtptr);
	      printf("\n%d:\n",os[0]);
	      for(k=1; k <= os[0];k++)
		 printf("%d ",os[k]);
	      printf("\n");
	  }
#endif
	  /*
	  os[0] = 0;
          insertelem(os,id);
	  qt = fullnode(os);
	  *qtptr = qt;
	  */
      }
      else
      {
	  qtgetset(os,*qtptr);
	  /* If the set is too large: subdivide */
	  if(QT_SET_CNT(os) < QT_SET_THRESHOLD)
	  {
	    *qtptr = qtaddelem(*qtptr,id);
#if 0
	    {
	      int k;
	      qtgetset(os,*qtptr);
	      printf("\n%d:\n",os[0]);
	      for(k=1; k <= os[0];k++)
		 printf("%d ",os[k]);
	      printf("\n");
	  }
#endif	    
	    /*
	     insertelem(os,id);
	     *qtptr = fullnode(os);
	     */
	  }
	  else
	  {
	    if (n < QT_MAX_LEVELS)
            {
		 /* If set size exceeds threshold: subdivide cell and
		    reinsert set tris into cell
		    */
		 n++;
		 qtfreeleaf(*qtptr);
		 qtSubdivide(qtptr);
		 found = qtAdd_tri(qtptr,id,t0,t1,t2,v0,v1,v2,n);
#if 0
		 if(!found)
		 {
#ifdef TEST_DRIVER     
      HANDLE_ERROR("qtAdd_tri():Found in parent but not children\n");
#else 
		   eputs("qtAdd_tri():Found in parent but not children\n");
#endif
		 }
#endif
		 for(optr = &(os[1]),i = QT_SET_CNT(os); i > 0; i--)
		 {
		   id = QT_SET_NEXT_ELEM(optr);
		   qtTri_verts_from_id(id,r0,r1,r2);
		   found=qtAdd_tri(qtptr,id,r0,r1,r2,v0,v1,v2,n);
#ifdef DEBUG
		 if(!found)
		    eputs("qtAdd_tri():Reinsert-in parent but not children\n");
#endif
	       }
	     }
	    else
	      if(QT_SET_CNT(os) < QT_MAX_SET)
		{
		  *qtptr = qtaddelem(*qtptr,id);
#if 0
		  {
	      int k;
	      qtgetset(os,*qtptr);
	      printf("\n%d:\n",os[0]);
	      for(k=1; k <= os[0];k++)
		 printf("%d ",os[k]);
	      printf("\n");
	  }
#endif		  
		  /*
		    insertelem(os,id);
		    *qtptr = fullnode(os);
		  */
		}
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
char *arg;
{
  char test;
  FVECT a,b,c;

  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  test = spherical_tri_intersect(t0,t1,t2,v0,v1,v2);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    qtSubdivide_tri(v0,v1,v2,a,b,c);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,0),t0,t1,t2,v0,a,c,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,1),t0,t1,t2,a,v1,b,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,2),t0,t1,t2,c,b,v2,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,3),t0,t1,t2,b,c,a,func,arg);
  }
  else
    return(func(qtptr,arg));
}


int
qtRemove_tri(qtptr,id,t0,t1,t2,v0,v1,v2)
QUADTREE *qtptr;
int id;
FVECT t0,t1,t2;
FVECT v0,v1,v2;
{
  
  char test;
  int i;
  FVECT a,b,c;
  OBJECT os[MAXSET+1];
  
  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  test = spherical_tri_intersect(t0,t1,t2,v0,v1,v2);

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
	  qtgetset(os,*qtptr);
	  if(!inset(os,id))
	  {
#ifdef DEBUG	      
	      eputs("qtRemove_tri(): tri not in set\n");
#endif
	  }
	  else
	  {
	    *qtptr = qtdelelem(*qtptr,id);
#if 0
	    {
	      int k;
	      if(!QT_IS_EMPTY(*qtptr))
	      {qtgetset(os,*qtptr);
	      printf("\n%d:\n",os[0]);
	      for(k=1; k <= os[0];k++)
		 printf("%d ",os[k]);
	      printf("\n");
	   }

	  }
#endif
	}
      }
  }
  return(TRUE);
}











