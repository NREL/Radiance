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

#define remove_tri_compress remove_tri
remove_tri(qtptr,fptr,tptr)
   QUADTREE *qtptr;
   int *fptr;
   int *tptr;
{
    int n;

    if(QT_IS_EMPTY(*qtptr))
      return;
    if(QT_LEAF_IS_FLAG(*qtptr))
      return;

    n = QT_SET_CNT(qtqueryset(*qtptr))-1;
    *qtptr = qtdelelem(*qtptr,*tptr);
    if(n  == 0)
      (*fptr) |= QT_COMPRESS;
    if(!QT_FLAG_FILL_TRI(*fptr))
      (*fptr)++;
}


smLocator_remove_tri(sm,t_id,v0_id,v1_id,v2_id)
SM *sm;
int t_id;
int v0_id,v1_id,v2_id;
{
  STREE *st;
  FVECT v0,v1,v2;
  
  st = SM_LOCATOR(sm);

  VSUB(v0,SM_NTH_WV(sm,v0_id),SM_VIEW_CENTER(sm));
  VSUB(v1,SM_NTH_WV(sm,v1_id),SM_VIEW_CENTER(sm));
  VSUB(v2,SM_NTH_WV(sm,v2_id),SM_VIEW_CENTER(sm));

  qtClearAllFlags();
  
  stApply_to_tri(st,v0,v1,v2,remove_tri,remove_tri_compress,&t_id);

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
    SM_SAMPLE_TRIS(sm)--;
    if(SM_IS_NTH_T_NEW(sm,t_id))
      smNew_tri_cnt--;
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

LIST 
*smVertex_star_polygon(sm,id,delptr)
SM *sm;
int id;
QUADTREE *delptr;
{
    TRI *tri,*t_next;
    LIST *elist,*end;
    int t_id,v_next,t_next_id;
    int e;
    OBJECT del_set[2];

    elist = end =  NULL;
    /* Get the first triangle adjacent to vertex id */
    t_id = SM_NTH_VERT(sm,id);
    tri = SM_NTH_TRI(sm,t_id);

    if((e = eNew_edge()) == INVALID)
      return(NULL);

    v_next = (T_WHICH_V(tri,id)+1)%3;
    SET_E_NTH_VERT(e,0,T_NTH_V(tri,v_next));
    SET_E_NTH_TRI(e,0,INVALID);
    SET_E_NTH_TRI(e,1,T_NTH_NBR(tri,v_next));
    v_next = (T_WHICH_V(tri,id)+2)%3;
    SET_E_NTH_VERT(e,1,T_NTH_V(tri,v_next));
    elist = add_data_to_circular_list(elist,&end,e);

    t_next_id = t_id;
    t_next = tri;

    del_set[0] =1; del_set[1] = t_id;
    *delptr = qtnewleaf(del_set);

    while((t_next_id = T_NTH_NBR(t_next,v_next)) != t_id)
    {	
      if((e = eNew_edge()) == INVALID)
	return(NULL);

      t_next = SM_NTH_TRI(sm,t_next_id);
      v_next = (T_WHICH_V(t_next,id)+1)%3;

      SET_E_NTH_VERT(e,0,T_NTH_V(t_next,v_next));
      SET_E_NTH_TRI(e,0,INVALID);
      SET_E_NTH_TRI(e,1,T_NTH_NBR(t_next,v_next));
      v_next = (T_WHICH_V(t_next,id)+2)%3;
      SET_E_NTH_VERT(e,1,T_NTH_V(t_next,v_next));
      elist = add_data_to_circular_list(elist,&end,e);


      if(qtinset(*delptr,t_next_id))
	{
#ifdef DEBUG
	  eputs("smVertex_star_polygon(): id already in set\n");
#endif	  
	  free_list(elist);
	  return(NULL);
	}
      else
	qtaddelem(*delptr,t_next_id);
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

      VSUB(e0,SM_NTH_WV(sm,id_e0),SM_VIEW_CENTER(sm));
      VSUB(e1,SM_NTH_WV(sm,id_e1),SM_VIEW_CENTER(sm));
      if(sedge_intersect(v0,v1,e0,e1))
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
    id = INVALID;
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
    return(INVALID);
}

int
split_edge_list(id0,id_new,l,lnew)
int id0,id_new;
LIST **l,**lnew;
{
    LIST *list,*lptr,*end;
    int e,e1,e2,new_e;

    e2 = INVALID;
    list = lptr = *l;

    if((new_e = eNew_edge())==INVALID)
     {
#ifdef DEBUG
	    eputs("split_edge_list():Too many edges\n");
#endif
	 return(FALSE);
     }
    SET_E_NTH_VERT(new_e,0,id0);
    SET_E_NTH_VERT(new_e,1,id_new);
    SET_E_NTH_TRI(new_e,0,INVALID);
    SET_E_NTH_TRI(new_e,1,INVALID);
    
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
    e2 = INVALID;
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


int
smTriangulate_convex(sm,plist,add_ptr)
SM *sm;
LIST *plist,**add_ptr;
{
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
	t_id = smAdd_tri(sm,v_id0,v_id1,v_id2);
	*add_ptr = push_data(*add_ptr,t_id);

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
    return(TRUE);
}
int
smTriangulate_elist(sm,plist,add_ptr)
SM *sm;
LIST *plist,**add_ptr;
{
    LIST *l,*el1;
    FVECT v0,v1,v2;
    int id0,id1,id2,e,id_next;
    char flipped;
    int done;

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
      if(id_next == INVALID)
      {
#ifdef DEBUG
	  eputs("smTriangulate_elist():Unable to find convex vertex\n");
#endif
	  return(FALSE);
      }
      /* add edge */
      el1 = NULL;
      /* Split edge list l into two lists: one from id1-id_next-id1,
	 and the next from id2-id_next-id2
      */
      split_edge_list(id1,id_next,&l,&el1);
      /* Recurse and triangulate the two edge lists */
      done = smTriangulate_elist(sm,l,add_ptr);
      if(done)
	done = smTriangulate_elist(sm,el1,add_ptr);
      return(done);
    }
    done = smTriangulate_convex(sm,plist,add_ptr);
    return(done);
}

int
smTriangulate_add_tri(sm,id0,id1,id2,e0,e1,e2ptr)
SM *sm;
int id0,id1,id2,e0,e1,*e2ptr;
{
  int t_id;
  int e2;

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
smTriangulate_elist_new(sm,id,plist,add_ptr)
SM *sm;
int id;
LIST *plist,**add_ptr;
{
    LIST *l,*prev,*t;
    FVECT v0,v1,v2,n,p;
    int is_tri,loop,t_id,id0,id1,id2,e2,eprev,enext;
    double dp;

    smDir(sm,p,id);
    enext=0;
    is_tri= loop = FALSE;
    l = prev = plist;
    /* get v0,v1,v2 */
    eprev = (int)LIST_DATA(l);
    id0 = E_NTH_VERT(eprev,0);
    id1 = E_NTH_VERT(eprev,1);
    smDir(sm,v0,id0);
    smDir(sm,v1,id1);   
    while(l)
    {
      l = LIST_NEXT(l);
      e2 = (int)LIST_DATA(l);
      id2 = E_NTH_VERT(e2,1);
      /* Check if have a triangle */
      if(LIST_NEXT(LIST_NEXT(l)) == prev)
      {
	is_tri = TRUE;
	break;
      }
      if(LIST_NEXT(l) == plist)
      {
	if(!loop)
	  loop = 1;
	else
	  loop++;
	if(loop > 3)
	  break;
      }
      smDir(sm,v2,id2);
      /* determine if convex (left turn), or concave(right turn) angle */
      if(!convex_angle(v0,v1,v2))
      {
	VCOPY(v0,v1);
	VCOPY(v1,v2);
	id0 = id1;
	id1 = id2;
	prev = l;
	eprev = e2;
	continue;
      }
      VCROSS(n,v0,v2);
      dp = DOT(n,p);
      if(loop <=1 && (!ZERO(dp) && dp  < 0.0))
      {
	VCOPY(v0,v1);
	VCOPY(v1,v2);
	id0 = id1;
	id1 = id2;
	eprev = e2;
	prev = l;
	continue;
      }
      loop = FALSE;

      enext = 0;
      t_id = smTriangulate_add_tri(sm,id0,id1,id2,eprev,e2,&enext);
      *add_ptr = push_data(*add_ptr,t_id);

      LIST_NEXT(prev) = LIST_NEXT(l);
      LIST_DATA(prev) = eprev = -enext;
      LIST_NEXT(l)=NULL;
      if(l== plist)
	plist = prev;
      free_list(l);
      l = prev;
      VCOPY(v1,v2);
      id1 = id2;
    }
    if(is_tri)
    {
      l = LIST_NEXT(l);
      enext = (int)LIST_DATA(l);
      t_id = smTriangulate_add_tri(sm,id0,id1,id2,eprev,e2,&enext);
      *add_ptr = push_data(*add_ptr,t_id);
      free_list(l);
     }
    else
      {
#ifdef DEBUG       
	eputs("smTriangulate_elist()Unable to triangulate\n");
#endif
	return(FALSE);
      }
    return(TRUE);
}

int
smTriangulate(sm,p_id,plist,add_ptr)
SM *sm;
int p_id;
LIST *plist,**add_ptr;
{
    int e,id_t0,id_t1,e0,e1;
    int test;

    test = smTriangulate_elist_new(sm,p_id,plist,add_ptr);
#if 0
    test = smTriangulate_elist(sm,plist,add_ptr);
#endif

    if(!test)
       return(test);

    FOR_ALL_EDGES(e)
    {
	id_t0 = E_NTH_TRI(e,0);
	id_t1 = E_NTH_TRI(e,1);
	if((id_t0==INVALID) || (id_t1==INVALID))
	{
#ifdef DEBUG
	   eputs("smTriangulate(): Unassigned edge neighbor\n");
#endif
	    continue;
	}
	
	e0 = T_WHICH_V(SM_NTH_TRI(sm,id_t0),E_NTH_VERT(e,0));
	T_NTH_NBR(SM_NTH_TRI(sm,id_t0),e0) = id_t1;

	e1 = T_WHICH_V(SM_NTH_TRI(sm,id_t1),E_NTH_VERT(e,1));
	T_NTH_NBR(SM_NTH_TRI(sm,id_t1),e1) = id_t0;
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

smFix_edges(sm,add_list,delptr)
   SM *sm;
   LIST *add_list;
   QUADTREE *delptr;

{
    int e,t0_id,t1_id,e_new,e0,e1,e0_next,e1_next;
    int i,v0_id,v1_id,v2_id,p_id,t0_nid,t1_nid;
    FVECT v0,v1,v2,p,np,v;

    FOR_ALL_EDGES(e)
    {
	t0_id = E_NTH_TRI(e,0);
	t1_id = E_NTH_TRI(e,1);
	if((t0_id==INVALID) || (t1_id==INVALID))
	{
#ifdef DEBUG
	    eputs("smFix_edges: Unassigned edge nbr\n");
#endif
	    continue;
	}
	e0 = T_WHICH_V(SM_NTH_TRI(sm,t0_id),E_NTH_VERT(e,0));
	e1 = T_WHICH_V(SM_NTH_TRI(sm,t1_id),E_NTH_VERT(-e,0));
	e0_next = (e0+2)%3;
	e1_next = (e1+2)%3;
	v0_id = E_NTH_VERT(e,0);
	v1_id = E_NTH_VERT(e,1);
	v2_id = T_NTH_V(SM_NTH_TRI(sm,t0_id),e0_next);
	p_id = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1_next);

	smDir_in_cone(sm,v0,v0_id);
	smDir_in_cone(sm,v1,v1_id);
	smDir_in_cone(sm,v2,v2_id);
	
	VCOPY(p,SM_NTH_WV(sm,p_id));	
	VSUB(p,p,SM_VIEW_CENTER(sm));
	if(point_in_cone(p,v0,v1,v2))
	{
	   smTris_swap_edge(sm,t0_id,t1_id,e0,e1,&t0_nid,&t1_nid,&add_list,
			    delptr);
	    
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
    smUpdate_locator(sm,add_list,qtqueryset(*delptr));
}

int
smMesh_remove_vertex(sm,id)
   SM *sm;
   int id;
{
    int t_id;
    LIST *elist,*add_list;
    int cnt,debug;
    QUADTREE delnode;
    /* generate list of vertices that form the boundary of the
       star polygon formed by vertex id and all of its adjacent
       triangles
     */
    eClear_edges();
    elist = smVertex_star_polygon(sm,id,&delnode);

    if(!elist)
    {
#ifdef DEBUG
      eputs("smMesh_remove_vertex(): Unable to remove vertex");
#endif
      qtfreeleaf(delnode);
      return(FALSE);
    }
    add_list = NULL;
    /* Triangulate spherical polygon */
    if(!smTriangulate(sm,id,elist,&add_list))
    {
      while(add_list)
      {
	t_id = pop_list(&add_list);
	smDelete_tri(sm,t_id);
      }
      qtfreeleaf(delnode);
      return(FALSE);
    }
    /* Fix up new triangles to be Delaunay */
    smFix_edges(sm,add_list,&delnode);

    qtfreeleaf(delnode);
    return(TRUE);
}
   














