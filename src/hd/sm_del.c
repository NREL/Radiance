/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm_del.c
 */
#include "standard.h"
#include "sm_flag.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm_qtree.h"
#include "sm_stree.h"
#include "sm.h"

static int Max_edges=200;
static EDGE *Edges=NULL;
static int Ecnt=0;

smFree_tri(sm,id)
SM *sm;
int id;
{
  TRI *tri,*t;

  tri = SM_NTH_TRI(sm,id);
  /* Add to the free_list */

  T_NEXT_FREE(tri) = SM_FREE_TRIS(sm);
  SM_FREE_TRIS(sm) = id;
  T_VALID_FLAG(tri) = -1;
}

/* Assumes mesh pointers have been cleaned up appropriately: just deletes from
   Point location and triangle data structure
*/
smDelete_tri(sm,t_id)
SM *sm;
int t_id;
{

  /* NOTE: Assumes that a new triangle adjacent to each vertex
     has been added- before the deletion: replacing
     the old tri- and therefore dont need to dereference any pointers
     to id because the vertices can no longer
     point to tri id as being the first triangle pointer
  */
  SM_SAMPLE_TRIS(sm)--;
  if(!SM_IS_NTH_T_BASE(sm,t_id))
  {

#if 0
    if(SM_IS_NTH_T_NEW(sm,t_id))
      smNew_tri_cnt--;
#endif
  }
  smClear_tri_flags(sm,t_id);

  smFree_tri(sm,t_id);
}


int
eNew_edge()
{
  if(!Edges)
    if(!(Edges = (EDGE *)realloc(NULL,(Max_edges+1)*sizeof(EDGE))))
      goto memerr;

  if(Ecnt >= Max_edges)
    {
      if(Max_edges > 10000)
	error(CONSISTENCY,"Too many edges in vertex loop\n");
      Max_edges += 100;
      if(!(Edges = (EDGE *)realloc(Edges,(Max_edges+1)*sizeof(EDGE))))
	goto memerr;
    }
  return(++Ecnt);

memerr:
  error(SYSTEM,"eNew_edge(): Unable to allocate memory");
}

/* Return list of edges defining polygon formed by boundary of triangles 
adjacent to id. Return set of triangles adjacent to id to delete in delptr
*/
LIST 
*smVertexPolygon(sm,id,del_ptr)
SM *sm;
int id;
LIST **del_ptr;
{
    TRI *tri,*t_next;
    LIST *elist,*end;
    int e,t_id,v_next,t_next_id,b_id,v_id;

    eClear_edges();
    elist = end =  NULL;

    /* Get the first triangle adjacent to vertex id */
    t_id = SM_NTH_VERT(sm,id);
    tri = SM_NTH_TRI(sm,t_id);

    e = eNew_edge();
    /* Get the  next vertex on the polygon boundary */
    v_id = T_WHICH_V(tri,id);
    b_id = (v_id + 1)%3;
    /* Create an edge */
    SET_E_NTH_VERT(e,0,T_NTH_V(tri,b_id));
    SET_E_NTH_TRI(e,0,INVALID);
    SET_E_NTH_TRI(e,1,T_NTH_NBR(tri,v_id));
    v_next = T_NTH_V(tri,(b_id+1)%3);
    SET_E_NTH_VERT(e,1,v_next);
    elist = add_data_to_circular_list(elist,&end,e);
    t_next_id = t_id;
    t_next = tri;

    *del_ptr = push_data(*del_ptr,t_id);
    /* Create a set to hold all of the triangles for deletion later */

    while((t_next_id = T_NTH_NBR(t_next,b_id)) != t_id)
    {
      e = eNew_edge();
      t_next = SM_NTH_TRI(sm,t_next_id);
      SET_E_NTH_VERT(e,0,v_next);
      SET_E_NTH_TRI(e,0,INVALID);
      v_id = T_WHICH_V(t_next,id);
      b_id = (v_id + 1)%3;
      SET_E_NTH_TRI(e,1,T_NTH_NBR(t_next,v_id));
      v_next = T_NTH_V(t_next,(b_id+1)%3);
      SET_E_NTH_VERT(e,1,v_next);
      elist = add_data_to_circular_list(elist,&end,e);
      *del_ptr = push_data(*del_ptr,t_next_id);
    }
    return(elist);
}


int
smTriangulate_add_tri(sm,id0,id1,id2,e0,e1,e2ptr)
SM *sm;
int id0,id1,id2,e0,e1,*e2ptr;
{
  int t_id;
  int e2;

#ifdef DEBUG 
  if(id0 == INVALID || id1==INVALID || id2==INVALID)
    error(CONSISTENCY,"bad id- smTriangulate_add_tri()\n");
#endif
  t_id = smAdd_tri(sm,id0,id1,id2);
  if(*e2ptr == 0)
  {
    e2 = eNew_edge();
    SET_E_NTH_VERT(e2,0,id2);
    SET_E_NTH_VERT(e2,1,id0);
  }
  else
    e2 = *e2ptr;
  /* set appropriate tri for each edge*/ 
  SET_E_NTH_TRI(e0,0,t_id);
  SET_E_NTH_TRI(e1,0,t_id);
  SET_E_NTH_TRI(e2,0,t_id);

  *e2ptr = e2;
  return(t_id);
}

int
smTriangulateConvex(sm,plist,add_ptr)
SM *sm;
LIST *plist,**add_ptr;
{
    int t_id,e_id0,e_id1,e_id2;
    int v_id0,v_id1,v_id2;
    LIST *lptr;

    lptr = plist;
    e_id0 = (int)LIST_DATA(lptr);
    v_id0 = E_NTH_VERT(e_id0,0);
    lptr = LIST_NEXT(lptr);
    while(LIST_NEXT(lptr) != plist)
    {
	e_id1 = (int)LIST_DATA(lptr);
	v_id1 = E_NTH_VERT(e_id1,0);
	v_id2 = E_NTH_VERT(e_id1,1);
	lptr = LIST_NEXT(lptr);

	if(LIST_NEXT(lptr) != plist)	
	  e_id2 = 0;
	else
	   e_id2 = (int)LIST_DATA(lptr);
	t_id = smTriangulate_add_tri(sm,v_id0,v_id1,v_id2,e_id0,e_id1,&e_id2);
	*add_ptr = push_data(*add_ptr,t_id);
	e_id0 = -e_id2;
    }
    free_list(plist);
    return(TRUE);
}
#ifdef TEST_DRIVER
FVECT Norm[500],B_V[500];
int Ncnt,Bcnt,Del=0;
#endif


/* Triangulate the polygon defined by plist, and generating vertex p_id.
   Return list of added triangles in list add_ptr. Returns TRUE if 
   successful, FALSE otherwise. This is NOT a general triangulation routine,
   assumes polygon star relative to id
*/

int
smTriangulate(sm,id,plist,add_ptr)
SM *sm;
int id;
LIST *plist,**add_ptr;
{
    LIST *l,*prev,*t;
    FVECT v0,v1,v2,n,p;
    int is_tri,is_convex,cut,t_id,id0,id1,id2,e2,e1,enew;
    double dp;
    static int debug=0;

    VSUB(p,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
    enew = 0;
    is_convex = TRUE;
    cut = is_tri= FALSE;
    l = prev = plist;

    /* get v0,v1 */
    e1 = (int)LIST_DATA(l);
    id0 = E_NTH_VERT(e1,0);
    id1 = E_NTH_VERT(e1,1);
    VSUB(v0,SM_NTH_WV(sm,id0),SM_VIEW_CENTER(sm));
    VSUB(v1,SM_NTH_WV(sm,id1),SM_VIEW_CENTER(sm));   
#ifdef TEST_DRIVER
    Del = TRUE;
    VCOPY(B_V[0],v0);
    VCOPY(B_V[1],v1);
    Bcnt = 2;
    Ncnt = 0;
#endif
    while(l)
    {
      l = LIST_NEXT(l);
      /* Get v2 */
      e2 = (int)LIST_DATA(l);
      id2 = E_NTH_VERT(e2,1);
      VSUB(v2,SM_NTH_WV(sm,id2),SM_VIEW_CENTER(sm));
#ifdef TEST_DRIVER
      VCOPY(B_V[Bcnt++],v2);
#endif
      if(LIST_NEXT(LIST_NEXT(l)) == prev)
      {/* Check if have a triangle */
	   is_tri = TRUE;	
	   break;
      }

      /* determine if v0-v1-v2 is convex:defined clockwise on the sphere-
       so switch orientation
       */
      if(convex_angle(v2,v1,v0))
      {
	  /* test if safe to cut off v0-v1-v2 by testing if p lies outside of
         triangle v0-v1-v2: if so, because plist is the star polygon around p,
          the new edge v2-v0 cannot intersect any existing edges
        */
	VCROSS(n,v0,v2);
        dp = DOT(n,p);
        if(dp  <= 0.0)
	{
	    /* remove edges e1,e2 and add triangle id0,id1,id2 */
          enew = 0;
          t_id = smTriangulate_add_tri(sm,id0,id1,id2,e1,e2,&enew);
	  cut = TRUE;
          *add_ptr = push_data(*add_ptr,t_id);
           /* Insert edge enew into the list, reuse prev list element */
	  LIST_NEXT(prev) = LIST_NEXT(l);
	  LIST_DATA(prev) = e1 = -enew;
           /* If removing head of list- reset plist pointer */
          if(l== plist)
	    plist = prev;
	  /* free list element for e2 */
	  LIST_NEXT(l)=NULL;
	  free_list(l);
	  l = prev;
	  VCOPY(v1,v2);
	  id1 = id2;
	  continue;
	}
      }
      else
	is_convex = FALSE;
      VCOPY(v0,v1);
      VCOPY(v1,v2);
      id0 = id1;
      id1 = id2;
      e1 = e2;	
      /* check if gone around circular list without adding any
	 triangles: prevent infinite loop */
      if(l == plist)
      {
	if(LIST_NEXT(LIST_NEXT(l)) == prev)
	  {/* Check if have a triangle */
	    is_tri = TRUE;	
	    break;
	  }

	if(is_convex)
	  break;
	if(!cut)
	{
#ifdef DEBUG
	  eputs("smTriangulate():Unable to triangulate\n");
#endif
	 free_list(l);
	  while(*add_ptr)
	  {
	    t_id = pop_list(add_ptr);
	    smDelete_tri(sm,t_id);
	  }
	  return(FALSE);
	 }
        
        cut = FALSE;
	is_convex = TRUE;
      }
      prev = l;
    }
    if(is_tri)
    {
      l = LIST_NEXT(l);
      enew = (int)LIST_DATA(l);
      t_id = smTriangulate_add_tri(sm,id0,id1,id2,e1,e2,&enew);
      *add_ptr = push_data(*add_ptr,t_id);
      free_list(l);
    }
    else
      if(!smTriangulateConvex(sm,l,add_ptr))
	return(FALSE);

    /* Set triangle adjacencies based on edge adjacencies */ 
    FOR_ALL_EDGES(enew)
    {
      id0 = E_NTH_TRI(enew,0);
      id1 = E_NTH_TRI(enew,1);
	
      e1 = (T_WHICH_V(SM_NTH_TRI(sm,id0),E_NTH_VERT(enew,0))+2)%3;
      T_NTH_NBR(SM_NTH_TRI(sm,id0),e1) = id1;
      
      e2 = (T_WHICH_V(SM_NTH_TRI(sm,id1),E_NTH_VERT(enew,1))+2)%3;
      T_NTH_NBR(SM_NTH_TRI(sm,id1),e2) = id0;
    }
    return(TRUE);
}

eIn_tri(e,t)
int e;
TRI *t;
{

  if(T_NTH_V(t,0)==E_NTH_VERT(e,0)) 
    return(T_NTH_V(t,1)==E_NTH_VERT(e,1)||T_NTH_V(t,2)==E_NTH_VERT(e,1));
  else
    if(T_NTH_V(t,1)==E_NTH_VERT(e,0))
      return(T_NTH_V(t,0)==E_NTH_VERT(e,1)||T_NTH_V(t,2)==E_NTH_VERT(e,1));
    else if(T_NTH_V(t,2)==E_NTH_VERT(e,0))
      return(T_NTH_V(t,0)==E_NTH_VERT(e,1)||T_NTH_V(t,1)==E_NTH_VERT(e,1));

  return(FALSE);
}

/* Test the new set of triangles for Delaunay condition. 'Edges' contains
   all of the new edges added. The CCW triangle assoc with each edge is
   tested against the opposite vertex of the CW triangle. If the vertex
   lies inside the circle defined by the CCW triangle- the edge is swapped 
   for the opposite diagonal
*/
smFixEdges(sm,add_list)
   SM *sm;
   LIST *add_list;
{
    int e,t0_id,t1_id,e_new,e0,e1,e0_next,e1_next;
    int i,v0_id,v1_id,v2_id,p_id,t0_nid,t1_nid;
    FVECT v0,v1,v2,p,np,v;
    TRI *t0,*t1;

    FOR_ALL_EDGES(e)
    {
	t0_id = E_NTH_TRI(e,0);
	t1_id = E_NTH_TRI(e,1);
	if((t0_id==INVALID) || (t1_id==INVALID))
	{
#ifdef DEBUG
	    error(CONSISTENCY,"smFix_edges: Unassigned edge nbr\n");
#endif
	}
	t0 = SM_NTH_TRI(sm,t0_id);
	t1 = SM_NTH_TRI(sm,t1_id);
	e0 = T_NTH_NBR_PTR(t1_id,t0);
	e1 = T_NTH_NBR_PTR(t0_id,t1);

	v0_id = E_NTH_VERT(e,0);
	v1_id = E_NTH_VERT(e,1);
	v2_id = T_NTH_V(t0,e0);
	p_id = T_NTH_V(t1,e1);

	smDir_in_cone(sm,v0,v0_id);
	smDir_in_cone(sm,v1,v1_id);
	smDir_in_cone(sm,v2,v2_id);
	
	VCOPY(p,SM_NTH_WV(sm,p_id));	
	VSUB(p,p,SM_VIEW_CENTER(sm));
	if(point_in_cone(p,v0,v1,v2))
	{
	   smTris_swap_edge(sm,t0_id,t1_id,e0,e1,&t0_nid,&t1_nid,&add_list);
	    
	   /* Adjust the triangle pointers of the remaining edges to be
	      processed
            */
	    FOR_ALL_EDGES_FROM(e,i)
	    {
	      if(E_NTH_TRI(i,0)==t0_id || E_NTH_TRI(i,0)==t1_id)
	      {
		if(eIn_tri(i,SM_NTH_TRI(sm,t0_nid)))
		  SET_E_NTH_TRI(i,0,t0_nid);
		else
		  SET_E_NTH_TRI(i,0,t1_nid);
	      }

	      if(E_NTH_TRI(i,1)==t0_id || E_NTH_TRI(i,1)==t1_id)
	      {
		if(eIn_tri(i,SM_NTH_TRI(sm,t0_nid)))
		  SET_E_NTH_TRI(i,1,t0_nid);
		else
		  SET_E_NTH_TRI(i,1,t1_nid);
	      }
	    }
	    t0_id = t0_nid;
	    t1_id = t1_nid;
	    e_new = eNew_edge();
	    SET_E_NTH_VERT(e_new,0,p_id);
	    SET_E_NTH_VERT(e_new,1,v2_id);
	    SET_E_NTH_TRI(e_new,0,t0_id);
	    SET_E_NTH_TRI(e_new,1,t1_id);
	}
    }
    /* Add/Delete the appropriate triangles from the stree */
    smUpdate_locator(sm,add_list);
}

/* Remove vertex "id" from the mesh- and retriangulate the resulting
   hole: Returns TRUE if successful, FALSE otherwise.
 */
int
smRemoveVertex(sm,id)
   SM *sm;
   int id;
{
    LIST *b_list,*add_list,*del_list;
    int t_id,i;
    static int cnt=0;
    OBJECT *optr,*os;
    /* generate list of edges that form the boundary of the
       polygon formed by the triangles adjacent to vertex 'id'
     */
    del_list = NULL;
    b_list = smVertexPolygon(sm,id,&del_list);

    add_list = NULL;
    /* Triangulate polygonal hole  */
    if(!smTriangulate(sm,id,b_list,&add_list))
    {
      free_list(del_list);
      return(FALSE);
    }
    else
    {
#ifdef DEBUG
      b_list = del_list;
      while(b_list)
      {
	t_id = LIST_DATA(b_list);
	b_list = LIST_NEXT(b_list);
	T_VALID_FLAG(SM_NTH_TRI(sm,t_id))=-1;
      }
#endif
      while(del_list)
      {
	t_id = pop_list(&del_list);
	smDelete_tri(sm,t_id);
      }     
    }      
    /* Fix up new triangles to be Delaunay-delnode contains set of
     triangles to delete,add_list is the set of new triangles to add
     */
    smFixEdges(sm,add_list);

    return(TRUE);
}
   


 











