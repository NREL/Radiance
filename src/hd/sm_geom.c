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


int
convex_angle(v0,v1,v2)
FVECT v0,v1,v2;
{
    FVECT cp01,cp12,cp;
    
    /* test sign of (v0Xv1)X(v1Xv2). v1 */
    VCROSS(cp01,v0,v1);
    VCROSS(cp12,v1,v2);
    VCROSS(cp,cp01,cp12);
    if(DOT(cp,v1) < 0.0)
       return(FALSE);
    return(TRUE);
}

/* calculates the normal of a face contour using Newell's formula. e

               a =  SUMi (yi - yi+1)(zi + zi+1)
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


tri_plane_equation(v0,v1,v2,n,nd,norm)
   FVECT v0,v1,v2,n;
   double *nd;
   int norm;
{
    tri_normal(v0,v1,v2,n,norm);

    *nd = -(DOT(n,v0));
}

/* From quad_edge-code */
int
point_in_circle_thru_origin(p,p0,p1)
FVECT p;
FVECT p0,p1;
{

    double dp0,dp1;
    double dp,det;
    
    dp0 = DOT_VEC2(p0,p0);
    dp1 = DOT_VEC2(p1,p1);
    dp  = DOT_VEC2(p,p);    

    det = -dp0*CROSS_VEC2(p1,p) + dp1*CROSS_VEC2(p0,p) - dp*CROSS_VEC2(p0,p1);
    
    return (det > 0);
}



point_on_sphere(ps,p,c)
FVECT ps,p,c;
{
    VSUB(ps,p,c);    
    normalize(ps);
}


/* returns TRUE if ray from origin in direction v intersects plane defined
  * by normal plane_n, and plane_d. If plane is not parallel- returns
  * intersection point if r != NULL. If tptr!= NULL returns value of
  * t, if parallel, returns t=FHUGE
  */
int
intersect_vector_plane(v,plane_n,plane_d,tptr,r)
   FVECT v,plane_n;
   double plane_d;
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
  d = -(DOT(plane_n,v));
  if(ZERO(d))
  {
      t = FHUGE;
      hit = 0;
  }
  else
  {
      t =  plane_d/d;
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
intersect_ray_plane(orig,dir,plane_n,plane_d,pd,r)
   FVECT orig,dir;
   FVECT plane_n;
   double plane_d;
   double *pd;
   FVECT r;
{
  double t;
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
    t =  -(DOT(plane_n,orig) + plane_d)/(DOT(plane_n,dir));
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
intersect_edge_plane(e0,e1,plane_n,plane_d,pd,r)
   FVECT e0,e1;
   FVECT plane_n;
   double plane_d;
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
  t =  -(DOT(plane_n,e0) + plane_d)/(DOT(plane_n,d));
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
point_in_cone(p,p0,p1,p2)
FVECT p;
FVECT p0,p1,p2;
{
    FVECT n;
    FVECT np,x_axis,y_axis;
    double d1,d2,d;
    
    /* Find the equation of the circle defined by the intersection
       of the cone with the plane defined by p1,p2,p3- project p into
       that plane and do an in-circle test in the plane
     */
    
    /* find the equation of the plane defined by p1-p3 */
    tri_plane_equation(p0,p1,p2,n,&d,FALSE);

    /* define a coordinate system on the plane: the x axis is in
       the direction of np2-np1, and the y axis is calculated from
       n cross x-axis
     */
    /* Project p onto the plane */
    /* NOTE: check this: does sideness check?*/
    if(!intersect_vector_plane(p,n,d,NULL,np))
	return(FALSE);

    /* create coordinate system on  plane: p2-p1 defines the x_axis*/
    VSUB(x_axis,p1,p0);
    normalize(x_axis);
    /* The y axis is  */
    VCROSS(y_axis,n,x_axis);
    normalize(y_axis);

    VSUB(p1,p1,p0);
    VSUB(p2,p2,p0);
    VSUB(np,np,p0);
    
    p1[0] = VLEN(p1);
    p1[1] = 0;

    d1 = DOT(p2,x_axis);
    d2 = DOT(p2,y_axis);
    p2[0] = d1;
    p2[1] = d2;
    
    d1 = DOT(np,x_axis);
    d2 = DOT(np,y_axis);
    np[0] = d1;
    np[1] = d2;

    /* perform the in-circle test in the new coordinate system */
    return(point_in_circle_thru_origin(np,p1,p2));
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
        VCROSS(n[0],v1,v0);
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
        VCROSS(n[1],v2,v1);
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
        VCROSS(n[2],v0,v2);
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




int
point_in_stri(v0,v1,v2,p)
FVECT v0,v1,v2,p;
{
    double d;
    FVECT n;  

    VCROSS(n,v1,v0);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
      return(FALSE);

    /* Test next edge */
    VCROSS(n,v2,v1);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
       return(FALSE);

    /* Test next edge */
    VCROSS(n,v0,v2);
    /* Test the point for sidedness */
    d  = DOT(n,p);
    if(d > 0.0)
       return(FALSE);
    /* Must be interior to the pyramid */
    return(GT_INTERIOR);
}

int
vertices_in_stri(t0,t1,t2,p0,p1,p2,nset,n,avg,pt_sides)
FVECT t0,t1,t2,p0,p1,p2;
int *nset;
FVECT n[3];
FVECT avg;
int pt_sides[3][3];

{
    int below_plane[3],test;

    SUM_3VEC3(avg,t0,t1,t2); 
    *nset = 0;
    /* Test vertex v[i] against triangle j*/
    /* Check if v[i] lies below plane defined by avg of 3 vectors
       defining triangle
       */

    /* test point 0 */
    if(DOT(avg,p0) < 0.0)
      below_plane[0] = 1;
    else
      below_plane[0] = 0;
    /* Test if b[i] lies in or on triangle a */
    test = point_set_in_stri(t0,t1,t2,p0,n,nset,pt_sides[0]);
    /* If pts[i] is interior: done */
    if(!below_plane[0])
      {
	if(test == GT_INTERIOR) 
	  return(TRUE);
      }
    /* Now test point 1*/

    if(DOT(avg,p1) < 0.0)
      below_plane[1] = 1;
    else
      below_plane[1]=0;
    /* Test if b[i] lies in or on triangle a */
    test = point_set_in_stri(t0,t1,t2,p1,n,nset,pt_sides[1]);
    /* If pts[i] is interior: done */
    if(!below_plane[1])
    {
      if(test == GT_INTERIOR) 
	return(TRUE);
    }
    
    /* Now test point 2 */
    if(DOT(avg,p2) < 0.0)
      below_plane[2] = 1;
    else
      below_plane[2] = 0;
	/* Test if b[i] lies in or on triangle a */
    test = point_set_in_stri(t0,t1,t2,p2,n,nset,pt_sides[2]);

    /* If pts[i] is interior: done */
    if(!below_plane[2])
      {
	if(test == GT_INTERIOR) 
	  return(TRUE);
      }

    /* If all three points below separating plane: trivial reject */
    if(below_plane[0] && below_plane[1] && below_plane[2])
       return(FALSE);
    /* Now check vertices in a against triangle b */
    return(FALSE);
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
	VCROSS(n[0],t1,t0);
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
	VCROSS(n[1],t2,t1);
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
	VCROSS(n[2],t0,t2);
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
	VCROSS(n[0],t1,t0);
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
	VCROSS(n[1],t2,t1);
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
	VCROSS(n[2],t0,t2);
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
	VCROSS(n[0],t1,t0);
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
	VCROSS(n[1],t2,t1);
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
	VCROSS(n[2],t0,t2);
      /* Test the point for sidedness */
      d  = DOT(n[2],p2);
      if(d >= 0.0)
	SET_NTH_BIT(test[2],2);
    }
    else
      if(sides[2][2] != GT_INTERIOR)
	SET_NTH_BIT(test[2],2);
}


int
stri_intersect(a1,a2,a3,b1,b2,b3)
FVECT a1,a2,a3,b1,b2,b3;
{
  int which,test,n_set[2];
  int sides[2][3][3],i,j,inext,jnext;
  int tests[2][3];
  FVECT n[2][3],p,avg[2];
 
  /* Test the vertices of triangle a against the pyramid formed by triangle
     b and the origin. If any vertex of a is interior to triangle b, or
     if all 3 vertices of a are ON the edges of b,return TRUE. Remember
     the results of the edge normal and sidedness tests for later.
   */
 if(vertices_in_stri(a1,a2,a3,b1,b2,b3,&(n_set[0]),n[0],avg[0],sides[1]))
     return(TRUE);
  
 if(vertices_in_stri(b1,b2,b3,a1,a2,a3,&(n_set[1]),n[1],avg[1],sides[0]))
     return(TRUE);


  set_sidedness_tests(b1,b2,b3,a1,a2,a3,tests[0],sides[0],n_set[1],n[1]);
  if(tests[0][0]&tests[0][1]&tests[0][2])
    return(FALSE);

  set_sidedness_tests(a1,a2,a3,b1,b2,b3,tests[1],sides[1],n_set[0],n[0]);
  if(tests[1][0]&tests[1][1]&tests[1][2])
    return(FALSE);

  for(j=0; j < 3;j++)
  {
    jnext = (j+1)%3;
    /* IF edge b doesnt cross any great circles of a, punt */
    if(tests[1][j] & tests[1][jnext])
      continue;
    for(i=0;i<3;i++)
    {
      inext = (i+1)%3;
      /* IF edge a doesnt cross any great circles of b, punt */
      if(tests[0][i] & tests[0][inext])
	continue;
      /* Now find the great circles that cross and test */
      if((NTH_BIT(tests[0][i],j)^(NTH_BIT(tests[0][inext],j))) 
	  && (NTH_BIT(tests[1][j],i)^NTH_BIT(tests[1][jnext],i)))
      {
	VCROSS(p,n[0][i],n[1][j]);
		     
	/* If zero cp= done */
	if(ZERO_VEC3(p))
	  continue;
	/* check above both planes */
	if(DOT(avg[0],p) < 0 || DOT(avg[1],p) < 0)
	  {
	    NEGATE_VEC3(p);
	    if(DOT(avg[0],p) < 0 || DOT(avg[1],p) < 0)
	      continue;
	  }
	return(TRUE);
      }
    }
  } 
  return(FALSE);
}

int
ray_intersect_tri(orig,dir,v0,v1,v2,pt)
FVECT orig,dir;
FVECT v0,v1,v2;
FVECT pt;
{
  FVECT p0,p1,p2,p,n;
  double pd;
  int type;

  VSUB(p0,v0,orig);
  VSUB(p1,v1,orig);
  VSUB(p2,v2,orig);

  if(point_in_stri(p0,p1,p2,dir))
  {
      /* Intersect the ray with the triangle plane */
      tri_plane_equation(v0,v1,v2,n,&pd,FALSE);
      return(intersect_ray_plane(orig,dir,n,pd,NULL,pt));
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
sedge_intersect(a0,a1,b0,b1)
FVECT a0,a1,b0,b1;
{
    FVECT na,nb,avga,avgb,p;
    double d;
    int sb0,sb1,sa0,sa1;

    /* First test if edge b straddles great circle of a */
    VCROSS(na,a0,a1);
    d = DOT(na,b0);
    sb0 = ZERO(d)?0:(d<0)? -1: 1;
    d = DOT(na,b1);
    sb1 = ZERO(d)?0:(d<0)? -1: 1;
    /* edge b entirely on one side of great circle a: edges cannot intersect*/
    if(sb0*sb1 > 0)
       return(FALSE);
    /* test if edge a straddles great circle of b */
    VCROSS(nb,b0,b1);
    d = DOT(nb,a0);
    sa0 = ZERO(d)?0:(d<0)? -1: 1;
    d = DOT(nb,a1);
    sa1 = ZERO(d)?0:(d<0)? -1: 1;
    /* edge a entirely on one side of great circle b: edges cannot intersect*/
    if(sa0*sa1 > 0)
       return(FALSE);

    /* Find one of intersection points of the great circles */
    VCROSS(p,na,nb);
    /* If they lie on same great circle: call an intersection */
    if(ZERO_VEC3(p))
       return(TRUE);

    VADD(avga,a0,a1);
    VADD(avgb,b0,b1);
    if(DOT(avga,p) < 0 || DOT(avgb,p) < 0)
    {
      NEGATE_VEC3(p);
      if(DOT(avga,p) < 0 || DOT(avgb,p) < 0)
	return(FALSE);
    }
    if((!sb0 || !sb1) && (!sa0 || !sa1))
      return(FALSE);
    return(TRUE);
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
  coord[2]  = 1.0 - coord[0] - coord[1];
 
}

bary_ith_child(coord,i)
double coord[3];
 int i;
{
 
  switch(i){
  case 0:
      /* update bary for child */
      coord[0] = 2.0*coord[0]- 1.0;
      coord[1] *= 2.0;
      coord[2] *= 2.0;
      break;
  case 1:
    coord[0] *= 2.0;
    coord[1] = 2.0*coord[1]- 1.0;
    coord[2] *= 2.0;
    break;
  case 2:
    coord[0] *= 2.0;
    coord[1] *= 2.0;
    coord[2] = 2.0*coord[2]- 1.0;
    break;
  case 3:
    coord[0] = 1.0 - 2.0*coord[0];
    coord[1] = 1.0 - 2.0*coord[1];
    coord[2] = 1.0 - 2.0*coord[2];
    break;
#ifdef DEBUG
  default:
    eputs("bary_ith_child():Invalid child\n");
    break;
#endif
  }
}


int
bary_child(coord)
double coord[3];
{
  int i;

  if(coord[0] > 0.5)
  { 
      /* update bary for child */
      coord[0] = 2.0*coord[0]- 1.0;
      coord[1] *= 2.0;
      coord[2] *= 2.0;
      return(0);
  }
  else
    if(coord[1] > 0.5)
    {
      coord[0] *= 2.0;
      coord[1] = 2.0*coord[1]- 1.0;
      coord[2] *= 2.0;
      return(1);
    }
    else
      if(coord[2] > 0.5)
      {
	coord[0] *= 2.0;
	coord[1] *= 2.0;
	coord[2] = 2.0*coord[2]- 1.0;
	return(2);
      }
      else
	 {
	   coord[0] = 1.0 - 2.0*coord[0];
	   coord[1] = 1.0 - 2.0*coord[1];
	   coord[2] = 1.0 - 2.0*coord[2];
	   return(3);
	 }
}

/* Coord was the ith child of the parent: set the coordinate
   relative to the parent
*/
bary_parent(coord,i)
double coord[3];
int i;
{

  switch(i) {
  case 0:
    /* update bary for child */
    coord[0] = coord[0]*0.5 + 0.5;
    coord[1] *= 0.5;
    coord[2] *= 0.5;
    break;
  case 1:
    coord[0] *= 0.5;
    coord[1]  = coord[1]*0.5 + 0.5;
    coord[2] *= 0.5;
    break;
    
  case 2:
    coord[0] *= 0.5;
    coord[1] *= 0.5;
    coord[2] = coord[2]*0.5 + 0.5;
    break;
    
  case 3:
    coord[0] = 0.5 - 0.5*coord[0];
    coord[1] = 0.5 - 0.5*coord[1];
    coord[2] = 0.5 - 0.5*coord[2];
    break;
#ifdef DEBUG
  default:
    eputs("bary_parent():Invalid child\n");
    break;
#endif
  }
}

bary_from_child(coord,child,next)
double coord[3];
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
    switch(next){
    case 1:
      coord[0] += 1.0;
      coord[1] -= 1.0;
      break;
    case 2:
      coord[0] += 1.0;
      coord[2] -= 1.0;
      break;
    case 3:
      coord[0] *= -1.0;
      coord[1] = 1 - coord[1];
      coord[2] = 1 - coord[2];
      break;

    }
    break;
  case 1:
    switch(next){
    case 0:
      coord[0] -= 1.0;
      coord[1] += 1.0;
      break;
    case 2:
      coord[1] += 1.0;
      coord[2] -= 1.0;
      break;
    case 3:
      coord[0] = 1 - coord[0];
      coord[1] *= -1.0;
      coord[2] = 1 - coord[2];
      break;
    }
    break;
  case 2:
    switch(next){
    case 0:
      coord[0] -= 1.0;
      coord[2] += 1.0;
      break;
    case 1:
      coord[1] -= 1.0;
      coord[2] += 1.0;
      break;
    case 3:
      coord[0] = 1 - coord[0];
      coord[1] = 1 - coord[1];
      coord[2] *= -1.0;
      break;
    }
    break;
  case 3:
    switch(next){
    case 0:
      coord[0] *= -1.0;
      coord[1] = 1 - coord[1];
      coord[2] = 1 - coord[2];
      break;
    case 1:
      coord[0] = 1 - coord[0];
      coord[1] *= -1.0;
      coord[2] = 1 - coord[2];
      break;
    case 2:
      coord[0] = 1 - coord[0];
      coord[1] = 1 - coord[1];
      coord[2] *= -1.0;
      break;
    }
    break;
  }
}

int
max_index(v)
FVECT v;
{
  double a,b,c;
  int i;

  a = fabs(v[0]);
  b = fabs(v[1]);
  c = fabs(v[2]);
  i = (a>=b)?((a>=c)?0:2):((b>=c)?1:2);  
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







