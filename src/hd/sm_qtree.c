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
qtPoint_locate(qtptr,v1,v2,v3,pt,type,which,p0,p1,p2)
QUADTREE *qtptr;
FVECT v1,v2,v3;
FVECT pt;
char *type,*which;
FVECT p0,p1,p2;
{
    char d,w;
    int i;
    QUADTREE *child;
    QUADTREE qt;
    FVECT a,b,c;
    FVECT t1,t2,t3;

    /* Determine if point lies within pyramid (and therefore
       inside a spherical quadtree cell):GT_INTERIOR, on one of the 
       pyramid sides (and on cell edge):GT_EDGE(1,2 or 3), 
       or on pyramid vertex (and on cell vertex):GT_VERTEX(1,2, or 3).
       For each triangle edge: compare the
       point against the plane formed by the edge and the view center
    */
    d = test_single_point_against_spherical_tri(v1,v2,v3,pt,&w);  

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
      qtSubdivide_tri(v1,v2,v3,a,b,c);
      child = QT_NTH_CHILD_PTR(*qtptr,0);
      if(!QT_IS_EMPTY(*child))
      {
	qt = qtPoint_locate(child,v1,a,c,pt,type,which,p0,p1,p2);
	if(!QT_IS_EMPTY(qt))
	  return(qt);
      }
      child = QT_NTH_CHILD_PTR(*qtptr,1);
      if(!QT_IS_EMPTY(*child))
      {
	qt = qtPoint_locate(child,a,b,c,pt,type,which,p0,p1,p2);
	if(!QT_IS_EMPTY(qt))
	  return(qt);
      }
      child = QT_NTH_CHILD_PTR(*qtptr,2);
      if(!QT_IS_EMPTY(*child))
      {
	qt = qtPoint_locate(child,a,v2,b,pt,type,which,p0,p1,p2);
	if(!QT_IS_EMPTY(qt))
	  return(qt);
      }
      child = QT_NTH_CHILD_PTR(*qtptr,3);
      if(!QT_IS_EMPTY(*child))
      {
	qt = qtPoint_locate(child,c,b,v3,pt,type,which,p0,p1,p2);
	if(!QT_IS_EMPTY(qt))
	  return(qt);
      }
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
	 VCOPY(p0,v1);
	 VCOPY(p1,v2);
	 VCOPY(p2,v3);
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
  
    qt = qtPoint_locate(qtptr,v0,v1,v2,pt,type,which,p0,p1,p2);
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

/* for triangle v1-v2-v3- returns a,b,c: children are:

	v3                     0: v1,a,c
        /\                     1: a,b,c
       /3 \                    2: a,v2,b
     c/____\b                  3: c,b,v3
     /\    /\  
    /0 \1 /2 \
 v1/____\/____\v2
        a
 */

qtSubdivide_tri(v1,v2,v3,a,b,c)
FVECT v1,v2,v3;
FVECT a,b,c;
{
    EDGE_MIDPOINT_VEC3(a,v1,v2);
    normalize(a);
    EDGE_MIDPOINT_VEC3(b,v2,v3);
    normalize(b);
    EDGE_MIDPOINT_VEC3(c,v3,v1);
    normalize(c);
}

qtNth_child_tri(v1,v2,v3,a,b,c,i,r1,r2,r3)
FVECT v1,v2,v3;
FVECT a,b,c;
int i;
FVECT r1,r2,r3;
{
  VCOPY(r1,a); VCOPY(r2,b);    VCOPY(r3,c);
  switch(i){
  case 0:  
    VCOPY(r2,r1);
    VCOPY(r1,v1);
    break;
  case 1:  
    break;
  case 2:  
    VCOPY(r3,r2);
    VCOPY(r2,v2);
    break;
  case 3:  
    VCOPY(r1,r3);
    VCOPY(r3,v3);
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
qtAdd_tri(qtptr,id,t1,t2,t3,v1,v2,v3,n)
QUADTREE *qtptr;
int id;
FVECT t1,t2,t3;
FVECT v1,v2,v3;
int n;
{
  
  char test;
  int i,index;
  FVECT a,b,c;
  OBJECT os[MAXSET+1],*optr;
  QUADTREE qt;
  int found;
  FVECT r1,r2,r3;

  /* test if triangle (t1,t2,t3) overlaps cell triangle (v1,v2,v3) */
  test = spherical_tri_intersect(t1,t2,t3,v1,v2,v3);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);
  found = 0;

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    n++;
    qtSubdivide_tri(v1,v2,v3,a,b,c);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,0),id,t1,t2,t3,v1,a,c,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,1),id,t1,t2,t3,a,b,c,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,2),id,t1,t2,t3,a,v2,b,n);
    found |= qtAdd_tri(QT_NTH_CHILD_PTR(*qtptr,3),id,t1,t2,t3,c,b,v3,n);

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
		 found = qtAdd_tri(qtptr,id,t1,t2,t3,v1,v2,v3,n);
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
		   qtTri_verts_from_id(id,r1,r2,r3);
		   found=qtAdd_tri(qtptr,id,r1,r2,r3,v1,v2,v3,n);
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
qtApply_to_tri_cells(qtptr,t1,t2,t3,v1,v2,v3,func,arg)
QUADTREE *qtptr;
FVECT t1,t2,t3;
FVECT v1,v2,v3;
int (*func)();
char *arg;
{
  char test;
  FVECT a,b,c;

  /* test if triangle (t1,t2,t3) overlaps cell triangle (v1,v2,v3) */
  test = spherical_tri_intersect(t1,t2,t3,v1,v2,v3);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    qtSubdivide_tri(v1,v2,v3,a,b,c);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,0),t1,t2,t3,v1,a,c,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,1),t1,t2,t3,a,b,c,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,2),t1,t2,t3,a,v2,b,func,arg);
    qtApply_to_tri_cells(QT_NTH_CHILD_PTR(*qtptr,3),t1,t2,t3,c,b,v3,func,arg);
  }
  else
    return(func(qtptr,arg));
}


int
qtRemove_tri(qtptr,id,t1,t2,t3,v1,v2,v3)
QUADTREE *qtptr;
int id;
FVECT t1,t2,t3;
FVECT v1,v2,v3;
{
  
  char test;
  int i;
  FVECT a,b,c;
  OBJECT os[MAXSET+1];
  
  /* test if triangle (t1,t2,t3) overlaps cell triangle (v1,v2,v3) */
  test = spherical_tri_intersect(t1,t2,t3,v1,v2,v3);

  /* If triangles do not overlap: done */
  if(!test)
    return(FALSE);

  /* if this is tree: recurse */
  if(QT_IS_TREE(*qtptr))
  {
    qtSubdivide_tri(v1,v2,v3,a,b,c);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,0),id,t1,t2,t3,v1,a,c);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,1),id,t1,t2,t3,a,b,c);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,2),id,t1,t2,t3,a,v2,b);
    qtRemove_tri(QT_NTH_CHILD_PTR(*qtptr,3),id,t1,t2,t3,c,b,v3);
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
