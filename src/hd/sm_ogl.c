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
#include "sm_geom.h"
#include "sm.h"

#ifdef TEST_DRIVER
#include "sm_draw.h"
/*static char smClean_notify = TRUE; 
MAKE STATIC LATER: ui.c using for now;
 */
char smClean_notify = TRUE; 
#else
static char smClean_notify = TRUE; 
#endif

int
mark_active_tris(qtptr,arg)
QUADTREE *qtptr;
char *arg;
{
  OBJECT os[MAXSET+1],*optr;
  int i,t_id;
  SM *sm;


  sm = (SM *)arg;

  /* For each triangle in the set, set the which flag*/
  if(QT_IS_EMPTY(*qtptr))
  {
      return(FALSE);
  }
  else
  {
    qtgetset(os,*qtptr);

    for (i = QT_SET_CNT(os),optr = QT_SET_PTR(os); i > 0; i--)
    {
      t_id = QT_SET_NEXT_ELEM(optr);
      /* Set the render flag */
      if(SM_IS_NTH_T_BASE(sm,t_id))
	continue;
      SM_SET_NTH_T_ACTIVE(sm,t_id);
      /* FOR NOW:Also set the LRU clock bit: MAY WANT TO CHANGE: */      
      SM_SET_NTH_T_LRU(sm,t_id);
    }
  }
  return(TRUE);
}


smMark_tris_in_frustum(sm,vp)
SM *sm;
VIEW *vp;
{
    FVECT nr[4],far[4];

    /* Mark triangles in approx. view frustum as being active:set
       LRU counter: for use in discarding samples when out
       of space
       Radiance often has no far clipping plane: but driver will set
       dev_zmin,dev_zmax to satisfy OGL
    */

    /* First clear all the triangle active flags */
    smClear_flags(sm,T_ACTIVE_FLAG);

    /* calculate the world space coordinates of the view frustum */
    calculate_view_frustum(vp->vp,vp->hvec,vp->vvec,vp->horiz,vp->vert,
			   dev_zmin,dev_zmax,nr,far);

    /* Project the view frustum onto the spherical quadtree */
    /* For every cell intersected by the projection of the faces
       of the frustum: mark all triangles in the cell as ACTIVE-
       Also set the triangles LRU clock counter
       */
    /* Need to do tonemap call */
    /* Near face triangles */
    smLocator_apply_func(sm,nr[0],nr[2],nr[3],mark_active_tris,(char *)(sm));
    smLocator_apply_func(sm,nr[2],nr[0],nr[1],mark_active_tris,(char *)(sm));
    /* Right face triangles */
    smLocator_apply_func(sm,nr[0],far[3],far[0],mark_active_tris,(char *)(sm));
    smLocator_apply_func(sm,far[3],nr[0],nr[3],mark_active_tris,(char *)(sm));
    /* Left face triangles */
    smLocator_apply_func(sm,nr[1],far[2],nr[2],mark_active_tris,(char *)(sm));
    smLocator_apply_func(sm,far[2],nr[1],far[1],mark_active_tris,(char *)(sm));
    /* Top face triangles */
    smLocator_apply_func(sm,nr[0],far[0],nr[1],mark_active_tris,(char *)(sm));
    smLocator_apply_func(sm,nr[1],far[0],far[1],mark_active_tris,(char *)(sm));
    /* Bottom face triangles */
    smLocator_apply_func(sm,nr[3],nr[2],far[3],mark_active_tris,(char *)(sm));
    smLocator_apply_func(sm,nr[2],far[2],far[3],mark_active_tris,(char *)(sm));
    /* Far face triangles */
   smLocator_apply_func(sm,far[0],far[2],far[1],mark_active_tris,(char *)(sm));
   smLocator_apply_func(sm,far[2],far[0],far[3],mark_active_tris,(char *)(sm));

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
  /* Mark all triangles in the frustum as active */
    if(!smMesh || SM_NUM_TRIS(smMesh)==0)
       return;
    
#ifdef TEST_DRIVER
    smMark_tris_in_frustum(smMesh,&Current_View);
#else
    smMark_tris_in_frustum(smMesh,&(odev.v));
#endif
    smClean_notify = TRUE;
}


smRender_tri(sm,i,vp)
SM *sm;
int i;
FVECT vp;
{
  TRI *tri;
  double ptr[3];
  int j;

  tri = SM_NTH_TRI(sm,i);
  SM_CLEAR_NTH_T_NEW(sm,i);
  if(smNew_tri_cnt)
     smNew_tri_cnt--;
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

/* NOTE SEEMS BAD TO PENALIZE POLYGONS INFRONT BY LETTING
ADJACENT TRIANGLES TO BG be BG
*/
smRender_bg_tri(sm,i,vp,d)
SM *sm;
int i;
FVECT vp;
double d;
{
  TRI *tri;
  FVECT p;
  int j,ids[3],cnt;
  int rgb[3];


  tri = SM_NTH_TRI(sm,i);
  SM_CLEAR_NTH_T_NEW(sm,i);
  if(smNew_tri_cnt)
     smNew_tri_cnt--;
  /* NOTE:Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  cnt = 0;
  rgb[0] = rgb[1] = rgb[2] = 0;
  for(j=0;j<3;j++)
  {
    ids[j] = T_NTH_V(tri,j);
    if(SM_BG_SAMPLE(sm,ids[j]))
    {
      rgb[0] += SM_NTH_RGB(sm,ids[j])[0];
      rgb[1] += SM_NTH_RGB(sm,ids[j])[1];
      rgb[2] += SM_NTH_RGB(sm,ids[j])[2];
      cnt++;
    }
  }
  if(cnt)
  {
    rgb[0]/=cnt; rgb[1]/=cnt; rgb[2]/=cnt;
  } 
  for(j=2; j>= 0; j--)
  {
    if(SM_BG_SAMPLE(sm,ids[j]))
      glColor3ub(SM_NTH_RGB(sm,ids[j])[0],SM_NTH_RGB(sm,ids[j])[1],
	         SM_NTH_RGB(sm,ids[j])[2]);
    else
      glColor3ub(rgb[0],rgb[1],rgb[2]);
    if(SM_BG_SAMPLE(sm,ids[j]))
      VSUB(p,SM_NTH_WV(sm,ids[j]),SM_VIEW_CENTER(sm));
    else
      smDir(sm,p,ids[j]);
    if(dev_zmin > 1.0)
    {
      p[0] *= d;
      p[1] *= d;
      p[2] *= d;
    }
    VADD(p,p,vp);
    glVertex3d(p[0],p[1],p[2]);
  }
}

smRender_mesh(sm,vp)
SM *sm;
FVECT vp;
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
    smRender_bg_tri(sm,i,vp,d);
  glEnd();
  
  glEnable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  SM_FOR_ALL_ACTIVE_FG_TRIS(sm,i)
    smRender_tri(sm,i,vp); 
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

smDepth_sort_tris(sm,vp,td)
SM *sm;
VIEW *vp;
T_DEPTH *td;
{
  int i,j,t_id;
  TRI *tri;
  double d[3];

  i = 0;
  SM_FOR_ALL_NEW_TRIS(sm,t_id)
  {
    if(i >= smNew_tri_cnt)
    {
#ifdef DEBUG
	eputs("smDepth_sort_tris(): more new tris then counted\n");
#endif
	break;
    }
    tri = SM_NTH_TRI(sm,t_id);
    td[i].tri = t_id;
    for(j=0;j < 3;j++)
      d[j] = DIST(vp->vp,SM_T_NTH_WV(sm,tri,j));
    td[i].depth = MIN_VEC3(d);
    i++;
  }
  qsort((void *)td,smNew_tri_cnt,sizeof(T_DEPTH),compare_tri_depths);
}


smUpdate_Rendered_mesh(sm,vp)
SM *sm;
VIEW *vp;
{
  static T_DEPTH *td= NULL;
  static int tsize = 0;
  int i;
  GLint depth_test;
  double d;

  /* For all of the NEW triangles (since last update): assume
     ACTIVE. Go through and sort on depth value (from vp). Turn
     Depth Buffer test off and render back-front
     */
  /* NOTE: could malloc each time or hard code */
  if(smNew_tri_cnt > tsize)
  {
    if(td)
      free(td);
    td = (T_DEPTH *)malloc(smNew_tri_cnt*sizeof(T_DEPTH));
    tsize = smNew_tri_cnt;
  }
  if(!td)
  {
    error(SYSTEM,"smUpdate_Rendered_mesh:Cannot allocate memory\n");
  }
  smDepth_sort_tris(sm,vp,td);

  /* Turn Depth Test off */
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  glDepthFunc(GL_ALWAYS);          /* Turn off Depth-painter's algorithm */

  d = (dev_zmin+dev_zmax)/2.0;
  /* Now render back-to front */
  /* First render bg triangles */
  glDisable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  for(i=0; i< smNew_tri_cnt; i++)
    if(SM_BG_TRI(sm,td[i].tri))
      smRender_bg_tri(sm,td[i].tri,vp,d);
  glEnd();

  glEnable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  for(i=0; i< smNew_tri_cnt; i++)
    if(!SM_BG_TRI(sm,td[i].tri))
      smRender_tri(sm,td[i].tri,vp);
  glEnd();

  /* Restore Depth Test */
  glPopAttrib();
}

/*
 * smUpdate(vp, qua)	: update OpenGL output geometry for view vp
 * VIEW	*vp;		: desired view
 * int	qual;           : quality level (percentage on linear time scale)
 *
 * Draw new geometric representation using OpenGL calls.  Assume that the
 * view has already been set up and the correct frame buffer has been
 * selected for drawing.  The quality level is on a linear scale, where 100%
 * is full (final) quality.  It is not necessary to redraw geometry that has
 * been output since the last call to smClean().
 */
smUpdate(vp,qual)
   VIEW *vp;
   int qual;
{
  double d;
  int t;
#ifdef TEST_DRIVER  
  Current_View = (*vp);
#endif
  /* If view has moved beyond epsilon from canonical: must rebuild -
     epsilon is calculated as running avg of distance of sample points
     from canonical view: m = 1/(SUM1/r): some fraction of this
   */
  d = DIST(vp->vp,SM_VIEW_CENTER(smMesh));
  if(qual >= 100 && d > SM_ALLOWED_VIEW_CHANGE(smMesh))
  {
      smNew_tri_cnt = 0;
      /* Re-build the mesh */
      smRebuild_mesh(smMesh,vp);
  }
  /* Check if should draw ALL triangles in current frustum */
  if(smClean_notify || (smNew_tri_cnt > SM_NUM_TRIS(smMesh)*SM_INC_PERCENT))
  {
#ifdef TEST_DRIVER
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
    tmClearHisto();
    tmAddHisto(SM_BRT(smMesh),SM_NUM_SAMP(smMesh),1);
    if(tmComputeMapping(0.,0.,0.) != TM_E_OK)
       return;
    if(tmMapPixels(SM_RGB(smMesh),SM_BRT(smMesh),SM_CHR(smMesh),
		   SM_NUM_SAMP(smMesh))!=TM_E_OK)
       return;
#endif
    smRender_mesh(smMesh,vp->vp);
    smClean_notify = FALSE;
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
    smUpdate_Rendered_mesh(smMesh,vp);
    
#ifdef TEST_DRIVER
    glDrawBuffer(GL_BACK);
#endif
  }
  SM_TONE_MAP(smMesh) = SM_NUM_SAMP(smMesh);
}

    /* LATER:If quality < 100 should draw approximation because it indicates
       the user is moving
       */


