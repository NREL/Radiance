/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm_del.c
 */
#include "standard.h"

#include "sm_list.h"
#include "sm_geom.h"
#include "sm.h"

static EDGE Edges[MAX_EDGES];
static int Ecnt=0;


smLocator_remove_tri(sm,t_id)
SM *sm;
int t_id;
{
  STREE *st;
  char found;
  TRI *t;
  FVECT p0,p1,p2;

  st = SM_LOCATOR(sm);

  t = SM_NTH_TRI(sm,t_id);

  smDir(sm,p0,T_NTH_V(t,0));
  smDir(sm,p1,T_NTH_V(t,1));
  smDir(sm,p2,T_NTH_V(t,2));

  found = stRemove_tri(st,t_id,p0,p1,p2);

  return(found);
}

smFree_tri(sm,id)
SM *sm;
int id;
{
  TRI *tri;

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
  if(!SM_IS_NTH_T_BASE(sm,t_id))
  {
    SM_NUM_TRIS(sm)--;
    if(SM_IS_NTH_T_NEW(sm,t_id))
      smNew_tri_cnt--;
  }
  smClear_tri_flags(sm,t_id);

  smFree_tri(sm,t_id);

}



LIST
*smVertex_star_polygon(sm,id)
SM *sm;
int id;
{
    TRI *tri,*t_next;
    LIST *elist,*end,*tlist;
    int t_id,v_next,t_next_id;
    int e;

    elist = end =  NULL;
    /* Get the first triangle adjacent to vertex id */
    t_id = SM_NTH_VERT(sm,id);
    tri = SM_NTH_TRI(sm,t_id);

    
    if((e = eNew_edge()) == SM_INVALID)
    {
#ifdef DEBUG
	eputs("smVertex_star_polygon():Too many edges\n");
#endif
	return(NULL);
    }
    elist = add_data_to_circular_list(elist,&end,e);
    v_next = (T_WHICH_V(tri,id)+1)%3;
    SET_E_NTH_VERT(e,0,T_NTH_V(tri,v_next));
    SET_E_NTH_TRI(e,0,SM_INVALID);
    SET_E_NTH_TRI(e,1,T_NTH_NBR(tri,v_next));
    v_next = (T_WHICH_V(tri,id)+2)%3;
    SET_E_NTH_VERT(e,1,T_NTH_V(tri,v_next));

    
    t_next_id = t_id;
    t_next = tri;

    tlist = push_data(NULL,t_id);

    while((t_next_id = smTri_next_ccw_nbr(sm,t_next,id)) != t_id)
    {	
	if((e = eNew_edge()) == SM_INVALID)
	{
#ifdef DEBUG
	    eputs("smVertex_star_polygon():Too many edges\n");
#endif
	    return(NULL);
	}
	elist = add_data_to_circular_list(elist,&end,e);
	t_next = SM_NTH_TRI(sm,t_next_id);
	v_next = (T_WHICH_V(t_next,id)+1)%3;
	SET_E_NTH_VERT(e,0,T_NTH_V(t_next,v_next));
	SET_E_NTH_TRI(e,0,SM_INVALID);
	SET_E_NTH_TRI(e,1,T_NTH_NBR(t_next,v_next));
	v_next = (T_WHICH_V(t_next,id)+2)%3;
	SET_E_NTH_VERT(e,1,T_NTH_V(t_next,v_next));
	tlist = push_data(tlist,t_next_id);
    }
    while(tlist)
    {
	t_id = (int)pop_list(&tlist);
	/* first remove from point location structure */
	smLocator_remove_tri(sm,t_id);
	smDelete_tri(sm,t_id);
    }
    return(elist);
}

int
smEdge_intersect_polygon(sm,v0,v1,l)
SM *sm;
FVECT v0,v1;
LIST *l;
{
    FVECT e0,e1;
    int e,id_e0,id_e1;
    LIST *el,*eptr;
    
    /* Test the edges in l against v0v1 to see if v0v1 intersects
       any other edges
     */
    
    el = l;

    while(el)
    {
      e = (int)LIST_DATA(el);
      id_e0 = E_NTH_VERT(e,0);
      id_e1 = E_NTH_VERT(e,1);

      smDir(sm,e0,id_e0);
      smDir(sm,e1,id_e1);
      if(spherical_polygon_edge_intersect(v0,v1,e0,e1))
	return(TRUE);

      el = LIST_NEXT(el);
      if(el == l)
	break;
    }
    return(FALSE);
}

int
smFind_next_convex_vertex(sm,id0,id1,v0,v1,l)
   SM *sm;
   int id0,id1;
   FVECT v0,v1;
   LIST *l;
{
    int e,id;
    LIST *el;
    FVECT v;

    /* starting with the end of edge at head of l, search sequentially for
      vertex v such that v0v1v is a convex angle, and the edge v1v does
      not intersect any other edges
   */
    id = SM_INVALID;
    el = l;
    while(id != id0)
    {
	e = (int)LIST_DATA(el);
	id = E_NTH_VERT(e,1);

	smDir(sm,v,id); 

	if(convex_angle(v0,v1,v) && !smEdge_intersect_polygon(sm,v1,v,l))
	   return(id);
	      
	el = LIST_NEXT(el);
	if(el == l)
	   break;
    }
    return(SM_INVALID);
}

int
split_edge_list(id0,id_new,l,lnew)
int id0,id_new;
LIST **l,**lnew;
{
    LIST *list,*lptr,*end;
    int e,e1,e2,new_e;

    e2 = SM_INVALID;
    list = lptr = *l;

    if((new_e = eNew_edge())==SM_INVALID)
     {
#ifdef DEBUG
	    eputs("split_edge_list():Too many edges\n");
#endif
	 return(FALSE);
     }
    SET_E_NTH_VERT(new_e,0,id0);
    SET_E_NTH_VERT(new_e,1,id_new);
    SET_E_NTH_TRI(new_e,0,SM_INVALID);
    SET_E_NTH_TRI(new_e,1,SM_INVALID);
    
    while(e2 != id_new)
    {
	lptr = LIST_NEXT(lptr);
	e = (int)LIST_DATA(lptr);
	e2 = E_NTH_VERT(e,1);
	if(lptr == list)
	{
#ifdef DEBUG	    
	  eputs("split_edge_list():cant find vertex\n");
#endif
	  *lnew = NULL;
	  return(FALSE);
	}

    }
    end = lptr;
    lptr =  LIST_NEXT(lptr);
    list = add_data_to_circular_list(list,&end,-new_e);
    *lnew = list;

    /* now follow other cycle */

    list = lptr;
    e2 = SM_INVALID;
    while(e2 != id0)
    {
	lptr = LIST_NEXT(lptr);
	e = (int)LIST_DATA(lptr);
	e2 = E_NTH_VERT(e,1);
	if(lptr == list)
	{
#ifdef DEBUG	    
	  eputs("split_edge_list():cant find intial vertex\n");
#endif
	  *l = NULL;
	  return(FALSE);
	}

    }
    end = lptr;
    list = add_data_to_circular_list(list,&end,new_e);
    *l = list;
    return(TRUE);
}



LIST
*smTriangulate_convex(sm,plist)
SM *sm;
LIST *plist;
{
    TRI *tri;
    int t_id,e_id0,e_id1,e_id2;
    int v_id0,v_id1,v_id2;
    LIST *lptr;
    int cnt;

    lptr = plist;
    e_id0 = (int)LIST_DATA(lptr);
    v_id0 = E_NTH_VERT(e_id0,0);
    lptr = LIST_NEXT(lptr);
    while(LIST_NEXT(lptr) != plist)
    {
	e_id1 = (int)LIST_DATA(lptr);
	v_id1 = E_NTH_VERT(e_id1,0);
	v_id2 = E_NTH_VERT(e_id1,1);
	/* form a triangle for each triple of with v0 as base of star */
	t_id = smAdd_tri(sm,v_id0,v_id1,v_id2,&tri);
	smLocator_add_tri(sm,t_id,v_id0,v_id1,v_id2);
	/* add which pointer?*/

	lptr = LIST_NEXT(lptr);

	if(LIST_NEXT(lptr) != plist)
	{
	    e_id2 = eNew_edge();
	    SET_E_NTH_VERT(e_id2,0,v_id2);
	    SET_E_NTH_VERT(e_id2,1,v_id0);
	}
	else
	   e_id2 = (int)LIST_DATA(lptr);
	 
	/* set appropriate tri for each edge*/ 
	SET_E_NTH_TRI(e_id0,0,t_id);
	SET_E_NTH_TRI(e_id1,0,t_id);
	SET_E_NTH_TRI(e_id2,0,t_id);

	e_id0 = -e_id2;
    }
    free_list(plist);
}

smTriangulate_elist(sm,plist)
SM *sm;
LIST *plist;
{
    LIST *l,*el1;
    FVECT v0,v1,v2;
    int id0,id1,id2,e,id_next;
    char flipped;
    int debug = TRUE;

    l = plist;
    
    while(l)
    {
	/* get v0,v1,v2 */
      e = (int)LIST_DATA(l);
      id0 = E_NTH_VERT(e,0);
      id1 = E_NTH_VERT(e,1);
      l = LIST_NEXT(l);
      e = (int)LIST_DATA(l);
      id2 = E_NTH_VERT(e,1);

      smDir(sm,v0,id0);
      smDir(sm,v1,id1);
      smDir(sm,v2,id2);
      /* determine if convex (left turn), or concave(right turn) angle */
      if(convex_angle(v0,v1,v2))
      {
	if(l == plist)
	  break;
	else
	  continue;
      }
      /* if concave: add edge and recurse on two sub polygons */
      id_next = smFind_next_convex_vertex(sm,id0,id1,v0,v1,LIST_NEXT(l));
      if(id_next == SM_INVALID)
      {
#ifdef DEBUG
	  eputs("smTriangulate_elist():Unable to find convex vertex\n");
#endif
	  return;
      }
      /* add edge */
      el1 = NULL;
      /* Split edge list l into two lists: one from id1-id_next-id1,
	 and the next from id2-id_next-id2
      */
      debug = split_edge_list(id1,id_next,&l,&el1);
      /* Recurse and triangulate the two edge lists */
      if(debug)
	debug = smTriangulate_elist(sm,l);
      if(debug)
	debug = smTriangulate_elist(sm,el1);

      return(debug);
    }
    smTriangulate_convex(sm,plist);
    return(TRUE);
}

int
smTriangulate_polygon(sm,plist)
SM *sm;
LIST *plist;
{
    int e,id_t0,id_t1,e0,e1;
    TRI *t0,*t1;
    int test;
    
    test = smTriangulate_elist(sm,plist,NULL);

    if(!test)
       return(test);
    FOR_ALL_EDGES(e)
    {
	id_t0 = E_NTH_TRI(e,0);
	id_t1 = E_NTH_TRI(e,1);
	if((id_t0==SM_INVALID) || (id_t1==SM_INVALID))
	{
#ifdef DEBUG
	   eputs("smTriangulate_polygon(): Unassigned edge neighbor\n");
#endif
	    continue;
	}
	t0 = SM_NTH_TRI(sm,id_t0);
	t1 = SM_NTH_TRI(sm,id_t1);
	
	e0 = T_WHICH_V(t0,E_NTH_VERT(e,0));
	T_NTH_NBR(t0,e0) = id_t1;

	e1 = T_WHICH_V(t1,E_NTH_VERT(e,1));
	T_NTH_NBR(t1,e1) = id_t0;
    }
    return(test);
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
smFix_edges(sm)
   SM *sm;
{
    int e,id_t0,id_t1,e_new,e0,e1,e0_next,e1_next;
    TRI *t0,*t1,*nt0,*nt1;
    int i,id_v0,id_v1,id_v2,id_p,nid_t0,nid_t1;
    FVECT v0,v1,v2,p,np,v;
    LIST *add,*del;
    
    add = del = NULL;
    FOR_ALL_EDGES(e)
    {
	id_t0 = E_NTH_TRI(e,0);
	id_t1 = E_NTH_TRI(e,1);
	if((id_t0==SM_INVALID) || (id_t1==SM_INVALID))
	{
#ifdef DEBUG
	    eputs("smFix_edges: Unassigned edge nbr\n");
#endif
	    continue;
	}
	t0 = SM_NTH_TRI(sm,id_t0);
	t1 = SM_NTH_TRI(sm,id_t1);
	
	e0 = T_WHICH_V(t0,E_NTH_VERT(e,0));
	e1 = T_WHICH_V(t1,E_NTH_VERT(-e,0));
	e0_next = (e0+2)%3;
	e1_next = (e1+2)%3;
	id_v0 = E_NTH_VERT(e,0);
	id_v1 = E_NTH_VERT(e,1);
	id_v2 = T_NTH_V(t0,e0_next);
	id_p = T_NTH_V(t1,e1_next);

	smDir(sm,v0,id_v0);
	smDir(sm,v1,id_v1);
	smDir(sm,v2,id_v2);
	
	VCOPY(p,SM_NTH_WV(sm,id_p));	
	VSUB(p,p,SM_VIEW_CENTER(sm));
	if(point_in_cone(p,v0,v1,v2))
	{
	    smTris_swap_edge(sm,id_t0,id_t1,e0,e1,&nid_t0,&nid_t1,&add,&del);
	    
	    nt0 = SM_NTH_TRI(sm,nid_t0);
	    nt1 = SM_NTH_TRI(sm,nid_t1);
	    FOR_ALL_EDGES_FROM(e,i)
	    {
	      if(E_NTH_TRI(i,0)==id_t0 || E_NTH_TRI(i,0)==id_t1)
	      {
		if(eIn_tri(i,nt0))
		  SET_E_NTH_TRI(i,0,nid_t0);
		else
		  SET_E_NTH_TRI(i,0,nid_t1);
	      }

	      if(E_NTH_TRI(i,1)==id_t0 || E_NTH_TRI(i,1)==id_t1)
	      {
		if(eIn_tri(i,nt0))
		  SET_E_NTH_TRI(i,1,nid_t0);
		else
		  SET_E_NTH_TRI(i,1,nid_t1);
	      }
	    }
	    id_t0 = nid_t0;
	    id_t1 = nid_t1;
	    e_new = eNew_edge();
	    SET_E_NTH_VERT(e_new,0,id_p);
	    SET_E_NTH_VERT(e_new,1,id_v2);
	    SET_E_NTH_TRI(e_new,0,id_t0);
	    SET_E_NTH_TRI(e_new,1,id_t1);
	}
    }
    smUpdate_locator(sm,add,del);
}

int
smMesh_remove_vertex(sm,id)
   SM *sm;
   int id;
{
    int tri;
    LIST *elist;
    int cnt,debug;
    /* generate list of vertices that form the boundary of the
       star polygon formed by vertex id and all of its adjacent
       triangles
     */
    eClear_edges();
    elist = smVertex_star_polygon(sm,id);
    if(!elist)
       return(FALSE);

    /* Triangulate spherical polygon */
    debug = smTriangulate_polygon(sm,elist);


    /* Fix up new triangles to be Delaunay */
    if(debug)
       smFix_edges(sm);

    return(TRUE);
}
   
/* Remove point from samples, and from mesh. Delete any triangles
   adjacent to the point and re-triangulate the hole
   Return TRUE is point found , FALSE otherwise
*/
int
smDelete_point(sm,id)
SM *sm;
int id;
{

    /* Remove the corresponding vertex from the mesh */
    smMesh_remove_vertex(sm,id);
    /* Free the sample point */
    smDelete_sample(sm,id);
    return(TRUE);
}














