/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm_geom.c
 */

#include "standard.h"
#include "sm_geom.h"

tri_centroid(v0,v1,v2,c)
FVECT v0,v1,v2,c;
{
/* Average three triangle vertices to give centroid: return in c */
  c[0] = (v0[0] + v1[0] + v2[0])/3.0;
  c[1] = (v0[1] + v1[1] + v2[1])/3.0;
  c[2] = (v0[2] + v1[2] + v2[2])/3.0;
}


int
vec3_equal(v1,v2)
FVECT v1,v2;
{
   return(EQUAL(v1[0],v2[0]) && EQUAL(v1[1],v2[1])&& EQUAL(v1[2],v2[2]));
}
#if 0
extern FVECT Norm[500];
extern int Ncnt;
#endif

int
convex_angle(v0,v1,v2)
FVECT v0,v1,v2;
{
    double dp,dp1;
    int test,test1;
    FVECT nv0,nv1,nv2;
    FVECT cp01,cp12,cp;

    /* test sign of (v0Xv1)X(v1Xv2). v1 */
    /* un-Simplified: */ 
    VCOPY(nv0,v0);
    normalize(nv0);
    VCOPY(nv1,v1);
    normalize(nv1);
    VCOPY(nv2,v2);
    normalize(nv2);

    VCROSS(cp01,nv0,nv1);
    VCROSS(cp12,nv1,nv2);
    VCROSS(cp,cp01,cp12);
    normalize(cp);
    dp1 = DOT(cp,nv1);
    if(dp1 <= 1e-8 || dp1 >= (1-1e-8)) /* Test if on other side,or colinear*/
      test1 = FALSE;
    else
      test1 = TRUE;

    dp =   nv0[2]*nv1[0]*nv2[1] -  nv0[2]*nv1[1]*nv2[0] - nv0[0]*nv1[2]*nv2[1] 
         + nv0[0]*nv1[1]*nv2[2] +  nv0[1]*nv1[2]*nv2[0] - nv0[1]*nv1[0]*nv2[2];
    
    if(dp <= 1e-8 || dp1 >= (1-1e-8))
      test = FALSE;
    else
      test = TRUE;

    if(test != test1)
      fprintf(stderr,"test %f simplified  %f\n",dp1,dp);
    return(test1);
}

/* calculates the normal of a face contour using Newell's formula. e

               a =  SUMi (yi - yi+1)(zi + zi+1);
	       b =  SUMi (zi - zi+1)(xi + xi+1)
	       c =  SUMi (xi - xi+1)(yi + yi+1)
*/
double
tri_normal(v0,v1,v2,n,norm)
FVECT v0,v1,v2,n;
int norm;
{
  double mag;

  n[0] = (v0[2] + v1[2]) * (v0[1] - v1[1]) + 
         (v1[2] + v2[2]) * (v1[1] - v2[1])  +
         (v2[2] + v0[2]) * (v2[1] - v0[1]);
  
  n[1] = (v0[2] - v1[2]) * (v0[0] + v1[0]) +
	   (v1[2] - v2[2]) * (v1[0] + v2[0]) +
	    (v2[2] - v0[2]) * (v2[0] + v0[0]);
  
  n[2] = (v0[1] + v1[1]) * (v0[0] - v1[0]) +
         (v1[1] + v2[1]) * (v1[0] - v2[0]) +
         (v2[1] + v0[1]) * (v2[0] - v0[0]);

  if(!norm)
     return(0);
  
  mag = normalize(n);

  return(mag);
}


tri_plane_equation(v0,v1,v2,peqptr,norm)
   FVECT v0,v1,v2; 
   FPEQ *peqptr;
   int norm;
{
    tri_normal(v0,v1,v2,FP_N(*peqptr),norm);

    FP_D(*peqptr) = -(DOT(FP_N(*peqptr),v0));
}


/* returns TRUE if ray from origin in direction v intersects plane defined
  * by normal plane_n, and plane_d. If plane is not parallel- returns
  * intersection point if r != NULL. If tptr!= NULL returns value of
  * t, if parallel, returns t=FHUGE
  */
int
intersect_vector_plane(v,peq,tptr,r)
   FVECT v;
   FPEQ peq;
   double *tptr;
   FVECT r;
{
  double t,d;
  int hit;
    /*
      Plane is Ax + By + Cz +D = 0:
      plane[0] = A,plane[1] = B,plane[2] = C,plane[3] = D
    */

    /* line is  l = p1 + (p2-p1)t, p1=origin */

    /* Solve for t: */
  d = -(DOT(FP_N(peq),v));
  if(ZERO(d))
  {
      t = FHUGE;
      hit = 0;
  }
  else
  {
      t =  FP_D(peq)/d;
      if(t < 0 )
	 hit = 0;
      else
	 hit = 1;
      if(r)
	 {
	     r[0] = v[0]*t;
	     r[1] = v[1]*t;
	     r[2] = v[2]*t;
	 }
  }
    if(tptr)
       *tptr = t;
  return(hit);
}

int
intersect_ray_plane(orig,dir,peq,pd,r)
   FVECT orig,dir;
   FPEQ peq;
   double *pd;
   FVECT r;
{
  double t,d;
  int hit;
    /*
      Plane is Ax + By + Cz +D = 0:
      plane[0] = A,plane[1] = B,plane[2] = C,plane[3] = D
    */
     /*  A(orig[0] + dxt) + B(orig[1] + dyt) + C(orig[2] + dzt) + pd = 0
         t = -(DOT(plane_n,orig)+ plane_d)/(DOT(plane_n,d))
       line is  l = p1 + (p2-p1)t 
     */
    /* Solve for t: */
  d = DOT(FP_N(peq),dir);
  if(ZERO(d))
     return(0);
  t =  -(DOT(FP_N(peq),orig) + FP_D(peq))/d; 

  if(t < 0)
       hit = 0;
    else
       hit = 1;

  if(r)
     VSUM(r,orig,dir,t);

    if(pd)
       *pd = t;
  return(hit);
}


int
intersect_ray_oplane(orig,dir,n,pd,r)
   FVECT orig,dir;
   FVECT n;
   double *pd;
   FVECT r;
{
  double t,d;
  int hit;
    /*
      Plane is Ax + By + Cz +D = 0:
      plane[0] = A,plane[1] = B,plane[2] = C,plane[3] = D
    */
     /*  A(orig[0] + dxt) + B(orig[1] + dyt) + C(orig[2] + dzt) + pd = 0
         t = -(DOT(plane_n,orig)+ plane_d)/(DOT(plane_n,d))
       line is  l = p1 + (p2-p1)t 
     */
    /* Solve for t: */
    d= DOT(n,dir);
    if(ZERO(d))
       return(0);
    t =  -(DOT(n,orig))/d;
    if(t < 0)
       hit = 0;
    else
       hit = 1;

  if(r)
     VSUM(r,orig,dir,t);

    if(pd)
       *pd = t;
  return(hit);
}

/* Assumption: know crosses plane:dont need to check for 'on' case */
intersect_edge_coord_plane(v0,v1,w,r)
FVECT v0,v1;
int w;
FVECT r;
{
  FVECT dv;
  int wnext;
  double t;

  VSUB(dv,v1,v0);
  t = -v0[w]/dv[w];
  r[w] = 0.0;
  wnext = (w+1)%3;
  r[wnext] = v0[wnext] + dv[wnext]*t;
  wnext = (w+2)%3;
  r[wnext] = v0[wnext] + dv[wnext]*t;
}

int
intersect_edge_plane(e0,e1,peq,pd,r)
   FVECT e0,e1;
   FPEQ peq;
   double *pd;
   FVECT r;
{
  double t;
  int hit;
  FVECT d;
  /*
      Plane is Ax + By + Cz +D = 0:
      plane[0] = A,plane[1] = B,plane[2] = C,plane[3] = D
    */
     /*  A(orig[0] + dxt) + B(orig[1] + dyt) + C(orig[2] + dzt) + pd = 0
         t = -(DOT(plane_n,orig)+ plane_d)/(DOT(plane_n,d))
       line is  l = p1 + (p2-p1)t 
     */
    /* Solve for t: */
  VSUB(d,e1,e0);
  t =  -(DOT(FP_N(peq),e0) + FP_D(peq))/(DOT(FP_N(peq),d));
    if(t < 0)
       hit = 0;
    else
       hit = 1;

  VSUM(r,e0,d,t);

    if(pd)
       *pd = t;
  return(hit);
}

int
point_set_in_stri(v0,v1,v2,p,n,nset,sides)
FVECT v0,v1,v2,p;
FVECT n[3];
int *nset;
int sides[3];

{
    double d;
    /* Find the normal to the triangle ORIGIN:v0:v1 */
    if(!NTH_BIT(*nset,0))
    {
        VCROSS(n[0],v0,v1);
	SET_NTH_BIT(*nset,0);
    }
    /* Test the point for sidedness */
    d  = DOT(n[0],p);

    if(d > 0.0)
     {
       sides[0] =  GT_OUT;
       sides[1] = sides[2] = GT_INVALID;
       return(FALSE);
      }
    else
       sides[0] = GT_INTERIOR;
       
    /* Test next edge */
    if(!NTH_BIT(*nset,1))
    {
        VCROSS(n[1],v1,v2);
	SET_NTH_BIT(*nset,1);
    }
    /* Test the point for sidedness */
    d  = DOT(n[1],p);
    if(d > 0.0)
    {
	sides[1] = GT_OUT;
	sides[2] = GT_INVALID;
	return(FALSE);
    }
    else 
       sides[1] = GT_INTERIOR;
    /* Test next edge */
    if(!NTH_BIT(*nset,2))
    {
        VCROSS(n[2],v2,v0);
	SET_NTH_BIT(*nset,2);
    }
    /* Test the point for sidedness */
    d  = DOT(n[2],p);
    if(d > 0.0)
    {
      sides[2] = GT_OUT;
      return(FALSE);
    }
    else 
       sides[2] = GT_INTERIOR;
    /* Must be interior to the pyramid */
    return(GT_INTERIOR);
}



set_sidedness_tests(t0,t1,t2,p0,p1,p2,test,sides,nset,n)
   FVECT t0,t1,t2,p0,p1,p2;
   int test[3];
   int sides[3][3];
   int nset;
   FVECT n[3];
{
    int t;
    double d;

    
    /* p=0 */
    test[0] = 0;
    if(sides[0][0] == GT_INVALID)
    {
      if(!NTH_BIT(nset,0))
	VCROSS(n[0],t0,t1);
      /* Test the point for sidedness */
      d  = DOT(n[0],p0);
      if(d >= 0.0)
	SET_NTH_BIT(test[0],0);
    }
    else
      if(sides[0][0] != GT_INTERIOR)
	SET_NTH_BIT(test[0],0);

    if(sides[0][1] == GT_INVALID)
    {
      if(!NTH_BIT(nset,1))
	VCROSS(n[1],t1,t2);
	/* Test the point for sidedness */
	d  = DOT(n[1],p0);
	if(d >= 0.0)
	  SET_NTH_BIT(test[0],1);
    }
    else
      if(sides[0][1] != GT_INTERIOR)
	SET_NTH_BIT(test[0],1);

    if(sides[0][2] == GT_INVALID)
    {
      if(!NTH_BIT(nset,2))
	VCROSS(n[2],t2,t0);
      /* Test the point for sidedness */
      d  = DOT(n[2],p0);
      if(d >= 0.0)
	SET_NTH_BIT(test[0],2);
    }
    else
      if(sides[0][2] != GT_INTERIOR)
	SET_NTH_BIT(test[0],2);

    /* p=1 */
    test[1] = 0;
    /* t=0*/
    if(sides[1][0] == GT_INVALID)
    {
      if(!NTH_BIT(nset,0))
	VCROSS(n[0],t0,t1);
      /* Test the point for sidedness */
      d  = DOT(n[0],p1);
      if(d >= 0.0)
	SET_NTH_BIT(test[1],0);
    }
    else
      if(sides[1][0] != GT_INTERIOR)
	SET_NTH_BIT(test[1],0);
    
    /* t=1 */
    if(sides[1][1] == GT_INVALID)
    {
      if(!NTH_BIT(nset,1))
	VCROSS(n[1],t1,t2);
      /* Test the point for sidedness */
      d  = DOT(n[1],p1);
      if(d >= 0.0)
	SET_NTH_BIT(test[1],1);
    }
    else
      if(sides[1][1] != GT_INTERIOR)
	SET_NTH_BIT(test[1],1);
       
    /* t=2 */
    if(sides[1][2] == GT_INVALID)
    {
      if(!NTH_BIT(nset,2))
	VCROSS(n[2],t2,t0);
      /* Test the point for sidedness */
      d  = DOT(n[2],p1);
      if(d >= 0.0)
	SET_NTH_BIT(test[1],2);
    }
    else
      if(sides[1][2] != GT_INTERIOR)
	SET_NTH_BIT(test[1],2);

    /* p=2 */
    test[2] = 0;
    /* t = 0 */
    if(sides[2][0] == GT_INVALID)
    {
      if(!NTH_BIT(nset,0))
	VCROSS(n[0],t0,t1);
      /* Test the point for sidedness */
      d  = DOT(n[0],p2);
      if(d >= 0.0)
	SET_NTH_BIT(test[2],0);
    }
    else
      if(sides[2][0] != GT_INTERIOR)
	SET_NTH_BIT(test[2],0);
    /* t=1 */
    if(sides[2][1] == GT_INVALID)
    {
      if(!NTH_BIT(nset,1))
	VCROSS(n[1],t1,t2);
      /* Test the point for sidedness */
      d  = DOT(n[1],p2);
      if(d >= 0.0)
	SET_NTH_BIT(test[2],1);
    }
    else
      if(sides[2][1] != GT_INTERIOR)
	SET_NTH_BIT(test[2],1);
    /* t=2 */
    if(sides[2][2] == GT_INVALID)
    {
      if(!NTH_BIT(nset,2))
	VCROSS(n[2],t2,t0);
      /* Test the point for sidedness */
      d  = DOT(n[2],p2);
      if(d >= 0.0)
	SET_NTH_BIT(test[2],2);
    }
    else
      if(sides[2][2] != GT_INTERIOR)
	SET_NTH_BIT(test[2],2);
}

double
point_on_sphere(ps,p,c)
FVECT ps,p,c;
{
  double d;
    VSUB(ps,p,c);    
    d= normalize(ps);
    return(d);
}

int
point_in_stri(v0,v1,v2,p)
FVECT v0,v1,v2,p;
{
    double d;
    FVECT n;  

    VCROSS(n,v0,v1);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
      return(FALSE);

    /* Test next edge */
    VCROSS(n,v1,v2);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
       return(FALSE);

    /* Test next edge */
    VCROSS(n,v2,v0);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
       return(FALSE);
    /* Must be interior to the pyramid */
    return(GT_INTERIOR);
}


int
ray_intersect_tri(orig,dir,v0,v1,v2,pt)
FVECT orig,dir;
FVECT v0,v1,v2;
FVECT pt;
{
  FVECT p0,p1,p2,p;
  FPEQ peq;
  int type;

  VSUB(p0,v0,orig);
  VSUB(p1,v1,orig);
  VSUB(p2,v2,orig);

  if(point_in_stri(p0,p1,p2,dir))
  {
      /* Intersect the ray with the triangle plane */
      tri_plane_equation(v0,v1,v2,&peq,FALSE);
      return(intersect_ray_plane(orig,dir,peq,NULL,pt));
  }
  return(FALSE);
}


calculate_view_frustum(vp,hv,vv,horiz,vert,near,far,fnear,ffar)
FVECT vp,hv,vv;
double horiz,vert,near,far;
FVECT fnear[4],ffar[4];
{
    double height,width;
    FVECT t,nhv,nvv,ndv;
    double w2,h2;
    /* Calculate the x and y dimensions of the near face */
    /* hv and vv are the horizontal and vertical vectors in the
       view frame-the magnitude is the dimension of the front frustum
       face at z =1
    */
    VCOPY(nhv,hv);
    VCOPY(nvv,vv);
    w2 = normalize(nhv);
    h2 = normalize(nvv);
    /* Use similar triangles to calculate the dimensions at z=near */
    width  = near*0.5*w2;
    height = near*0.5*h2;

    VCROSS(ndv,nvv,nhv);
    /* Calculate the world space points corresponding to the 4 corners
       of the front face of the view frustum
     */
    fnear[0][0] =  width*nhv[0] + height*nvv[0] + near*ndv[0] + vp[0] ;
    fnear[0][1] =  width*nhv[1] + height*nvv[1] + near*ndv[1] + vp[1];
    fnear[0][2] =  width*nhv[2] + height*nvv[2] + near*ndv[2] + vp[2];
    fnear[1][0] = -width*nhv[0] + height*nvv[0] + near*ndv[0] + vp[0];
    fnear[1][1] = -width*nhv[1] + height*nvv[1] + near*ndv[1] + vp[1];
    fnear[1][2] = -width*nhv[2] + height*nvv[2] + near*ndv[2] + vp[2];
    fnear[2][0] = -width*nhv[0] - height*nvv[0] + near*ndv[0] + vp[0];
    fnear[2][1] = -width*nhv[1] - height*nvv[1] + near*ndv[1] + vp[1];
    fnear[2][2] = -width*nhv[2] - height*nvv[2] + near*ndv[2] + vp[2];
    fnear[3][0] =  width*nhv[0] - height*nvv[0] + near*ndv[0] + vp[0];
    fnear[3][1] =  width*nhv[1] - height*nvv[1] + near*ndv[1] + vp[1];
    fnear[3][2] =  width*nhv[2] - height*nvv[2] + near*ndv[2] + vp[2];

    /* Now do the far face */
    width  = far*0.5*w2;
    height = far*0.5*h2;
    ffar[0][0] =  width*nhv[0] + height*nvv[0] + far*ndv[0] + vp[0] ;
    ffar[0][1] =  width*nhv[1] + height*nvv[1] + far*ndv[1] + vp[1] ;
    ffar[0][2] =  width*nhv[2] + height*nvv[2] + far*ndv[2] + vp[2] ;
    ffar[1][0] = -width*nhv[0] + height*nvv[0] + far*ndv[0] + vp[0] ;
    ffar[1][1] = -width*nhv[1] + height*nvv[1] + far*ndv[1] + vp[1] ;
    ffar[1][2] = -width*nhv[2] + height*nvv[2] + far*ndv[2] + vp[2] ;
    ffar[2][0] = -width*nhv[0] - height*nvv[0] + far*ndv[0] + vp[0] ;
    ffar[2][1] = -width*nhv[1] - height*nvv[1] + far*ndv[1] + vp[1] ;
    ffar[2][2] = -width*nhv[2] - height*nvv[2] + far*ndv[2] + vp[2] ;
    ffar[3][0] =  width*nhv[0] - height*nvv[0] + far*ndv[0] + vp[0] ;
    ffar[3][1] =  width*nhv[1] - height*nvv[1] + far*ndv[1] + vp[1] ;
    ffar[3][2] =  width*nhv[2] - height*nvv[2] + far*ndv[2] + vp[2] ;
}

int
max_index(v,r)
FVECT v;
double *r;
{
  double p[3];
  int i;

  p[0] = fabs(v[0]);
  p[1] = fabs(v[1]);
  p[2] = fabs(v[2]);
  i = (p[0]>=p[1])?((p[0]>=p[2])?0:2):((p[1]>=p[2])?1:2);  
  if(r)
    *r = p[i];
  return(i);
}

int
closest_point_in_tri(p0,p1,p2,p,p0id,p1id,p2id)
FVECT p0,p1,p2,p;
int p0id,p1id,p2id;
{
    double d,d1;
    int i;
    
    d =  DIST_SQ(p,p0);
    d1 = DIST_SQ(p,p1);
    if(d < d1)
    {
      d1 = DIST_SQ(p,p2);
      i = (d1 < d)?p2id:p0id;
    }
    else
    {
      d = DIST_SQ(p,p2);
      i = (d < d1)? p2id:p1id;
    }
    return(i);
}

/* Find the normalized barycentric coordinates of p relative to
 * triangle v0,v1,v2. Return result in coord
 */
bary2d(x1,y1,x2,y2,x3,y3,px,py,coord)
double x1,y1,x2,y2,x3,y3;
double px,py;
double coord[3];
{
  double a;

  a =  (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
  coord[0] = ((x2 - px) * (y3 - py) - (x3 - px) * (y2 - py)) / a; 
  coord[1] = ((x3 - px) * (y1 - py) - (x1 - px) * (y3 - py)) / a;
  coord[2] = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / a;
 
}




bary_parent(coord,i)
BCOORD coord[3];
int i;
{
  switch(i) {
  case 0:
    /* update bary for child */
    coord[0] = (coord[0] >> 1) + MAXBCOORD4;
    coord[1] >>= 1;
    coord[2] >>= 1;
    break;
  case 1:
    coord[0] >>= 1;
    coord[1]  = (coord[1] >> 1) + MAXBCOORD4;
    coord[2] >>= 1;
    break;
    
  case 2:
    coord[0] >>= 1;
    coord[1] >>= 1;
    coord[2] = (coord[2] >> 1) + MAXBCOORD4;
    break;
    
  case 3:
    coord[0] = MAXBCOORD4 - (coord[0] >> 1);
    coord[1] = MAXBCOORD4 - (coord[1] >> 1);
    coord[2] = MAXBCOORD4 - (coord[2] >> 1);
    break;
#ifdef DEBUG
  default:
    eputs("bary_parent():Invalid child\n");
    break;
#endif
  }
}

bary_from_child(coord,child,next)
BCOORD coord[3];
int child,next;
{
#ifdef DEBUG
  if(child <0 || child > 3)
  {
    eputs("bary_from_child():Invalid child\n");
    return;
  }
  if(next <0 || next > 3)
  {
    eputs("bary_from_child():Invalid next\n");
    return;
  }
#endif
  if(next == child)
    return;

  switch(child){
  case 0:
      coord[0] = 0;
      coord[1] = MAXBCOORD2 - coord[1];
      coord[2] = MAXBCOORD2 - coord[2];
      break;
  case 1:
      coord[0] = MAXBCOORD2 - coord[0];
      coord[1] = 0;
      coord[2] = MAXBCOORD2 - coord[2];
      break;
  case 2:
      coord[0] = MAXBCOORD2 - coord[0];
      coord[1] = MAXBCOORD2 - coord[1];
      coord[2] = 0;
    break;
  case 3:
    switch(next){
    case 0:
      coord[0] = 0;
      coord[1] = MAXBCOORD2 - coord[1];
      coord[2] = MAXBCOORD2 - coord[2];
      break;
    case 1:
      coord[0] = MAXBCOORD2 - coord[0];
      coord[1] = 0;
      coord[2] = MAXBCOORD2 - coord[2];
      break;
    case 2:
      coord[0] = MAXBCOORD2 - coord[0];
      coord[1] = MAXBCOORD2 - coord[1];
      coord[2] = 0;
      break;
    }
    break;
  }
}

int
bary_child(coord)
BCOORD coord[3];
{
  int i;

  if(coord[0] > MAXBCOORD4)
  { 
      /* update bary for child */
      coord[0] = (coord[0]<< 1) - MAXBCOORD2;
      coord[1] <<= 1;
      coord[2] <<= 1;
      return(0);
  }
  else
    if(coord[1] > MAXBCOORD4)
    {
      coord[0] <<= 1;
      coord[1] = (coord[1] << 1) - MAXBCOORD2;
      coord[2] <<= 1;
      return(1);
    }
    else
      if(coord[2] > MAXBCOORD4)
      {
	coord[0] <<= 1;
	coord[1] <<= 1;
	coord[2] = (coord[2] << 1) - MAXBCOORD2;
	return(2);
      }
      else
	 {
	   coord[0] = MAXBCOORD2 - (coord[0] << 1);
	   coord[1] = MAXBCOORD2 - (coord[1] << 1);
	   coord[2] = MAXBCOORD2 - (coord[2] << 1);
	   return(3);
	 }
}

int
bary_nth_child(coord,i)
BCOORD coord[3];
int i;
{

  switch(i){
  case 0:
    /* update bary for child */
    coord[0] = (coord[0]<< 1) - MAXBCOORD2;
    coord[1] <<= 1;
    coord[2] <<= 1;
    break;
  case 1:
    coord[0] <<= 1;
    coord[1] = (coord[1] << 1) - MAXBCOORD2;
    coord[2] <<= 1;
    break;
  case 2:
    coord[0] <<= 1;
    coord[1] <<= 1;
    coord[2] = (coord[2] << 1) - MAXBCOORD2;
    break;
  case 3:
    coord[0] = MAXBCOORD2 - (coord[0] << 1);
    coord[1] = MAXBCOORD2 - (coord[1] << 1);
    coord[2] = MAXBCOORD2 - (coord[2] << 1);
    break;
  }
}



