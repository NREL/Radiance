/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm.c
 */
#include "standard.h"
#include "sm_flag.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm_qtree.h"
#include "sm_stree.h"
#include "sm.h"


SM *smMesh = NULL;
double smDist_sum=0;
int smNew_tri_cnt=0;
double smMinSampDiff = 1e-4;

static FVECT smDefault_base[4] = { {SQRT3_INV, SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, -SQRT3_INV, SQRT3_INV},
			  {-SQRT3_INV, SQRT3_INV, -SQRT3_INV},
			  {SQRT3_INV, -SQRT3_INV, -SQRT3_INV}};
static int smTri_verts[4][3] = { {2,1,0},{3,2,0},{1,3,0},{2,3,1}};

static int smBase_nbrs[4][3] =  { {3,2,1},{3,0,2},{3,1,0},{1,2,0}};

#ifdef TEST_DRIVER
VIEW  Current_View = {0,{0,0,0},{0,0,-1},{0,1,0},60,60,0};
int Pick_cnt =0;
int Pick_tri = -1,Picking = FALSE,Pick_samp=-1;
FVECT Pick_point[500],Pick_origin,Pick_dir;
FVECT  Pick_v0[500], Pick_v1[500], Pick_v2[500];
FVECT P0,P1,P2;
FVECT FrustumNear[4],FrustumFar[4];
double dev_zmin=.01,dev_zmax=1000;
#endif

smDir(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    VSUB(ps,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm)); 
    normalize(ps);
}

smDir_in_cone(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    
    VSUB(ps,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm)); 
    normalize(ps);
}

smClear_flags(sm,which)
SM *sm;
int which;
{
  int i;

  if(which== -1)
    for(i=0; i < T_FLAGS;i++)
      bzero(SM_NTH_FLAGS(sm,i),FLAG_BYTES(SM_MAX_TRIS(sm)));
  else
    bzero(SM_NTH_FLAGS(sm,which),FLAG_BYTES(SM_MAX_TRIS(sm)));
}

/* Given an allocated mesh- initialize */
smInit_mesh(sm)
    SM *sm;
{
    /* Reset the triangle counters */
    SM_NUM_TRI(sm) = 0;
    SM_SAMPLE_TRIS(sm) = 0;
    SM_FREE_TRIS(sm) = -1;
    smClear_flags(sm,-1);
}


/* Clear the SM for reuse: free any extra allocated memory:does initialization
   as well
*/
smClear(sm)
SM *sm;
{
  smClear_samples(sm);
  smClear_locator(sm);
  smInit_mesh(sm);
}

static int realloc_cnt=0;

int
smRealloc_mesh(sm)
SM *sm;
{
  int fbytes,i,max_tris,max_verts;
  
  if(realloc_cnt <=0)
    realloc_cnt = SM_MAX_TRIS(sm);

  max_tris = SM_MAX_TRIS(sm) + realloc_cnt;
  fbytes = FLAG_BYTES(max_tris);

  if(!(SM_TRIS(sm) = (TRI *)realloc(SM_TRIS(sm),max_tris*sizeof(TRI))))
    goto memerr;

  for(i=0; i< T_FLAGS; i++)
   if(!(SM_NTH_FLAGS(sm,i)=(int4 *)realloc((char *)SM_NTH_FLAGS(sm,i),fbytes)))
	goto memerr;

  SM_MAX_TRIS(sm) = max_tris;
  return(max_tris);

memerr:
	error(SYSTEM, "out of memory in smRealloc_mesh()");
}

/* Allocate and initialize a new mesh with max_verts and max_tris */
int
smAlloc_mesh(sm,max_verts,max_tris)
SM *sm;
int max_verts,max_tris;
{
    int fbytes,i;

    fbytes = FLAG_BYTES(max_tris);

    if(!(SM_TRIS(sm) = (TRI *)realloc(NULL,max_tris*sizeof(TRI))))
      goto memerr;

    if(!(SM_VERTS(sm) = (VERT *)realloc(NULL,max_verts*sizeof(VERT))))
      goto memerr;

    for(i=0; i< T_FLAGS; i++)
      if(!(SM_NTH_FLAGS(sm,i)=(int4 *)realloc(NULL,fbytes)))
	goto memerr;

    SM_MAX_VERTS(sm) = max_verts;
    SM_MAX_TRIS(sm) = max_tris;

    realloc_cnt = max_verts >> 1;

    smInit_mesh(sm);

    return(max_tris);
memerr:
	error(SYSTEM, "out of memory in smAlloc_mesh()");
}


int
smAlloc_tri(sm)
SM *sm;
{
  int id;

  /* First check if there are any tris on the free list */
  /* Otherwise, have we reached the end of the list yet?*/
  if(SM_NUM_TRI(sm) < SM_MAX_TRIS(sm))
    return(SM_NUM_TRI(sm)++);

  if((id = SM_FREE_TRIS(sm)) != -1)
  {
    SM_FREE_TRIS(sm) =  T_NEXT_FREE(SM_NTH_TRI(sm,id));
    return(id);
  }

  /*Else: realloc */
  smRealloc_mesh(sm);
  return(SM_NUM_TRI(sm)++);
}

smFree_mesh(sm)
SM *sm;
{
  int i;

  if(SM_TRIS(sm))
    free(SM_TRIS(sm));
  if(SM_VERTS(sm))
    free(SM_VERTS(sm));
  for(i=0; i< T_FLAGS; i++)
    if(SM_NTH_FLAGS(sm,i))
      free(SM_NTH_FLAGS(sm,i));
}

  
/* Initialize/clear global smL sample list for at least n samples */
smAlloc(max_samples)
   register int max_samples;
{
  unsigned nbytes;
  register unsigned i;
  int total_points;
  int max_tris,n;

  n = max_samples;
  /* If this is the first call, allocate sample,vertex and triangle lists */
  if(!smMesh)
  {
    if(!(smMesh = (SM *)malloc(sizeof(SM))))
       error(SYSTEM,"smAlloc():Unable to allocate memory\n");
    bzero(smMesh,sizeof(SM));
  }
  else
  {   /* If existing structure: first deallocate */
    smFree_mesh(smMesh);
    smFree_samples(smMesh);
    smFree_locator(smMesh);
 }

  /* First allocate at least n samples + extra points:at least enough
   necessary to form the BASE MESH- Default = 4;
  */
  SM_SAMP(smMesh) = sAlloc(&n,SM_EXTRA_POINTS);

  total_points = n + SM_EXTRA_POINTS;

  max_tris = total_points*4;
  /* Now allocate space for mesh vertices and triangles */
  max_tris = smAlloc_mesh(smMesh, total_points, max_tris);

  /* Initialize the structure for point,triangle location.
   */
  smAlloc_locator(smMesh);
}



smInit_sm(sm,vp)
SM *sm;
FVECT vp;
{

  smDist_sum = 0;
  smNew_tri_cnt = 0;

  VCOPY(SM_VIEW_CENTER(sm),vp);
  smInit_locator(sm,vp);
  smInit_samples(sm);
  smInit_mesh(sm);  
  smCreate_base_mesh(sm,SM_DEFAULT);
}

/*
 * int
 * smInit(n)		: Initialize/clear data structures for n entries
 * int	n;
 *
 * This routine allocates/initializes the sample, mesh, and point-location
 * structures for at least n samples.
 * If n is <= 0, then clear data structures.  Returns number samples
 * actually allocated.
 */

int
smInit(n)
   register int	n;
{
  int max_vertices;


  /* If n <=0, Just clear the existing structures */
  if(n <= 0)
  {
    smClear(smMesh);
    return(0);
  }

  /* Total mesh vertices includes the sample points and the extra vertices
     to form the base mesh
  */
  max_vertices = n + SM_EXTRA_POINTS;

  /* If the current mesh contains enough room, clear and return */
  if(smMesh && (max_vertices <= SM_MAX_VERTS(smMesh)) && SM_MAX_SAMP(smMesh) <=
     n && SM_MAX_POINTS(smMesh) <= max_vertices)
  {
    smClear(smMesh);
    return(SM_MAX_SAMP(smMesh));
  }
  /* Otherwise- mesh must be allocated with the appropriate number of
    samples
  */
  smAlloc(n);

  return(SM_MAX_SAMP(smMesh));
}


smLocator_apply_func(sm,v0,v1,v2,edge_func,tri_func,argptr)
   SM *sm;
   FVECT v0,v1,v2;
   int (*edge_func)();
   int (*tri_func)();
   int *argptr;
{
  STREE *st;
  FVECT p0,p1,p2;

  st = SM_LOCATOR(sm);

  VSUB(p0,v0,SM_VIEW_CENTER(sm));
  VSUB(p1,v1,SM_VIEW_CENTER(sm));
  VSUB(p2,v2,SM_VIEW_CENTER(sm));

  stApply_to_tri(st,p0,p1,p2,edge_func,tri_func,argptr);

}

QUADTREE 
delete_tri_set(qt,optr,del_set)
QUADTREE qt;
OBJECT *optr,*del_set;
{

  int set_size,i,t_id;
  OBJECT *rptr,r_set[QT_MAXSET+1];
  OBJECT *tptr,t_set[QT_MAXSET+1],*sptr;

  /* First need to check if set size larger than QT_MAXSET: if so
     need to allocate larger array
     */
  if((set_size = MAX(QT_SET_CNT(optr),QT_SET_CNT(del_set))) >QT_MAXSET)
    rptr = (OBJECT *)malloc((set_size+1)*sizeof(OBJECT));
  else
    rptr = r_set;
  if(!rptr)
    goto memerr;
  setintersect(rptr,del_set,optr);
	  
  if(QT_SET_CNT(rptr) > 0)
  {
    /* First need to check if set size larger than QT_MAXSET: if so
       need to allocate larger array
       */
    sptr = QT_SET_PTR(rptr);
    for(i = QT_SET_CNT(rptr); i > 0; i--)
      {
	t_id = QT_SET_NEXT_ELEM(sptr);
	qt = qtdelelem(qt,t_id);
      }
  }
  /* If we allocated memory: free it */
  if(rptr != r_set)
    free(rptr);

  return(qt);
memerr:
    error(SYSTEM,"delete_tri_set():Unable to allocate memory");
}

QUADTREE
expand_node(qt,q0,q1,q2,optr,n)
QUADTREE qt;
FVECT q0,q1,q2;
OBJECT *optr;
int n;
{
  OBJECT *tptr,t_set[QT_MAXSET+1];
  int i,t_id,found;
  TRI *t;
  FVECT v0,v1,v2;

  if(QT_SET_CNT(optr) > QT_MAXSET)
    tptr = (OBJECT *)malloc((QT_SET_CNT(optr)+1)*sizeof(OBJECT));
  else
    tptr = t_set;
  if(!tptr)
    goto memerr;

  qtgetset(tptr,qt);
  /* If set size exceeds threshold: subdivide cell and reinsert tris*/
  qtfreeleaf(qt);
  qtSubdivide(qt);

  for(optr = QT_SET_PTR(tptr),i=QT_SET_CNT(tptr); i > 0; i--)
  {
    t_id = QT_SET_NEXT_ELEM(optr);
    t = SM_NTH_TRI(smMesh,t_id);
    if(!T_IS_VALID(t))
      continue;
    VSUB(v0,SM_T_NTH_WV(smMesh,t,0),SM_VIEW_CENTER(smMesh));
    VSUB(v1,SM_T_NTH_WV(smMesh,t,1),SM_VIEW_CENTER(smMesh));
    VSUB(v2,SM_T_NTH_WV(smMesh,t,2),SM_VIEW_CENTER(smMesh));
    qt = qtAdd_tri(qt,q0,q1,q2,v0,v1,v2,t_id,n);
  }
  /* If we allocated memory: free it */
  if( tptr != t_set)
    free(tptr);

  return(qt);
memerr:
    error(SYSTEM,"expand_node():Unable to allocate memory");
}

add_tri_expand(qtptr,f,argptr,q0,q1,q2,t0,t1,t2,n)
QUADTREE *qtptr;
int *f;
ADD_ARGS *argptr;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int n;
{
  int t_id;
  OBJECT *optr,*del_set;

  t_id = argptr->t_id;

  if(QT_IS_EMPTY(*qtptr))
  {
    *qtptr = qtaddelem(*qtptr,t_id);
    return;
  }
  if(!QT_LEAF_IS_FLAG(*qtptr))
  {
    optr = qtqueryset(*qtptr);

    if(del_set=argptr->del_set)
      *qtptr = delete_tri_set(*qtptr,optr,del_set);
    *qtptr = qtaddelem(*qtptr,t_id);
  }
  if (n >= QT_MAX_LEVELS)
    return;
  optr = qtqueryset(*qtptr);
  if(QT_SET_CNT(optr) < QT_SET_THRESHOLD)
    return;
  *qtptr = expand_node(*qtptr,q0,q1,q2,optr,n);    
}



add_tri(qtptr,fptr,argptr)
   QUADTREE *qtptr;
   int *fptr;
   ADD_ARGS *argptr;
{

  OBJECT *optr,*del_set;
  int t_id;

  t_id = argptr->t_id;


  if(QT_IS_EMPTY(*qtptr))
  {
    *qtptr = qtaddelem(*qtptr,t_id);
    if(!QT_FLAG_FILL_TRI(*fptr))
      (*fptr)++;
    return;
  }
  if(QT_LEAF_IS_FLAG(*qtptr))
    return;
  
  optr = qtqueryset(*qtptr);
  
  if(del_set = argptr->del_set)
    *qtptr = delete_tri_set(*qtptr,optr,del_set);

  if(!QT_IS_EMPTY(*qtptr))
    {
      optr = qtqueryset(*qtptr);
      if(QT_SET_CNT(optr) >= QT_SET_THRESHOLD)
	(*fptr) |= QT_EXPAND;
    }
  if(!QT_FLAG_FILL_TRI(*fptr))
    (*fptr)++;
  *qtptr = qtaddelem(*qtptr,t_id);
  
}


smLocator_add_tri(sm,t_id,v0_id,v1_id,v2_id,del_set)
SM *sm;
int t_id;
int v0_id,v1_id,v2_id;
OBJECT *del_set;
{
  STREE *st;
  FVECT v0,v1,v2;
  ADD_ARGS args;

  st = SM_LOCATOR(sm);


  VSUB(v0,SM_NTH_WV(sm,v0_id),SM_VIEW_CENTER(sm));
  VSUB(v1,SM_NTH_WV(sm,v1_id),SM_VIEW_CENTER(sm));
  VSUB(v2,SM_NTH_WV(sm,v2_id),SM_VIEW_CENTER(sm));

  qtClearAllFlags();
  args.t_id = t_id;
  args.del_set = del_set;

  stApply_to_tri(st,v0,v1,v2,add_tri,add_tri_expand,&args);

}

/* Add a triangle to the base array with vertices v1-v2-v3 */
int
smAdd_tri(sm, v0_id,v1_id,v2_id)
SM *sm;
int v0_id,v1_id,v2_id;
{
    int t_id;
    TRI *t;

    t_id = smAlloc_tri(sm);

    if(t_id == -1)
      return(t_id);

    t = SM_NTH_TRI(sm,t_id);

    T_CLEAR_NBRS(t);
    /* set the triangle vertex ids */
    T_NTH_V(t,0) = v0_id;
    T_NTH_V(t,1) = v1_id;
    T_NTH_V(t,2) = v2_id;

    SM_NTH_VERT(sm,v0_id) = t_id;
    SM_NTH_VERT(sm,v1_id) = t_id;
    SM_NTH_VERT(sm,v2_id) = t_id;

    if(SM_BASE_ID(sm,v0_id) || SM_BASE_ID(sm,v1_id) || SM_BASE_ID(sm,v2_id))
    {
      smClear_tri_flags(sm,t_id);
      SM_SET_NTH_T_BASE(sm,t_id);
    }
    else
    {
      SM_CLR_NTH_T_BASE(sm,t_id);
      SM_SET_NTH_T_ACTIVE(sm,t_id);
      SM_SET_NTH_T_NEW(sm,t_id);
      S_SET_FLAG(T_NTH_V(t,0));
      S_SET_FLAG(T_NTH_V(t,1));
      S_SET_FLAG(T_NTH_V(t,2));
      SM_SAMPLE_TRIS(sm)++;
      smNew_tri_cnt++;
    }

    /* return initialized triangle */
    return(t_id);
}


void
smTris_swap_edge(sm,t_id,t1_id,e,e1,tn_id,tn1_id,add_ptr,delptr)
   SM *sm;
   int t_id,t1_id;
   int e,e1;
   int *tn_id,*tn1_id;
   LIST **add_ptr;
   QUADTREE *delptr;

{
    int verts[3],enext,eprev,e1next,e1prev;
    TRI *n;
    FVECT p1,p2,p3;
    int ta_id,tb_id;
    /* swap diagonal (e relative to t, and e1 relative to t1)
      defined by quadrilateral
      formed by t,t1- swap for the opposite diagonal
   */
    enext = (e+1)%3;
    eprev = (e+2)%3;
    e1next = (e1+1)%3;
    e1prev = (e1+2)%3;
    verts[e] = T_NTH_V(SM_NTH_TRI(sm,t_id),e);
    verts[enext] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1prev);
    verts[eprev] = T_NTH_V(SM_NTH_TRI(sm,t_id),eprev);
    ta_id = smAdd_tri(sm,verts[0],verts[1],verts[2]);
    *add_ptr = push_data(*add_ptr,ta_id);
    verts[e1] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1);
    verts[e1next] = T_NTH_V(SM_NTH_TRI(sm,t_id),eprev);
    verts[e1prev] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1prev);
    tb_id = smAdd_tri(sm,verts[0],verts[1],verts[2]);
    *add_ptr = push_data(*add_ptr,tb_id);

    /* set the neighbors */
    T_NTH_NBR(SM_NTH_TRI(sm,ta_id),e) = T_NTH_NBR(SM_NTH_TRI(sm,t1_id),e1next);
    T_NTH_NBR(SM_NTH_TRI(sm,tb_id),e1) = T_NTH_NBR(SM_NTH_TRI(sm,t_id),enext);
    T_NTH_NBR(SM_NTH_TRI(sm,ta_id),enext) =tb_id;
    T_NTH_NBR(SM_NTH_TRI(sm,tb_id),e1next) = ta_id;
    T_NTH_NBR(SM_NTH_TRI(sm,ta_id),eprev)=T_NTH_NBR(SM_NTH_TRI(sm,t_id),eprev);
    T_NTH_NBR(SM_NTH_TRI(sm,tb_id),e1prev)=
      T_NTH_NBR(SM_NTH_TRI(sm,t1_id),e1prev);    

    /* Reset neighbor pointers of original neighbors */
    n = SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t_id),enext));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = tb_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t_id),eprev));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = ta_id;

    n = SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t1_id),e1next));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = ta_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t1_id),e1prev));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = tb_id;

    /* Delete two parent triangles */
    if(remove_from_list(t_id,add_ptr))
       smDelete_tri(sm,t_id); 
    else
      *delptr = qtaddelem(*delptr,t_id);

    if(remove_from_list(t1_id,add_ptr))
       smDelete_tri(sm,t1_id); 
    else
      *delptr = qtaddelem(*delptr,t1_id);

    *tn_id = ta_id;
    *tn1_id = tb_id;
}

smUpdate_locator(sm,add_list,del_set)
SM *sm;
LIST *add_list;
OBJECT *del_set;
{
  int t_id,i;
  TRI *t;
  OBJECT *optr;
  
  while(add_list)
  {
    t_id = pop_list(&add_list);
    t = SM_NTH_TRI(sm,t_id);
    smLocator_add_tri(sm,t_id,T_NTH_V(t,0),T_NTH_V(t,1),T_NTH_V(t,2),del_set);
  }

  optr = QT_SET_PTR(del_set);
  for(i = QT_SET_CNT(del_set); i > 0; i--)
  {
      t_id = QT_SET_NEXT_ELEM(optr);
#if 0
      t = SM_NTH_TRI(sm,t_id);
      smLocator_remove_tri(sm,t_id,T_NTH_V(t,0),T_NTH_V(t,1),T_NTH_V(t,2));
#endif
      smDelete_tri(sm,t_id); 
  }
}
/* MUST add check for constrained edges */
int
smFix_tris(sm,id,tlist,add_list,delptr)
SM *sm;
int id;
LIST *tlist,*add_list;
QUADTREE *delptr;
{
    TRI *t,*t_opp;
    FVECT p,p1,p2,p3;
    int e,e1,swapped = 0;
    int t_id,t_opp_id;

    VSUB(p,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
    while(tlist)
    {
	t_id = pop_list(&tlist);
        t = SM_NTH_TRI(sm,t_id);
        e = (T_WHICH_V(t,id)+1)%3;
        t_opp_id = T_NTH_NBR(t,e);
        t_opp = SM_NTH_TRI(sm,t_opp_id);
	
	smDir_in_cone(sm,p1,T_NTH_V(t_opp,0));
	smDir_in_cone(sm,p2,T_NTH_V(t_opp,1));
	smDir_in_cone(sm,p3,T_NTH_V(t_opp,2));
	if(point_in_cone(p,p1,p2,p3))
	{
	    swapped = 1;
	    e1 = T_NTH_NBR_PTR(t_id,t_opp);
	    /* check list for t_opp and Remove if there */
	    remove_from_list(t_opp_id,&tlist);
	    smTris_swap_edge(sm,t_id,t_opp_id,e,e1,&t_id,&t_opp_id,
			     &add_list,delptr);
	    tlist = push_data(tlist,t_id);
	    tlist = push_data(tlist,t_opp_id);
	}
    }
    smUpdate_locator(sm,add_list,qtqueryset(*delptr));
    return(swapped);
}

/* Give the vertex "id" and a triangle "t" that it belongs to- return the
   next nbr in a counter clockwise order about vertex "id"
*/
int
smTri_next_ccw_nbr(sm,t,id)
SM *sm;
TRI *t;
int id;
{
  int t_id;
  int nbr_id;

  /* Want the edge for which "id" is the destination */
  t_id = (T_WHICH_V(t,id)+ 2)% 3;
  nbr_id = T_NTH_NBR(t,t_id);
  return(nbr_id);
}

smClear_tri_flags(sm,id)
SM *sm;
int id;
{
  int i;

  for(i=0; i < T_FLAGS; i++)
    SM_CLR_NTH_T_FLAG(sm,id,i);

}

/* Locate the point-id in the point location structure: */
int
smInsert_samp(sm,s_id,tri_id)
   SM *sm;
   int s_id,tri_id;
{
    int v0_id,v1_id,v2_id;
    int t0_id,t1_id,t2_id,replaced;
    LIST *tlist,*add_list;
    OBJECT del_set[2];
    QUADTREE delnode;
    FVECT npt;
    TRI *tri,*nbr;

    add_list = NULL;

    v0_id = T_NTH_V(SM_NTH_TRI(sm,tri_id),0);
    v1_id = T_NTH_V(SM_NTH_TRI(sm,tri_id),1);
    v2_id = T_NTH_V(SM_NTH_TRI(sm,tri_id),2);

    t0_id = smAdd_tri(sm,s_id,v0_id,v1_id);
    /* Add triangle to the locator */
    
    add_list = push_data(add_list,t0_id);

    t1_id = smAdd_tri(sm,s_id,v1_id,v2_id);
    add_list = push_data(add_list,t1_id);

    t2_id = smAdd_tri(sm,s_id,v2_id,v0_id);
    add_list = push_data(add_list,t2_id);

    /* CAUTION: tri must be assigned after Add_tri: pointers may change*/
    tri = SM_NTH_TRI(sm,tri_id);

    /* Set the neighbor pointers for the new tris */
    T_NTH_NBR(SM_NTH_TRI(sm,t0_id),0) = t2_id;
    T_NTH_NBR(SM_NTH_TRI(sm,t0_id),1) = T_NTH_NBR(tri,0);
    T_NTH_NBR(SM_NTH_TRI(sm,t0_id),2) = t1_id;
    T_NTH_NBR(SM_NTH_TRI(sm,t1_id),0) = t0_id;
    T_NTH_NBR(SM_NTH_TRI(sm,t1_id),1) = T_NTH_NBR(tri,1);
    T_NTH_NBR(SM_NTH_TRI(sm,t1_id),2) = t2_id;
    T_NTH_NBR(SM_NTH_TRI(sm,t2_id),0) = t1_id;
    T_NTH_NBR(SM_NTH_TRI(sm,t2_id),1) = T_NTH_NBR(tri,2);
    T_NTH_NBR(SM_NTH_TRI(sm,t2_id),2) = t0_id;

    /* Reset the neigbor pointers for the neighbors of the original */
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,0));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t0_id;
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,1));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t1_id;
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,2));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t2_id;
	
    del_set[0] = 1; del_set[1] = tri_id;
    delnode = qtnewleaf(del_set);

    /* Fix up the new triangles*/
    tlist = push_data(NULL,t0_id);
    tlist = push_data(tlist,t1_id);
    tlist = push_data(tlist,t2_id);

    smFix_tris(sm,s_id,tlist,add_list,&delnode);

    qtfreeleaf(delnode);

    return(TRUE);
}
    

int
smTri_in_set(sm,p,optr)
SM *sm;
FVECT p;
OBJECT *optr;
{
  int i,t_id;
  FVECT n,v0,v1,v2;
  TRI *t;
  
  for (i = QT_SET_CNT(optr),optr = QT_SET_PTR(optr);i > 0; i--)
  {
    /* Find the first triangle that pt falls */
    t_id = QT_SET_NEXT_ELEM(optr);
    t = SM_NTH_TRI(sm,t_id);
    if(!T_IS_VALID(t))
      continue;
    VSUB(v0,SM_T_NTH_WV(sm,t,0),SM_VIEW_CENTER(sm));
    VSUB(v1,SM_T_NTH_WV(sm,t,1),SM_VIEW_CENTER(sm));
    VSUB(v2,SM_T_NTH_WV(sm,t,2),SM_VIEW_CENTER(sm));

    if(EQUAL_VEC3(v0,p) || EQUAL_VEC3(v1,p) || EQUAL_VEC3(v2,p))
       return(t_id);
    
    VCROSS(n,v1,v0);
    if(DOT(n,p) >0.0)
      continue;
    VCROSS(n,v2,v1);
    if(DOT(n,p) > 0.0)
      continue;

    VCROSS(n,v0,v2);
    if(DOT(n,p) > 0.0)
      continue;

    return(t_id);
  }
  return(INVALID);
}

int
smPointLocateTri(sm,id)
SM *sm;
int id;
{
  FVECT tpt;
  QUADTREE qt,*optr;
  
  VSUB(tpt,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
  qt = smPointLocateCell(sm,tpt);
  if(QT_IS_EMPTY(qt))
    return(INVALID);  
 
  optr = qtqueryset(qt);
  return(smTri_in_set(sm,tpt,optr));
}


/*
   Determine whether this sample should:
   a) be added to the mesh by subdividing the triangle
   b) ignored
   c) replace values of an existing mesh vertex

   In case a, the routine will return TRUE, for b,c will return FALSE
   In case a, also determine if this sample should cause the deletion of
   an existing vertex. If so *rptr will contain the id of the sample to delete
   after the new sample has been added.

   ASSUMPTION: this will not be called with a new sample that is
               a BASE point.

   The following tests are performed (in order) to determine the fate
   of the sample:

   1) If the world space point of the sample coincides with one of the
      triangle vertex samples: The values of the triangle sample are
      replaced with the new values and FALSE is returned
   2) If the new sample is close in ws, and close in the spherical projection
      to one of the triangle vertex samples:
         pick the point with dir closest to that of the canonical view
	 If it is the new point: mark existing point for deletion,and return
	 TRUE,else return FALSE
   3) If the spherical projection of new is < REPLACE_EPS from a base point:
      add new and mark the base for deletion: return TRUE
   4) If the addition of the new sample point would introduce a "puncture"
      or cause new triangles with large depth differences:return FALSE
     This attempts to throw out points that should be occluded   
*/
int
smTest_sample(sm,tri_id,s_id,dir,rptr)
   SM *sm;
   int tri_id,s_id;
   FVECT dir;
   int *rptr;
{
    TRI *tri;
    double d,d2,dnear,dspt,d01,d12,d20,s0,s1,s2,ds,dv;
    int vid[3],i,nearid,norm,dcnt,bcnt;
    FVECT diff[3],spt,npt;
    FVECT p;

    VCOPY(p,SM_NTH_WV(sm,s_id));
    *rptr = INVALID;
    bcnt = dcnt = 0;
    tri = SM_NTH_TRI(sm,tri_id);
    vid[0] = T_NTH_V(tri,0);
    vid[1] = T_NTH_V(tri,1);
    vid[2] = T_NTH_V(tri,2);

    /* TEST 1: Test if the new point has the same world space point as
       one of the existing triangle vertices
     */
    for(i=0; i<3; i++)
    {
	if(SM_BASE_ID(sm,vid[i]))
	{
	  bcnt++;
	   continue;
	}
	if(SM_DIR_ID(sm,vid[i]))
	   dcnt++;
	VSUB(diff[i],SM_NTH_WV(sm,vid[i]),p);
	/* If same world point: replace */
	if(ZERO_VEC3(diff[i]))
	{
	    sReset_samp(SM_SAMP(sm),vid[i],s_id);
	    SM_TONE_MAP(sm) = 0;
	    return(FALSE);
	}
    }
    if(SM_DIR_ID(sm,s_id))
       return(TRUE);
    /* TEST 2: If the new sample is close in ws, and close in the spherical
       projection to one of the triangle vertex samples
    */
    norm = FALSE;
    if(bcnt + dcnt != 3)
    {
      VSUB(spt,p,SM_VIEW_CENTER(sm));
      ds = DOT(spt,spt);
      dnear = FHUGE;
      for(i=0; i<3; i++)
	{
	  if(SM_BASE_ID(sm,vid[i]) || SM_DIR_ID(sm,vid[i]))
	    continue;
	  d = DOT(diff[i],diff[i])/ds;
	  if(d < dnear)
	    {
	      dnear = d;
	      nearid=vid[i];
	    }
	}

      if(dnear <=  smMinSampDiff*smMinSampDiff)
	{
	  /* Pick the point with dir closest to that of the canonical view
	     if it is the new sample: mark existing point for deletion
	     */
	  normalize(spt);
	  norm = TRUE;
	  VSUB(npt,SM_NTH_WV(sm,nearid),SM_VIEW_CENTER(sm));
	  normalize(npt);
	  d = fdir2diff(SM_NTH_W_DIR(sm,nearid), npt);
	  if(dir)
	    d2 = 2. - 2.*DOT(dir,spt);
	  else
	    d2 = fdir2diff(SM_NTH_W_DIR(sm,s_id), spt);
	  /* The existing sample is a better sample:punt */
	  if(d2 > d)
	    return(FALSE);
	  else
	    {
		/* The new sample is better: mark the existing one
		   for deletion after the new one is added*/
	      *rptr = nearid;
	      return(TRUE);
	    }
	}
    }
  /* TEST 3: If the spherical projection of new is < S_REPLACE_EPS
     from a base point
  */
    if(bcnt)
    {   
	dnear = FHUGE;
	if(bcnt + dcnt ==3)
	  VSUB(spt,p,SM_VIEW_CENTER(sm));
	if(!norm)
	  normalize(spt);

	for(i=0; i<3; i++)
	{
	    if(!SM_BASE_ID(sm,vid[i]))
	       continue;
	    VSUB(npt,SM_NTH_WV(sm,vid[i]),SM_VIEW_CENTER(sm));
	    d = DIST_SQ(npt,spt);
	    if(d < S_REPLACE_EPS && d < dnear)
	       {
		   dnear = d;
		   nearid = vid[i];
	       }
	}
	if(dnear != FHUGE)
	{
	    /* add new and mark the base for deletion */
	    *rptr = nearid;
	    return(TRUE);
	}
    }
	
  /* TEST 4:
     If the addition of the new sample point would introduce a "puncture"
     or cause new triangles with large depth differences:dont add:    
     */
    if(bcnt || dcnt)
       return(TRUE);
    /* If the new point is in front of the existing points- add */
    dv = DIST_SQ(SM_NTH_WV(sm,vid[0]),SM_VIEW_CENTER(sm));
    if(ds < dv)
      return(TRUE);

    d01 = DIST_SQ(SM_NTH_WV(sm,vid[1]),SM_NTH_WV(sm,vid[0]));
    s0 = DOT(diff[0],diff[0]);
    if(s0 < S_REPLACE_SCALE*d01)
       return(TRUE);
    d12 = DIST_SQ(SM_NTH_WV(sm,vid[2]),SM_NTH_WV(sm,vid[1]));
    if(s0 < S_REPLACE_SCALE*d12)
       return(TRUE);    
    d20 = DIST_SQ(SM_NTH_WV(sm,vid[0]),SM_NTH_WV(sm,vid[2]));
    if(s0 < S_REPLACE_SCALE*d20)
       return(TRUE);    
    d = MIN3(d01,d12,d20);
    s1 = DOT(diff[1],diff[1]);
    if(s1 < S_REPLACE_SCALE*d)
       return(TRUE);
    s2 = DOT(diff[2],diff[2]);
    if(s2 < S_REPLACE_SCALE*d)
       return(TRUE);    


    return(FALSE);
}


int
smAlloc_samp(sm,c,dir,pt)
SM *sm;
COLR c;
FVECT dir,pt;
{
  int s_id,replaced,cnt;
  SAMP *s;
  FVECT p;

  s = SM_SAMP(sm);
  s_id = sAlloc_samp(s,&replaced);

  cnt=0;
  while(replaced)
  {
    if(smMesh_remove_vertex(sm,s_id))
      break;
    s_id = sAlloc_samp(s,&replaced);
    cnt++;
    if(cnt > S_MAX_SAMP(s))
      error(CONSISTENCY,"smAlloc_samp():unable to find free samp\n");
  }
  
  if(pt)
    sInit_samp(s,s_id,c,dir,pt);	  
  else
    {
      VADD(p,dir,SM_VIEW_CENTER(sm));
      sInit_samp(s,s_id,c,NULL,p);	  
    }
  return(s_id);
}

int
smAdd_samp(sm,s_id,p,dir)
   SM *sm;
   int s_id;
   FVECT p,dir;
{
    int t_id,r_id,test;
    double d;

    r_id = INVALID;
    t_id = smPointLocateTri(sm,s_id);
    if(t_id == INVALID)
    {
#ifdef DEBUG
      eputs("smAdd_samp(): tri not found \n");
#endif
      return(FALSE);
    }
    if(!SM_BASE_ID(sm,s_id))
    {
      if(!smTest_sample(sm,t_id,s_id,dir,&r_id))
	return(FALSE);
      /* If not a sky point, add distance from the viewcenter to average*/
      if( !SM_DIR_ID(sm,s_id))
      {
	  d = DIST(p,SM_VIEW_CENTER(smMesh));
	  smDist_sum += 1.0/d;
      }
    }
    test = smInsert_samp(smMesh,s_id,t_id);

    if(test && r_id != INVALID)
      smMesh_remove_vertex(sm,r_id);

    return(test);
}

/*
 * int
 * smNewSamp(c, dir, p)	: register new sample point and return index
 * COLR	c;		: pixel color (RGBE)
 * FVECT	dir;	: ray direction vector
 * FVECT	p;	: world intersection point
 *
 * Add new sample point to data structures, removing old values as necessary.
 * New sample representation will be output in next call to smUpdate().
 * If the point is a sky point: then v=NULL
 */
int
smNewSamp(c,dir,p)
COLR c;
FVECT dir;
FVECT p;

{
    int s_id;
   int debug=0;
    static FILE *fp;
    static int cnt=0,n=3010;
    /* First check if this the first sample: if so initialize mesh */
    if(SM_NUM_SAMP(smMesh) == 0)
     {
	 smInit_sm(smMesh,odev.v.vp);
#if 0
      fp = fopen("Debug_data.view","w");
      fprintf(fp,"%d %d %f %f %f ",1280,1024,odev.v.vp[0],odev.v.vp[1],
              odev.v.vp[2]);
      fprintf(fp,"%f %f %f ",odev.v.vdir[0],odev.v.vdir[1],
              odev.v.vdir[2]);
      fprintf(fp,"%f %f %f ",odev.v.vup[0],odev.v.vup[1],odev.v.vup[2]);
      fprintf(fp,"%f %f ",odev.v.horiz,odev.v.vert);
      fclose(fp);
      fp = fopen("Debug_data","w");
#endif
     }
#if 0
    fprintf(fp,"%f %f %f %f %f %f ",p[0],p[1],p[2],(float)c[0]/255.0,(float)c[1]/255.0,
           (float)c[2]/255.0);
#endif

    /* Allocate space for a sample */
    s_id = smAlloc_samp(smMesh,c,dir,p);
#if 0
    if(cnt==n)
       return(-1);
    cnt++;
#endif
    /* Add the sample to the mesh */
    if(!smAdd_samp(smMesh,s_id,p,dir))
    {  
      /* If the sample space was not used: return */
      smUnalloc_samp(smMesh,s_id);
      s_id = INVALID;
    }
    return(s_id);
    
 }    
int
smAdd_base_vertex(sm,v)
   SM *sm;
   FVECT v;
{
  int id;

  /* First add coordinate to the sample array */
  id = sAdd_base_point(SM_SAMP(sm),v);
  if(id == INVALID)
    return(INVALID);
  /* Initialize triangle pointer to -1 */
  smClear_vert(sm,id);
  return(id);
}



/* Initialize a the point location DAG based on a 6 triangle tesselation
   of the unit sphere centered on the view center. The DAG structure
   contains 6 roots: one for each initial base triangle
*/

int
smCreate_base_mesh(sm,type)
SM *sm;
int type;
{
  int i,s_id,tri_id,nbr_id;
  int p[4],ids[4];
  int v0_id,v1_id,v2_id;
  FVECT d,pt,cntr,v0,v1,v2;
  
  /* First insert the base vertices into the sample point array */

  for(i=0; i < 4; i++)
  {
    VCOPY(cntr,smDefault_base[i]);
    cntr[0] += .01;
    cntr[1] += .02;
    cntr[2] += .03;
    VADD(cntr,cntr,SM_VIEW_CENTER(sm));
    p[i]  = smAdd_base_vertex(sm,cntr);
  }
  /* Create the base triangles */
  for(i=0;i < 4; i++)
  {
    v0_id = p[smTri_verts[i][0]];
    v1_id = p[smTri_verts[i][1]];
    v2_id = p[smTri_verts[i][2]];
    ids[i] = smAdd_tri(sm, v0_id,v1_id,v2_id);
    smLocator_add_tri(sm,ids[i],v0_id,v1_id,v2_id,NULL);
  }
  
  /* Set neighbors */

  for(tri_id=0;tri_id < 4; tri_id++)
   for(nbr_id=0; nbr_id < 3; nbr_id++)
    T_NTH_NBR(SM_NTH_TRI(sm,ids[tri_id]),nbr_id) = smBase_nbrs[tri_id][nbr_id];

  
  /* Now add the centroids of the faces */
  for(tri_id=0;tri_id < 4; tri_id++)
  {
      VCOPY(v0,SM_T_NTH_WV(sm,SM_NTH_TRI(sm,ids[tri_id]),0));
      VCOPY(v1,SM_T_NTH_WV(sm,SM_NTH_TRI(sm,ids[tri_id]),1));
      VCOPY(v2,SM_T_NTH_WV(sm,SM_NTH_TRI(sm,ids[tri_id]),2));
      tri_centroid(v0,v1,v2,cntr);
      VSUB(cntr,cntr,SM_VIEW_CENTER(sm));
      normalize(cntr);
      VADD(cntr,cntr,SM_VIEW_CENTER(sm));
      s_id = smAdd_base_vertex(sm,cntr);
      smAdd_samp(sm,s_id,NULL,NULL);
  }
 return(1);

}


int
smNext_tri_flag_set(sm,i,which,b)
     SM *sm;
     int i,which;
     int b;
{

  for(; i < SM_NUM_TRI(sm);i++)
  {

    if(!T_IS_VALID(SM_NTH_TRI(sm,i)))
	 continue;
    if(!SM_IS_NTH_T_FLAG(sm,i,which))
	continue;
    if(!b)
      break;
    if((b==1) && !SM_BG_TRI(sm,i))
      break;
    if((b==2) && SM_BG_TRI(sm,i))
      break;
  }
     
  return(i);
}


int
smNext_valid_tri(sm,i)
     SM *sm;
     int i;
{

  while( i < SM_NUM_TRI(sm) && !T_IS_VALID(SM_NTH_TRI(sm,i)))
     i++;
     
  return(i);
}



qtTri_from_id(t_id,v0,v1,v2)
int t_id;
FVECT v0,v1,v2;
{
  TRI *t;
  
  t = SM_NTH_TRI(smMesh,t_id);
  if(!T_IS_VALID(t))
    return(0);
  VSUB(v0,SM_T_NTH_WV(smMesh,t,0),SM_VIEW_CENTER(smMesh));
  VSUB(v1,SM_T_NTH_WV(smMesh,t,1),SM_VIEW_CENTER(smMesh));
  VSUB(v2,SM_T_NTH_WV(smMesh,t,2),SM_VIEW_CENTER(smMesh));
  return(1);
}


smRebuild_mesh(sm,v)
   SM *sm;
   VIEW *v;
{
    int i,j,cnt;
    FVECT p,ov,dir;
    double d;

#ifdef DEBUG
    eputs("smRebuild_mesh(): rebuilding....");
#endif
    /* Clear the mesh- and rebuild using the current sample array */
    /* Remember the current number of samples */
    cnt = SM_NUM_SAMP(sm);
    /* Calculate the difference between the current and new viewpoint*/
    /* Will need to subtract this off of sky points */
    VSUB(ov,v->vp,SM_VIEW_CENTER(sm));
    /* Initialize the mesh to 0 samples and the base triangles */

    /* Go through all samples and add in if in the new view frustum, and
       the dir is <= 30 degrees from new view
     */
    j=0;
    for(i=0; i < cnt; i++)
    {
      /* First check if sample visible(conservative approx: if one of tris 
	 attached to sample is in frustum)	 */
      if(!S_IS_FLAG(i))
	continue;

      /* Next test if direction > 30 from current direction */
	if(SM_NTH_W_DIR(sm,i)!=-1)
	{
	    VSUB(p,SM_NTH_WV(sm,i),v->vp);
	    normalize(p);
	    d = fdir2diff(SM_NTH_W_DIR(sm,i), p);
	    if (d > MAXDIFF2)
	      continue;
	}
	sReset_samp(SM_SAMP(sm),j,i);
	j++;
    }
    smInit_sm(sm,v->vp);
    for(i=0; i< j; i++)
    {
	S_SET_FLAG(i);
	VCOPY(p,SM_NTH_WV(sm,i));
	smAdd_samp(sm,i,p,NULL);	
    }
    SM_NUM_SAMP(sm) = j;
    smNew_tri_cnt = SM_SAMPLE_TRIS(sm);
#ifdef DEBUG
    eputs("smRebuild_mesh():done\n");
#endif
}

int
intersect_tri_set(t_set,orig,dir,pt)
   OBJECT *t_set;
   FVECT orig,dir,pt;
{
    OBJECT *optr;
    int i,t_id,id;
    int pid0,pid1,pid2;
    FVECT p0,p1,p2,p;
    TRI *t;
    
    optr = QT_SET_PTR(t_set);
    for(i = QT_SET_CNT(t_set); i > 0; i--)
    {
	t_id = QT_SET_NEXT_ELEM(optr);

	t = SM_NTH_TRI(smMesh,t_id);
	if(!T_IS_VALID(t))
	  continue;

	pid0 = T_NTH_V(t,0);
	pid1 = T_NTH_V(t,1);
	pid2 = T_NTH_V(t,2);
	VCOPY(p0,SM_NTH_WV(smMesh,pid0));
	VCOPY(p1,SM_NTH_WV(smMesh,pid1));
	VCOPY(p2,SM_NTH_WV(smMesh,pid2));
	if(ray_intersect_tri(orig,dir,p0,p1,p2,p))
        {
	  id = closest_point_in_tri(p0,p1,p2,p,pid0,pid1,pid2);

	  if(pt)
	     VCOPY(pt,p);
#ifdef DEBUG_TEST_DRIVER
	  Pick_tri = t_id;
	  Pick_samp = id;
	  VCOPY(Pick_point[0],p);
#endif
	  return(id);
	}
    }
    return(-1);
}

/* OS is constrained to be <= QT_MAXCSET : if the set exceeds this, the
 results of check_set are conservative
*/

ray_trace_check_set(qtptr,fptr,argptr)
   QUADTREE *qtptr;
   int *fptr;
   RT_ARGS *argptr;
{
    OBJECT tset[QT_MAXSET+1],*tptr;	
    double dt,t;
    int found;
    
    if(QT_LEAF_IS_FLAG(*qtptr))
      {
	QT_FLAG_SET_DONE(*fptr);
#if DEBUG
	eputs("ray_trace_check_set():Already visited this node:aborting\n");
#endif
	return;
      }
    if(!QT_IS_EMPTY(*qtptr))
      {
	tptr = qtqueryset(*qtptr);
	if(QT_SET_CNT(tptr) > QT_MAXSET)
	  tptr = (OBJECT *)malloc((QT_SET_CNT(tptr)+1)*sizeof(OBJECT));
	else
	  tptr = tset;
	if(!tptr)
	  goto memerr;
    
	qtgetset(tptr,*qtptr);
	 /* Check triangles in set against those seen so far(os):only
	    check new triangles for intersection (t_set') 
	    */
       check_set_large(tptr,argptr->os);
       if((found = intersect_tri_set(tptr,argptr->orig,argptr->dir,NULL))!= -1)
       {
	     argptr->t_id = found;
	     if(tptr != tset)
	       free(tptr);
	     QT_FLAG_SET_DONE(*fptr);
	     return;
       }
       if(tptr != tset)
	 free(tptr);
     }
    return;
memerr:
    error(SYSTEM,"ray_trace_check_set():Unable to allocate memory");
}


/*
 * int
 * smFindSamp(FVECT orig, FVECT dir) 
 *
 * Find the closest sample to the given ray.  Returns sample id, -1 on failure.
 * "dir" is assumed to be normalized
 */

int
smFindSamp(orig,dir)
FVECT orig,dir;
{
  FVECT b,p,o;
  OBJECT *ts;
  QUADTREE qt;
  int s_id;
  double d;

 /*  r is the normalized vector from the view center to the current
  *  ray point ( starting with "orig"). Find the cell that r falls in,
  *  and test the ray against all triangles stored in the cell. If
  *  the test fails, trace the projection of the ray across to the
  *  next cell it intersects: iterate until either an intersection
  *  is found, or the projection ray is // to the direction. The sample
  *  corresponding to the triangle vertex closest to the intersection
  *  point is returned.
  */
  
  /* First test if "orig" coincides with the View_center or if "dir" is
     parallel to r formed by projecting "orig" on the sphere. In
     either case, do a single test against the cell containing the
     intersection of "dir" and the sphere
   */
  /* orig will be updated-so preserve original value */
  if(!smMesh)
     return;
  point_on_sphere(b,orig,SM_VIEW_CENTER(smMesh));
  d = -DOT(b,dir);
  if(EQUAL_VEC3(orig,SM_VIEW_CENTER(smMesh)) || EQUAL(fabs(d),1.0))
  {
      qt = smPointLocateCell(smMesh,dir);
      /* Test triangles in the set for intersection with Ray:returns
	 first found
      */
#ifdef DEBUG
      if(QT_IS_EMPTY(qt))
      {
	eputs("smFindSamp(): point not found");
	return;
      }
#endif
      ts = qtqueryset(qt);
      s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
      VCOPY(Pick_point[0],p);
#endif
  }
  else
  {
    OBJECT t_set[QT_MAXCSET + 1];
    RT_ARGS rt;

    /* Test each of the root triangles against point id */
    QT_CLEAR_SET(t_set);
    VSUB(o,orig,SM_VIEW_CENTER(smMesh));
    ST_CLEAR_FLAGS(SM_LOCATOR(smMesh));
    rt.t_id = -1;
    rt.os = t_set;
    VCOPY(rt.orig,orig);
    VCOPY(rt.dir,dir);
    stTrace_ray(SM_LOCATOR(smMesh),o,dir,ray_trace_check_set,&rt);
    s_id = rt.t_id;
 }    
  return(s_id);
}











