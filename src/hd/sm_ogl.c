/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * sm_ogl.c
*/
#include "standard.h"

#include <GL/gl.h>

#ifdef TEST_DRIVER
#include <gl/device.h>
#include <GL/glu.h>
#include <glut.h>
#endif
#include "object.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm.h"

#ifdef TEST_DRIVER
#include "sm_draw.h"
/*static char smClean_notify = TRUE; 
MAKE STATIC LATER: ui.c using for now;
 */
char smClean_notify = TRUE; 
#else
static int smClean_notify = TRUE; 
#endif

typedef struct {
	float	dist;		/* average distance */
	BYTE	rgb[3];		/* average color */
} QTRAVG;		/* average quadtree value */

typedef struct {
	QUADTREE	qt;	/* quadtree node (key & hash value) */
	QTRAVG		av;	/* node average */
} QT_LUENT;		/* lookup table entry */

static QT_LUENT	*qt_htbl = NULL;	/* quadtree hash table */
static int	qt_hsiz = 0;		/* quadtree hash table size */


int
mark_active_tris(qtptr,arg)
QUADTREE *qtptr;
char *arg;
{
  QUADTREE qt = *qtptr;
  OBJECT os[QT_MAX_SET+1],*optr;
  register int i,t_id;

  if (!QT_IS_LEAF(qt))
    return(TRUE);
  /* For each triangle in the set, set the which flag*/
  qtgetset(os,qt);

  for (i = QT_SET_CNT(os), optr = QT_SET_PTR(os); i > 0; i--)
  {
    t_id = QT_SET_NEXT_ELEM(optr);
    /* Set the render flag */
    if(SM_IS_NTH_T_BASE(smMesh,t_id))
	continue;
    SM_SET_NTH_T_ACTIVE(smMesh,t_id);
    /* FOR NOW:Also set the LRU clock bit: MAY WANT TO CHANGE: */      
    SM_SET_NTH_T_LRU(smMesh,t_id);
  }
  return(TRUE);
}

mark_tris_in_frustum(view)
VIEW *view;
{
    FVECT nr[4],far[4];

    /* Mark triangles in approx. view frustum as being active:set
       LRU counter: for use in discarding samples when out
       of space
       Radiance often has no far clipping plane: but driver will set
       dev_zmin,dev_zmax to satisfy OGL
    */

    /* First clear all the quadtree node and triangle active flags */
    qtClearAllFlags();
    smClear_flags(smMesh,T_ACTIVE_FLAG);

    /* calculate the world space coordinates of the view frustum */
    calculate_view_frustum(view->vp,view->hvec,view->vvec,view->horiz,
			   view->vert, dev_zmin,dev_zmax,nr,far);

    /* Project the view frustum onto the spherical quadtree */
    /* For every cell intersected by the projection of the faces
       of the frustum: mark all triangles in the cell as ACTIVE-
       Also set the triangles LRU clock counter
       */
    /* Near face triangles */
    smLocator_apply_func(smMesh,nr[0],nr[2],nr[3],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,nr[2],nr[0],nr[1],mark_active_tris,NULL);
    /* Right face triangles */
    smLocator_apply_func(smMesh,nr[0],far[3],far[0],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,far[3],nr[0],nr[3],mark_active_tris,NULL);
    /* Left face triangles */
    smLocator_apply_func(smMesh,nr[1],far[2],nr[2],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,far[2],nr[1],far[1],mark_active_tris,NULL);
    /* Top face triangles */
    smLocator_apply_func(smMesh,nr[0],far[0],nr[1],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,nr[1],far[0],far[1],mark_active_tris,NULL);
    /* Bottom face triangles */
    smLocator_apply_func(smMesh,nr[3],nr[2],far[3],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,nr[2],far[2],far[3],mark_active_tris,NULL);
    /* Far face triangles */
    smLocator_apply_func(smMesh,far[0],far[2],far[1],mark_active_tris,NULL);
    smLocator_apply_func(smMesh,far[2],far[0],far[3],mark_active_tris,NULL);

#ifdef TEST_DRIVER
    VCOPY(FrustumFar[0],far[0]);
    VCOPY(FrustumFar[1],far[1]);
    VCOPY(FrustumFar[2],far[2]);
    VCOPY(FrustumFar[3],far[3]);
    VCOPY(FrustumNear[0],nr[0]);
    VCOPY(FrustumNear[1],nr[1]);
    VCOPY(FrustumNear[2],nr[2]);
    VCOPY(FrustumNear[3],nr[3]);
#endif
}

/*
 * smClean()		: display has been wiped clean
 *
 * Called after display has been effectively cleared, meaning that all
 * geometry must be resent down the pipeline in the next call to smUpdate().
 */
smClean()
{
    smClean_notify = TRUE;
}

int
qtHash_init(nel)		/* initialize for at least nel elements */
int	nel;
{
	static int  hsiztab[] = {
		8191, 16381, 32749, 65521, 131071, 262139, 524287, 1048573, 0
	};
	register int  i;

	if (nel <= 0) {			/* call to free table */
		if (qt_hsiz) {
			free((char *)qt_htbl);
			qt_htbl = NULL;
			qt_hsiz = 0;
		}
		return(0);
	}
	nel += nel>>1;			/* 66% occupancy */
	for (i = 0; hsiztab[i]; i++)
		if (hsiztab[i] > nel)
			break;
	if (!(qt_hsiz = hsiztab[i]))
		qt_hsiz = nel*2 + 1;		/* not always prime */
	qt_htbl = (QT_LUENT *)calloc(qt_hsiz, sizeof(QT_LUENT));
	if (qt_htbl == NULL)
		qt_hsiz = 0;
	for (i = qt_hsiz; i--; )
		qt_htbl[i].qt = EMPTY;
	return(qt_hsiz);
}

QT_LUENT *
qtHash_find(qt)			/* find a quadtree table entry */
QUADTREE qt;
{
	int	i, n;
	register int	ndx;
	register QT_LUENT	*le;

	if (qt_hsiz == 0)
		qtHash_init(1);
tryagain:				/* hash table lookup */
	ndx = (unsigned long)qt % qt_hsiz;
	for (i = 0, n = 1; i < qt_hsiz; i++, n += 2) {
		le = &qt_htbl[ndx];
		if (QT_IS_EMPTY(le->qt) || le->qt == qt)
			return(le);
		if ((ndx += n) >= qt_hsiz)	/* this happens rarely */
			ndx = ndx % qt_hsiz;
	}
					/* table is full, reallocate */
	le = qt_htbl;
	ndx = qt_hsiz;
	if (!qtHash_init(ndx+1)) {	/* no more memory! */
		qt_htbl = le;
		qt_hsiz = ndx;
		return(NULL);
	}
	if (!ndx)
		goto tryagain;
					/* copy old table to new */
	while (ndx--)
		if (!QT_IS_EMPTY(le[ndx].qt))
			copystruct(qtHash_find(le[ndx].qt), &le[ndx]);
	free((char *)le);
	goto tryagain;			/* should happen only once! */
}

stCount_level_leaves(lcnt, qt)	/* count quadtree leaf nodes at each level */
int lcnt[];
register QUADTREE qt;
{
  if (QT_IS_EMPTY(qt))
    return;
  if (QT_IS_TREE(qt)) {
    if (!QT_IS_FLAG(qt))	/* not in our frustum */
      return;
    stCount_level_leaves(lcnt+1, QT_NTH_CHILD(qt,0));
    stCount_level_leaves(lcnt+1, QT_NTH_CHILD(qt,1));
    stCount_level_leaves(lcnt+1, QT_NTH_CHILD(qt,2));
    stCount_level_leaves(lcnt+1, QT_NTH_CHILD(qt,3));
  }
  else
    lcnt[0]++;
}


QTRAVG *
qtRender_level(qt,v0,v1,v2,sm,lvl)
QUADTREE qt;
FVECT v0,v1,v2;
SM *sm;
int lvl;
{
  FVECT a,b,c;
  register QT_LUENT *le;
  QTRAVG *rc[4];
  
  if (QT_IS_EMPTY(qt))				/* empty leaf node */
    return(NULL);
  if (QT_IS_TREE(qt) && !QT_IS_FLAG(qt))	/* not in our frustum */
    return(NULL);
					/* else look up node */
  if ((le = qtHash_find(qt)) == NULL)
    error(SYSTEM, "out of memory in qtRender_level");
  if (QT_IS_TREE(qt) && (QT_IS_EMPTY(le->qt) || lvl > 0))
  {					/* compute children */
    qtSubdivide_tri(v0,v1,v2,a,b,c);
    rc[0] = qtRender_level(QT_NTH_CHILD(qt,0),v0,a,c,sm,lvl-1);
    rc[1] = qtRender_level(QT_NTH_CHILD(qt,1),a,v1,b,sm,lvl-1);
    rc[2] = qtRender_level(QT_NTH_CHILD(qt,2),c,b,v2,sm,lvl-1);
    rc[3] = qtRender_level(QT_NTH_CHILD(qt,3),b,c,a,sm,lvl-1);
  }
  if (QT_IS_EMPTY(le->qt))
  {					/* let's make some data! */
    int rgbs[3];
    double distsum;
    register int i, n;
					/* average our triangle vertices */
    rgbs[0] = rgbs[1] = rgbs[2] = 0;
    distsum = 0.; n = 0;
    if(QT_IS_TREE(qt))
    {					/* from subtree */
      for (i = 4; i--; )
	if (rc[i] != NULL)
	{
	  rgbs[0] += rc[i]->rgb[0]; rgbs[1] += rc[i]->rgb[1];
	  rgbs[2] += rc[i]->rgb[2]; distsum += rc[i]->dist; n++;
	}
    }
    else
    {					/* from triangle set */
      OBJECT os[QT_MAX_SET+1];
      int s0, s1, s2;

      qtgetset(os,qt);
      for (n = os[0]; n; n--)
      {
	qtTri_from_id(os[n],a,b,c,NULL,NULL,NULL,&s0,&s1,&s2);
	distsum += SM_BG_SAMPLE(sm,s0) ? dev_zmax
				: sqrt(dist2(a,SM_VIEW_CENTER(sm)));
	distsum += SM_BG_SAMPLE(sm,s1) ? dev_zmax
				: sqrt(dist2(b,SM_VIEW_CENTER(sm)));
	distsum += SM_BG_SAMPLE(sm,s2) ? dev_zmax
				: sqrt(dist2(c,SM_VIEW_CENTER(sm)));
	rgbs[0] += SM_NTH_RGB(sm,s0)[0] + SM_NTH_RGB(sm,s1)[0]
		  + SM_NTH_RGB(sm,s2)[0];
	rgbs[1] += SM_NTH_RGB(sm,s0)[1] + SM_NTH_RGB(sm,s1)[1]
		  + SM_NTH_RGB(sm,s2)[1];
	rgbs[2] += SM_NTH_RGB(sm,s0)[2] + SM_NTH_RGB(sm,s1)[2]
		  + SM_NTH_RGB(sm,s2)[2];
      }
      n = 3*os[0];
    }
    if (!n)
      return(NULL);
    le->qt = qt;
    le->av.rgb[0] = rgbs[0]/n; le->av.rgb[1] = rgbs[1]/n;
    le->av.rgb[2] = rgbs[2]/n; le->av.dist = distsum/(double)n;
  }
  if (lvl == 0 || (lvl > 0 && QT_IS_LEAF(qt)))
  {				/* render this node */
					/* compute pseudo vertices */
    VCOPY(a,v0); VCOPY(b,v1); VCOPY(c,v2);
    normalize(a); normalize(b); normalize(c);
    VSUM(a,SM_VIEW_CENTER(sm),a,le->av.dist);
    VSUM(b,SM_VIEW_CENTER(sm),b,le->av.dist);
    VSUM(c,SM_VIEW_CENTER(sm),c,le->av.dist);
					/* draw triangle */
    glColor3ub(le->av.rgb[0],le->av.rgb[1],le->av.rgb[2]);
    /* NOTE: Triangle vertex order may change */
    glVertex3d(c[0],c[1],c[2]);
    glVertex3d(b[0],b[1],b[2]);
    glVertex3d(a[0],a[1],a[2]);
  }
  return(&le->av);
}


smRender_stree_level(sm,lvl)
SM *sm;
int lvl;
{
  QUADTREE root;
  int i;
  FVECT t0,t1,t2;

  if (lvl < 1)
    return;
  glPushAttrib(GL_LIGHTING_BIT);
  glShadeModel(GL_FLAT);
  glBegin(GL_TRIANGLES);
  for(i=0; i < 4; i++)
  {
    root = ST_NTH_ROOT(SM_LOCATOR(sm),i);
    stNth_base_verts(SM_LOCATOR(sm),i,t0,t1,t2); 
    qtRender_level(root,t0,t1,t2,sm,lvl-1);
  }
  glEnd();
  glPopAttrib();
}


smRender_stree(sm, qual)	/* render some quadtree triangles */
SM *sm;
int qual;
{
  int i, ntarget;
  int lvlcnt[QT_MAX_LEVELS];

  if (qual <= 0)
    return;
				/* compute rendering target */
  ntarget = 0;
  SM_FOR_ALL_ACTIVE_TRIS(sm,i)
    ntarget++;
  ntarget = ntarget*qual/100;
  if (!ntarget)
    return;
  for (i = QT_MAX_LEVELS; i--; )
    lvlcnt[i] = 0;
  stCount_level_leaves(lvlcnt, SM_LOCATOR(sm)->root);
  for (ntarget -= lvlcnt[i=0]; i < QT_MAX_LEVELS-1; ntarget -= lvlcnt[++i])
    if (ntarget < lvlcnt[i+1])
      break;
				/* compute and render target level */
  smRender_stree_level(sm,i);
}


smRender_tri(sm,i,vp,clr)
SM *sm;
int i;
FVECT vp;
int clr;
{
  TRI *tri;
  double ptr[3];
  int j;

  tri = SM_NTH_TRI(sm,i);
  if (clr) SM_CLEAR_NTH_T_NEW(sm,i);

  /* NOTE:Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  for(j=2; j>= 0; j--)
  {
#ifdef DEBUG
    if(SM_BG_SAMPLE(sm,T_NTH_V(tri,j)))
      eputs("SmRenderTri(): shouldnt have bg samples\n");
#endif
    glColor3ub(SM_NTH_RGB(sm,T_NTH_V(tri,j))[0],
	       SM_NTH_RGB(sm,T_NTH_V(tri,j))[1],
	       SM_NTH_RGB(sm,T_NTH_V(tri,j))[2]);
    VCOPY(ptr,SM_T_NTH_WV(sm,tri,j));
    glVertex3d(ptr[0],ptr[1],ptr[2]);
  }
}

smRender_mixed_tri(sm,i,vp,clr)
SM *sm;
int i;
FVECT vp;
int clr;
{
  TRI *tri;
  double p[3],d;
  int j,ids[3],cnt;
  int rgb[3];

  tri = SM_NTH_TRI(sm,i);
  if (clr) SM_CLEAR_NTH_T_NEW(sm,i);

  /* NOTE:Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  cnt = 0;
  d = 0.0;
  rgb[0] = rgb[1] = rgb[2] = 0;
  for(j=0;j < 3;j++)
  {
      ids[j] = T_NTH_V(tri,j);
      if(!SM_BG_SAMPLE(sm,ids[j]))
      {
	  rgb[0] += SM_NTH_RGB(sm,ids[j])[0];
	  rgb[1] += SM_NTH_RGB(sm,ids[j])[1];
	  rgb[2] += SM_NTH_RGB(sm,ids[j])[2];
	  cnt++;
	  d += DIST(vp,SM_NTH_WV(sm,ids[j]));
      }
  }
  if(cnt > 1)
  {
    rgb[0]/=cnt; rgb[1]/=cnt; rgb[2]/=cnt;
    d /= (double)cnt;
  } 
  for(j=2; j>= 0; j--)
  {
    if(SM_BG_SAMPLE(sm,ids[j]))
    {
	glColor3ub(rgb[0],rgb[1],rgb[2]);
	VSUB(p,SM_NTH_WV(sm,ids[j]),SM_VIEW_CENTER(sm));
	p[0] *= d;
	p[1] *= d;
	p[2] *= d;
	VADD(p,p,SM_VIEW_CENTER(sm));
    }
    else
    {
	glColor3ub(SM_NTH_RGB(sm,ids[j])[0],SM_NTH_RGB(sm,ids[j])[1],
		   SM_NTH_RGB(sm,ids[j])[2]);
	VCOPY(p,SM_NTH_WV(sm,ids[j]));
    }
    glVertex3d(p[0],p[1],p[2]);
   }
}

smRender_bg_tri(sm,i,vp,d,clr)
SM *sm;
int i;
FVECT vp;
double d;
int clr;
{
  double p[3];
  int j,id;
  TRI *tri;
  
  tri = SM_NTH_TRI(sm,i);
  if (clr) SM_CLEAR_NTH_T_NEW(sm,i);

  /* NOTE:Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  for(j=2; j>= 0; j--)
  {
      id = T_NTH_V(tri,j);
      glColor3ub(SM_NTH_RGB(sm,id)[0],SM_NTH_RGB(sm,id)[1],
	         SM_NTH_RGB(sm,id)[2]);
      VSUB(p,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
      if(dev_zmin >= 0.99)
       {
	   p[0] *= d;
	   p[1] *= d;
	   p[2] *= d;
       }
      VADD(p,p,vp);
      glVertex3d(p[0],p[1],p[2]);
  }
}

smRender_mesh(sm,vp,clr)
SM *sm;
FVECT vp;
int clr;
{
  int i;
  TRI *tri;
  double ptr[3],d;
  int j;

  d = (dev_zmin+dev_zmax)/2.0;
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  
  /* First draw background polygons */

  glDisable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  SM_FOR_ALL_ACTIVE_BG_TRIS(sm,i)
     smRender_bg_tri(sm,i,vp,d,clr);
  glEnd();
  
  glEnable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  SM_FOR_ALL_ACTIVE_FG_TRIS(sm,i)
  {
      if(!SM_MIXED_TRI(sm,i))
	smRender_tri(sm,i,vp,clr);
     else
	smRender_mixed_tri(sm,i,vp,clr);
  }
  glEnd();

  glPopAttrib();
}

smRender_tri_edges(sm,i)
SM *sm;
int i;
{
  TRI *tri;
  int j;
  double ptr[3];


  tri = SM_NTH_TRI(sm,i);

  /* Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  for(j=2; j >=0; j--)
  {
    VCOPY(ptr,SM_NTH_WV(sm,T_NTH_V(tri,j)));
    glVertex3d(ptr[0],ptr[1],ptr[2]);
    VCOPY(ptr,SM_NTH_WV(sm,T_NTH_V(tri,(j+1)%3)));
    glVertex3d(ptr[0],ptr[1],ptr[2]);
  }
}

int
compare_tri_depths(T_DEPTH *td1,T_DEPTH *td2)
{
  double d;

  d = td2->depth-td1->depth;
  
  if(d > 0.0)
    return(1);
  if(d < 0.0)
    return(-1);
  
  return(0);
}

LIST
*smDepth_sort_tris(sm,vp,td)
SM *sm;
FVECT vp;
T_DEPTH *td;
{
  int i,j,t_id,v;
  TRI *tri;
  double d,min_d;
  LIST *tlist=NULL;

  i = 0;
  SM_FOR_ALL_NEW_TRIS(sm,t_id)
  {
    if(SM_BG_TRI(sm,t_id))
    {
	tlist = push_data(tlist,t_id);
	continue;
    }
    tri = SM_NTH_TRI(sm,t_id);
    td[i].tri = t_id;
    min_d = -1;
    for(j=0;j < 3;j++)
    {
	v = T_NTH_V(tri,j);
	if(!SM_BG_SAMPLE(sm,v))
	{
	    d = DIST(vp,SM_NTH_WV(sm,v));
	    if(min_d == -1 || d < min_d)
	       min_d = d;
	}
    }
    td[i].depth = min_d;
    i++;
  }
  td[i].tri = -1;
  if(i)
     qsort((void *)td,i,sizeof(T_DEPTH),compare_tri_depths);
  return(tlist);
}


smUpdate_Rendered_mesh(sm,vp,clr)
SM *sm;
FVECT vp;
int clr;
{
  static T_DEPTH *td= NULL;
  static int tsize = 0;
  int i;
  GLint depth_test;
  double d;
  LIST *bglist;
  /* For all of the NEW triangles (since last update): assume
     ACTIVE. Go through and sort on depth value (from vp). Turn
     Depth Buffer test off and render back-front
     */
  /* NOTE: could malloc each time or hard code */
  if(smNew_tri_cnt > tsize)
  {
    if(td)
      free((char *)td);
    td = (T_DEPTH *)malloc(smNew_tri_cnt*sizeof(T_DEPTH));
    tsize = smNew_tri_cnt;
  }
  if(!td)
  {
    error(SYSTEM,"smUpdate_Rendered_mesh:Cannot allocate memory\n");
  }
  bglist = smDepth_sort_tris(sm,vp,td);

  /* Turn Depth Test off -- using Painter's algorithm */
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  d = (dev_zmin+dev_zmax)/2.0;
  /* Now render back-to front */
  /* First render bg triangles */
  glBegin(GL_TRIANGLES);
  while(bglist)
     smRender_bg_tri(sm,pop_list(&bglist),vp,d,clr);
  glEnd();

  glEnable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  i=0;
  while(td[i].tri != -1)
     if(!SM_MIXED_TRI(sm,td[i].tri))
	smRender_tri(sm,td[i++].tri,vp,clr);
     else
	smRender_mixed_tri(sm,td[i++].tri,vp,clr);
  glEnd();

  /* Restore Depth Test */
  glPopAttrib();
}

/*
 * smUpdate(view, qua)	: update OpenGL output geometry for view vp
 * VIEW	*view;		: desired view
 * int	qual;           : quality level (percentage on linear time scale)
 *
 * Draw new geometric representation using OpenGL calls.  Assume that the
 * view has already been set up and the correct frame buffer has been
 * selected for drawing.  The quality level is on a linear scale, where 100%
 * is full (final) quality.  It is not necessary to redraw geometry that has
 * been output since the last call to smClean().  (The last view drawn will
 * be view==&odev.v each time.)
 */
smUpdate(view,qual)
   VIEW *view;
   int qual;
{
  double d;
  int last_update;
  int t;

  /* If view has moved beyond epsilon from canonical: must rebuild -
     epsilon is calculated as running avg of distance of sample points
     from canonical view: m = 1/(AVG(1/r)): some fraction of this
   */
  d = DIST(view->vp,SM_VIEW_CENTER(smMesh));
  if(qual >= 100 && d > SM_ALLOWED_VIEW_CHANGE(smMesh))
  {
      /* Re-build the mesh */
#ifdef TEST_DRIVER
    odev.v = *view;
#endif    
      smRebuild_mesh(smMesh,view->vp);
  }
  /* This is our final update iff qual==100 and view==&odev.v */
  last_update = qual>=100 && view==&(odev.v);
  /* Check if we should draw ALL triangles in current frustum */
  if(smClean_notify || smNew_tri_cnt > SM_NUM_TRIS(smMesh)*SM_INC_PERCENT)
  {
#ifdef TEST_DRIVER
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
    if (SM_TONE_MAP(smMesh) < SM_NUM_SAMP(smMesh))
    {
       tmClearHisto();
       tmAddHisto(SM_BRT(smMesh),SM_NUM_SAMP(smMesh),1);
       if(tmComputeMapping(0.,0.,0.) != TM_E_OK ||
		   tmMapPixels(SM_RGB(smMesh),SM_BRT(smMesh),SM_CHR(smMesh),
				SM_NUM_SAMP(smMesh)) != TM_E_OK)
	    return;
    }
#endif
    mark_tris_in_frustum(view);
    if (qual <= 75)
	smRender_stree(smMesh,qual);
    else
	smRender_mesh(smMesh,view->vp,last_update);
#ifdef TEST_DRIVER
    glFlush();
    glutSwapBuffers();
#endif
  }
  /* Do an incremental update instead */
  else
  {  
    if(!smNew_tri_cnt)
      return;
#ifdef TEST_DRIVER
    glDrawBuffer(GL_FRONT);
#else
    t = SM_TONE_MAP(smMesh);
    if(t == 0)
    {
	tmClearHisto();
	tmAddHisto(SM_BRT(smMesh),SM_NUM_SAMP(smMesh),1);
	if(tmComputeMapping(0.,0.,0.) != TM_E_OK)
	   return;
    }
    if(tmMapPixels(SM_NTH_RGB(smMesh,t),&SM_NTH_BRT(smMesh,t),
		   SM_NTH_CHR(smMesh,t), SM_NUM_SAMP(smMesh)-t) != TM_E_OK)
	  return;
#endif    
    smUpdate_Rendered_mesh(smMesh,view->vp,last_update);
    
#ifdef TEST_DRIVER
    glDrawBuffer(GL_BACK);
#endif
  }
  SM_TONE_MAP(smMesh) = SM_NUM_SAMP(smMesh);
  if (last_update)
  {
    smClean_notify = FALSE;
    smNew_tri_cnt = 0;
    qtHash_init(0);
  }

}
