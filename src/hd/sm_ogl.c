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
#include "sm_flag.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm_qtree.h"
#include "sm_stree.h"
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

static QT_LUENT	*qt_htbl = NULL;	/* quadtree cache */
static int	qt_hsiz = 0;		/* quadtree cache size */


typedef struct _T_DEPTH {
  int tri;
  double depth;
}T_DEPTH;

 /*
  * smClean(tmflag)	: display has been wiped clean
  *     int tmflag;
  * Called after display has been effectively cleared, meaning that all
  * geometry must be resent down the pipeline in the next call to smUpdate().
  * If tmflag is set, tone-mapping should be performed
  */
smClean(tmflag)
   int tmflag;
{
    smClean_notify = TRUE;
    if(tmflag)
       SM_TONE_MAP(smMesh) = 0;
}

int
qtCache_init(nel)		/* initialize for at least nel elements */
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
qtCache_find(qt)		/* find a quadtree table entry */
QUADTREE qt;
{
	int	i, n;
	register int	ndx;
	register QT_LUENT	*le;

	if (qt_hsiz == 0 && !qtCache_init(1))
		return(NULL);
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
	if (!qtCache_init(ndx+1)) {	/* no more memory! */
		qt_htbl = le;
		qt_hsiz = ndx;
		return(NULL);
	}
					/* copy old table to new and free */
	while (ndx--)
		if (!QT_IS_EMPTY(le[ndx].qt))
			copystruct(qtCache_find(le[ndx].qt), &le[ndx]);
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
    if(QT_LEAF_IS_FLAG(qt))
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
  TRI *tri;
  
  if (QT_IS_EMPTY(qt))				/* empty leaf node */
    return(NULL);
  if (QT_IS_TREE(qt) && !QT_IS_FLAG(qt))	/* not in our frustum */
    return(NULL);
  if(QT_IS_LEAF(qt)  && !QT_LEAF_IS_FLAG(qt))	/* not in our frustum */
    return(NULL);
					/* else look up node */
  if ((le = qtCache_find(qt)) == NULL)
    error(SYSTEM, "out of memory in qtRender_level");
  if (QT_IS_TREE(qt) && (QT_IS_EMPTY(le->qt) || lvl > 0))
  {					/* compute children */
    qtSubdivide_tri(v0,v1,v2,a,b,c);
    rc[0] = qtRender_level(QT_NTH_CHILD(qt,0),v0,c,b,sm,lvl-1);
    rc[1] = qtRender_level(QT_NTH_CHILD(qt,1),c,v1,a,sm,lvl-1);
    rc[2] = qtRender_level(QT_NTH_CHILD(qt,2),b,a,v2,sm,lvl-1);
    rc[3] = qtRender_level(QT_NTH_CHILD(qt,3),a,b,c,sm,lvl-1);
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
      OBJECT *os;
      int s0, s1, s2;
      
      os = qtqueryset(qt);
      for (i = os[0]; i; i--)
      {
	if(SM_IS_NTH_T_BASE(sm,os[i]))
	   continue;
	tri = SM_NTH_TRI(sm,os[i]);
	if(!T_IS_VALID(tri))
	  continue;
	n++;
	s0 = T_NTH_V(tri,0);
	s1 = T_NTH_V(tri,1);
	s2 = T_NTH_V(tri,2);
	VCOPY(a,SM_NTH_WV(sm,s0));
	VCOPY(b,SM_NTH_WV(sm,s1));
	VCOPY(c,SM_NTH_WV(sm,s2));	      
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
      n *= 3;
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
    glVertex3d(a[0],a[1],a[2]);
    glVertex3d(b[0],b[1],b[2]);
    glVertex3d(c[0],c[1],c[2]);

  }
  return(&le->av);
}


smRender_stree_level(sm,lvl)
SM *sm;
int lvl;
{
  QUADTREE qt;
  int i;
  FVECT t0,t1,t2;
  STREE *st;

  
  if (lvl < 0)
    return;
  st = SM_LOCATOR(sm);
  glPushAttrib(GL_LIGHTING_BIT);
  glShadeModel(GL_FLAT);
  glBegin(GL_TRIANGLES);
  for(i=0; i < ST_NUM_ROOT_NODES; i++)
  {
    qt = ST_ROOT_QT(st,i);
    qtRender_level(qt,ST_NTH_V(st,i,0),ST_NTH_V(st,i,1),ST_NTH_V(st,i,2),
		   sm,lvl);
  }
  glEnd();
  glPopAttrib();
}


smRender_stree(sm, qual)	/* render some quadtree triangles */
SM *sm;
int qual;
{
  int i, n,ntarget;
  int lvlcnt[QT_MAX_LEVELS];
  STREE *st;
  int4 *active_flag;
  if (qual <= 0)
    return;
				/* compute rendering target */
  ntarget = 0;

  active_flag = SM_NTH_FLAGS(sm,T_ACTIVE_FLAG);
  for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
    if(active_flag[n])
      for(i=0; i < 32; i++)
	if(active_flag[n] & (1L << i))
	  ntarget++;

  ntarget = ntarget*qual/MAXQUALITY;
  if (!ntarget)
    return;
  for (i = QT_MAX_LEVELS; i--; )
    lvlcnt[i] = 0;
  
  st = SM_LOCATOR(sm);
  for(i=0; i < ST_NUM_ROOT_NODES;i++)
    stCount_level_leaves(lvlcnt, ST_ROOT_QT(st,i));
  
  for (ntarget -= lvlcnt[i=0]; i < QT_MAX_LEVELS-1; ntarget -= lvlcnt[++i])
    if (ntarget < lvlcnt[i+1])
      break;
				/* compute and render target level */
  smRender_stree_level(sm,i);
}



#define render_tri(v0,v1,v2,rgb0,rgb1,rgb2) \
  {glColor3ub(rgb0[0],rgb0[1],rgb0[2]);  glVertex3fv(v0); \
  glColor3ub(rgb1[0],rgb1[1],rgb1[2]);  glVertex3fv(v1); \
  glColor3ub(rgb2[0],rgb2[1],rgb2[2]);  glVertex3fv(v2);} \

render_mixed_tri(v0,v1,v2,rgb0,rgb1,rgb2,bg0,bg1,bg2,vp,vc)
float v0[3],v1[3],v2[3];
BYTE rgb0[3],rgb1[3],rgb2[3];
int bg0,bg1,bg2;
FVECT vp,vc;
{
  double p[3],d;
  int j,ids[3],cnt;
  int rgb[3];

  /* NOTE:Triangles are defined clockwise:historical relative to spherical
     tris: could change
     */
  if(bg0 && bg1 && bg2)
    return;

  cnt = 0;
  d = 0.0;
  rgb[0] = rgb[1] = rgb[2] = 0;

  if(!bg0)
  {
    rgb[0] += rgb0[0];
    rgb[1] += rgb0[1];
    rgb[2] += rgb0[2];
    cnt++;
    d += DIST(vp,v0);
  }
  if(!bg1)
  {
    rgb[0] += rgb1[0];
    rgb[1] += rgb1[1];
    rgb[2] += rgb1[2];
    cnt++;
    d += DIST(vp,v1);
  }
  if(!bg2)
  {
    rgb[0] += rgb2[0];
    rgb[1] += rgb2[1];
    rgb[2] += rgb2[2];
    cnt++;
    d += DIST(vp,v2);
  }
  if(cnt > 1)
  {
    rgb[0]/=cnt; rgb[1]/=cnt; rgb[2]/=cnt;
    d /= (double)cnt;
  } 
  if(bg0)
  {
    glColor3ub(rgb[0],rgb[1],rgb[2]);
    VSUB(p,v0,vc);
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
    VADD(p,p,vc);
    glVertex3dv(p);
  }
  else
  {
    glColor3ub(rgb0[0],rgb0[1],rgb0[2]);
    glVertex3fv(v0);
   }
  if(bg1)
  {
    glColor3ub(rgb[0],rgb[1],rgb[2]);
    VSUB(p,v1,vc);
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
    VADD(p,p,vc);
    glVertex3dv(p);
  }
  else
  {
    glColor3ub(rgb1[0],rgb1[1],rgb1[2]);
    glVertex3fv(v1);
   }
  if(bg2)
  {
    glColor3ub(rgb[0],rgb[1],rgb[2]);
    VSUB(p,v2,vc);
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
    VADD(p,p,vc);
    glVertex3dv(p);
  }
  else
  {
    glColor3ub(rgb2[0],rgb2[1],rgb2[2]);
    glVertex3fv(v2);
   }

}

render_bg_tri(v0,v1,v2,rgb0,rgb1,rgb2,vp,vc,d)
float v0[3],v1[3],v2[3];
BYTE rgb0[3],rgb1[3],rgb2[3];
FVECT vp,vc;
double d;
{
  double p[3];
  
  glColor3ub(rgb0[0],rgb0[1],rgb0[2]);
  VSUB(p,v0,vc);
  if(dev_zmin >= 0.99)
  {
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
  }
  VADD(p,p,vp);
  glVertex3dv(p);

  glColor3ub(rgb1[0],rgb1[1],rgb1[2]);
  VSUB(p,v1,vc);
  if(dev_zmin >= 0.99)
  {
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
  }
  VADD(p,p,vp);
  glVertex3dv(p);


  glColor3ub(rgb2[0],rgb2[1],rgb2[2]);
  VSUB(p,v2,vc);
  if(dev_zmin >= 0.99)
  {
    p[0] *= d;
    p[1] *= d;
    p[2] *= d;
  }
  VADD(p,p,vp);
  glVertex3dv(p);

}

smRender_mesh(sm,vp)
SM *sm;
FVECT vp;
{
  int i,n,bg0,bg1,bg2;
  double d;
  int v0_id,v1_id,v2_id;
  TRI *tri;
  float (*wp)[3];
  BYTE  (*rgb)[3];
  int4  *active_flag,*bg_flag;

  wp = SM_WP(sm);
  rgb =SM_RGB(sm);
  d = (dev_zmin+dev_zmax)/2.0;
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  
  /* First draw background polygons */
  glDisable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);

  active_flag = SM_NTH_FLAGS(sm,T_ACTIVE_FLAG);
  bg_flag = SM_NTH_FLAGS(sm,T_BG_FLAG);
  for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
    if(active_flag[n] & bg_flag[n])
      for(i=0; i < 32; i++)
	if(active_flag[n] & bg_flag[n] & (1L << i))
	 {
	   tri = SM_NTH_TRI(sm,(n<<5)+i);
	   v0_id = T_NTH_V(tri,0);
	   v1_id = T_NTH_V(tri,1);
	   v2_id = T_NTH_V(tri,2);
	   render_bg_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],rgb[v1_id],
			 rgb[v2_id],vp,SM_VIEW_CENTER(sm),d);
	 }
  glEnd();

  glEnable(GL_DEPTH_TEST);
  glBegin(GL_TRIANGLES);
  for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
    if(active_flag[n])
      for(i=0; i < 32; i++)
	if((active_flag[n] & (1L << i)) && !(bg_flag[n] & (1L << i)))
	 {
	   tri = SM_NTH_TRI(sm,(n<<5)+i);
	   v0_id = T_NTH_V(tri,0);
	   v1_id = T_NTH_V(tri,1);
	   v2_id = T_NTH_V(tri,2);
	   bg0 = SM_DIR_ID(sm,v0_id) || SM_BASE_ID(sm,v0_id);
	   bg1 = SM_DIR_ID(sm,v1_id) || SM_BASE_ID(sm,v1_id);
	   bg2 = SM_DIR_ID(sm,v2_id) || SM_BASE_ID(sm,v2_id);
	   if(!(bg0 || bg1 || bg2))
	     render_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],rgb[v1_id],
			rgb[v2_id])
	       else
		 render_mixed_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],
		  rgb[v1_id],rgb[v2_id],bg0,bg1,bg2,vp,SM_VIEW_CENTER(sm));
	 }
  glEnd();

  glPopAttrib();
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

#ifdef DEBUG
#define freebuf(b)  tempbuf(-1)
#endif


char *
tempbuf(len)			/* get a temporary buffer */
unsigned  len;
{
	extern char  *malloc(), *realloc();
	static char  *tempbuf = NULL;
	static unsigned  tempbuflen = 0;

#ifdef DEBUG
	static int in_use=FALSE;

	if(len == -1)
	  {
	    in_use = FALSE;
	    return(NULL);
	  }
	if(in_use)
        {
	    eputs("Buffer in use:cannot allocate:tempbuf()\n");
	    return(NULL);
	}
#endif
	if (len > tempbuflen) {
		if (tempbuflen > 0)
			tempbuf = realloc(tempbuf, len);
		else
			tempbuf = malloc(len);
		tempbuflen = tempbuf==NULL ? 0 : len;
	}
#ifdef DEBUG
	in_use = TRUE;
#endif
	return(tempbuf);
}

smOrder_new_tris(sm,vp,td)
SM *sm;
FVECT vp;
T_DEPTH *td;
{
  int n,i,j,tcnt,v;
  TRI *tri;
  double d,min_d;
  FVECT diff;
  int4 *new_flag,*bg_flag;

  tcnt=0;
  new_flag = SM_NTH_FLAGS(sm,T_NEW_FLAG);
  bg_flag = SM_NTH_FLAGS(sm,T_BG_FLAG);
  for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
    if(new_flag[n] & ~bg_flag[n])
      for(i=0; i < 32; i++)
	if(new_flag[n] & (1L << i) & ~bg_flag[n])
	 {
	   tri = SM_NTH_TRI(sm,(n<<5)+i);
	   td[tcnt].tri = (n << 5)+i;
	   min_d = -1;
	   for(j=0;j < 3;j++)
	     {
	       v = T_NTH_V(tri,j);
	       if(!SM_BG_SAMPLE(sm,v))
		 {
		   VSUB(diff,SM_NTH_WV(sm,v),vp);
		   d = DOT(diff,diff);
		   if(min_d == -1 || d < min_d)
		     min_d = d;
		 }
	     }
	   td[tcnt++].depth = min_d;
  }
  td[tcnt].tri = -1;
  if(tcnt)
      qsort((void *)td,tcnt,sizeof(T_DEPTH),compare_tri_depths);
}


smUpdate_Rendered_mesh(sm,vp,clr)
SM *sm;
FVECT vp;
int clr;
{
  int i,n,v0_id,v1_id,v2_id,bg0,bg1,bg2;
  GLint depth_test;
  double d;
  TRI *tri;
  float (*wp)[3];
  BYTE  (*rgb)[3];
  int4  *new_flag,*bg_flag;
  T_DEPTH *td = NULL;
  /* For all of the NEW triangles (since last update): assume
     ACTIVE. Go through and sort on depth value (from vp). Turn
     Depth Buffer test off and render back-front
     */
  if(!EQUAL_VEC3(SM_VIEW_CENTER(sm),vp))
  {
    /* Must depth sort if view points do not coincide */
    td = (T_DEPTH *)tempbuf(smNew_tri_cnt*sizeof(T_DEPTH));
#ifdef DEBUG
    if(!td)
	eputs("Cant create list:wont depth sort:smUpdate_rendered_mesh\n");
#endif
    smOrder_new_tris(sm,vp,td);
  }
  wp = SM_WP(sm);
  rgb =SM_RGB(sm);
  /* Turn Depth Test off -- using Painter's algorithm */
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  glDepthFunc(GL_ALWAYS);
  d = (dev_zmin+dev_zmax)/2.0;

  /* Now render back-to front */
  /* First render bg triangles */
  new_flag = SM_NTH_FLAGS(sm,T_NEW_FLAG);
  bg_flag = SM_NTH_FLAGS(sm,T_BG_FLAG);
  glBegin(GL_TRIANGLES);
  for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
    if(new_flag[n] & bg_flag[n])
      for(i=0; i < 32; i++)
	if(new_flag[n] & (1L << i) & bg_flag[n] )
	 {
	   tri = SM_NTH_TRI(sm,(n<<5)+i);
	   v0_id = T_NTH_V(tri,0);
	   v1_id = T_NTH_V(tri,1);
	   v2_id = T_NTH_V(tri,2);
	   render_bg_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],rgb[v1_id],
			 rgb[v2_id],vp,SM_VIEW_CENTER(sm),d);
	 }
  glEnd();


  glBegin(GL_TRIANGLES);
  if(!td)
  {
    for(n=((SM_NUM_TRI(sm)+31)>>5) +1; --n;)
     if(new_flag[n] & ~bg_flag[n])
      for(i=0; i < 32; i++)
	if(new_flag[n] & (1L << i) & ~bg_flag[n])
	 {
	   tri = SM_NTH_TRI(sm,(n<<5)+i);
	   /* Dont need to check for valid tri because flags are
	      cleared on delete
	    */
	   v0_id = T_NTH_V(tri,0);
	   v1_id = T_NTH_V(tri,1);
	   v2_id = T_NTH_V(tri,2);
	   bg0 = SM_DIR_ID(sm,v0_id) || SM_BASE_ID(sm,v0_id);
	   bg1 = SM_DIR_ID(sm,v1_id) || SM_BASE_ID(sm,v1_id);
	   bg2 = SM_DIR_ID(sm,v2_id) || SM_BASE_ID(sm,v2_id);
	   if(!(bg0 || bg1 || bg2))
	     render_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],rgb[v1_id],
			rgb[v2_id])
	   else
	     render_mixed_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],
			      rgb[v1_id],rgb[v2_id],bg0,bg1,bg2,vp,
			      SM_VIEW_CENTER(sm));
	 }
  }
  else
  {
    for(i=0; td[i].tri != -1;i++)
    {
      tri = SM_NTH_TRI(sm,td[i].tri);
      /* Dont need to check for valid tri because flags are
	 cleared on delete
	 */
      v0_id = T_NTH_V(tri,0);
      v1_id = T_NTH_V(tri,1);
      v2_id = T_NTH_V(tri,2);
      bg0 = SM_DIR_ID(sm,v0_id) || SM_BASE_ID(sm,v0_id);
      bg1 = SM_DIR_ID(sm,v1_id) || SM_BASE_ID(sm,v1_id);
      bg2 = SM_DIR_ID(sm,v2_id) || SM_BASE_ID(sm,v2_id);
      if(!(bg0 || bg1 || bg2))
	render_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],rgb[v1_id],
		   rgb[v2_id])
	  else
	    render_mixed_tri(wp[v0_id],wp[v1_id],wp[v2_id],rgb[v0_id],
			     rgb[v1_id],rgb[v2_id],bg0,bg1,bg2,vp,
			     SM_VIEW_CENTER(sm));
    }
#ifdef DEBUG
    freebuf(td);
#endif
  }
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
  int last_update,t;

  if(!smMesh)
    return;

  /* If view has moved beyond epsilon from canonical: must rebuild -
     epsilon is calculated as running avg of distance of sample points
     from canonical view: m = 1/(AVG(1/r)): some fraction of this
   */
  d = DIST(view->vp,SM_VIEW_CENTER(smMesh));
  if(qual >= MAXQUALITY  && d > SM_ALLOWED_VIEW_CHANGE(smMesh))
  {
      /* Re-build the mesh */
#ifdef TEST_DRIVER
    odev.v = *view;
#endif  
      mark_tris_in_frustum(view);
      smRebuild_mesh(smMesh,view);
      smClean_notify = TRUE;
  }
  /* This is our final update iff qual==MAXQUALITY and view==&odev.v */
  last_update = qual>=MAXQUALITY && view==&(odev.v);
  /* Check if we should draw ALL triangles in current frustum */
  if(smClean_notify || smNew_tri_cnt>SM_SAMPLE_TRIS(smMesh)*SM_INC_PERCENT)
  {
#ifdef TEST_DRIVER
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
    if (SM_TONE_MAP(smMesh) < SM_NUM_SAMP(smMesh))
    {
       tmClearHisto();
       tmAddHisto(SM_BRT(smMesh),SM_NUM_SAMP(smMesh),1);
       if(tmComputeMapping(0.,0.,0.) != TM_E_OK)
	 return;
       tmMapPixels(SM_RGB(smMesh),SM_BRT(smMesh),SM_CHR(smMesh),
		   SM_NUM_SAMP(smMesh));
    }
#endif
    mark_tris_in_frustum(view);
    if (qual <= (MAXQUALITY*3/4))
	smRender_stree(smMesh,qual);
    else
	smRender_mesh(smMesh,view->vp);
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
      tmMapPixels(SM_NTH_RGB(smMesh,t),&SM_NTH_BRT(smMesh,t),
		   SM_NTH_CHR(smMesh,t), SM_NUM_SAMP(smMesh)-t);
	
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
    smClear_flags(smMesh,T_NEW_FLAG);
    qtCache_init(0);
  }

}








