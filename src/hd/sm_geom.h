/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm_geom.h
 */

/* Assumes included after standard.h  */

#include <values.h>

#define F_TINY 1e-10
#define FZERO(x) ((x) < F_TINY && (x) > -F_TINY)
#define FEQUAL(a,b) FZERO((a) - (b))

#define ZERO(x) ((x) < FTINY && (x) > -FTINY)
#define EQUAL(a,b) ZERO((a) - (b))

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef struct _FPEQ {
    FVECT n;
    double d;
    char x,y,z;
}FPEQ;

#define FP_N(f) ((f).n)
#define FP_D(f) ((f).d)
#define FP_X(f) ((f).x) 
#define FP_Y(f) ((f).y) 
#define FP_Z(f) ((f).z) 
  
typedef long BCOORD;
typedef long BDIR;
typedef long TINT;

#define BITS_BCOORD     (BITS(long)-2)
#define SHIFT_MAXBCOORD (BITS_BCOORD-1)   
#define MAXBCOORD      ((1L << BITS_BCOORD)-1)
#define MAXBCOORD2      (MAXBCOORD>>1)
#define MAXBCOORD4      (MAXBCOORD2>>1)

#define M_2_3_PI PI*2/3

#ifndef INVALID
#define INVALID -1
#endif

#define GT_INVALID  0
#define GT_VERTEX   1
#define GT_EDGE     2
#define GT_FACE     4
#define GT_INTERIOR 8
#define GT_INTERSECT 16
#define GT_ADJACENT  32
#define GT_OUT       64

#define IADDV3(v,a)  ((v)[0] += (a)[0],(v)[1] += (a)[1],(v)[2] += (a)[2])
#define ISUBV3(v,a)  ((v)[0] -= (a)[0],(v)[1] -= (a)[1],(v)[2] -= (a)[2])
#define ISCALEV3(v,a)  ((v)[0] *= (a),(v)[1] *= (a),(v)[2] *= (a))
#define IDIVV3(v,a)  ((v)[0] /= (a),(v)[1] /= (a),(v)[2] /= (a))


#define ADDV3(v,a,b) ((v)[0] = (a)[0]+(b)[0],(v)[1] = (a)[1]+(b)[1],\
		      (v)[2] = (a)[2]+(b)[2])
#define SUBV3(v,a,b) ((v)[0] = (a)[0]-(b)[0],(v)[1] = (a)[1]-(b)[1],\
		      (v)[2] = (a)[2]-(b)[2])
#define SUMV3(v,a,b,s) ((v)[0] = (a)[0]+(s)*(b)[0],(v)[1]=(a)[1]+(s)*(b)[1],\
		      (v)[2] = (a)[2]+(s)*(b)[2])
#define SCALEV3(v,a,s)  ((v)[0]=(a)[0]*(s),(v)[1]=(a)[1]*(s),(v)[2]=(a)[2]*(s))
#define ZERO_VEC3(v)     (ZERO(v[0]) && ZERO(v[1]) && ZERO(v[2]) )
#define EQUAL_VEC3(a,b)  (FEQUAL(a[0],b[0])&&FEQUAL(a[1],b[1])&&FEQUAL(a[2],b[2]))
#define OPP_EQUAL_VEC3(a,b)  (EQUAL(a[0],-b[0])&&EQUAL(a[1],-b[1])&&EQUAL(a[2],-b[2]))
#define FZERO_VEC3(v)     (FZERO(v[0]) && FZERO(v[1]) && FZERO(v[2]) )
#define FEQUAL_VEC3(a,b) (FEQUAL(a[0],b[0])&&FEQUAL(a[1],b[1])&&FEQUAL(a[2],b[2]))
#define NEGATE_VEC3(v)   ((v)[0] *= -1.0,(v)[1] *= -1.0,(v)[2] *= -1.0)
#define COPY_VEC2(v1,v2) ((v1)[0]=(v2)[0],(v1)[1]=(v2)[1])
#define DIST(a,b)         (sqrt(((a)[0]-(b)[0])*((a)[0]-(b)[0]) + \
			        ((a)[1]-(b)[1])*((a)[1]-(b)[1]) + \
				((a)[2]-(b)[2])*((a)[2]-(b)[2])))
#define DIST_SQ(a,b)      (((a)[0]-(b)[0])*((a)[0]-(b)[0]) + \
			        ((a)[1]-(b)[1])*((a)[1]-(b)[1]) + \
				((a)[2]-(b)[2])*((a)[2]-(b)[2]))

#define SIGN(x) ((x<0)?-1:1)
#define CROSS_VEC2(v1,v2) (((v1)[0]*(v2)[1]) - ((v1)[1]*(v2)[0])) 
#define DOT_VEC2(v1,v2) ((v1)[0]*(v2)[0] + (v1)[1]*(v2)[1])

#define EDGE_MIDPOINT(a,v1,v2) ((a)[0]=((v1)[0]+(v2)[0])*0.5, \
(a)[1]=((v1)[1]+(v2)[1])*0.5,(a)[2] = ((v1)[2]+(v2)[2])*0.5)

#define MIN_VEC3(v) ((v)[0]<(v)[1]?((v)[0]<(v)[2]?(v)[0]:v[2]): \
		     (v)[1]<(v)[2]?(v)[1]:(v)[2])
#define MAX3(a,b,c) (((b)>(a))?((b) > (c))?(b):(c):((a)>(c))?(a):(c))   
#define MIN3(a,b,c) (((b)<(a))?((b) < (c))?(b):(c):((a)<(c))?(a):(c))   		   
#define MAX(a,b)    (((b)>(a))?(b):(a))   
#define MIN(a,b)  (((b)<(a))?(b):(a))   

#define SUM_3VEC3(r,a,b,c) ((r)[0]=(a)[0]+(b)[0]+(c)[0], \
   (r)[1]=(a)[1]+(b)[1]+(c)[1],(r)[2]=(a)[2]+(b)[2]+(c)[2])

#define NTH_BIT(n,i)         ((n) & (1<<(i)))
#define SET_NTH_BIT(n,i)     ((n) |= (1<<(i)))   


/* int convex_angle(FVECT v0,FVECT v1,FVECT v2) */
/* void triangle_centroid(FVECT v0,FVECT v1,FVECT v2,FVECT c) */
/* void triangle_plane_equation(FVECT v0,FVECT v1,FVECT v2,FVECT n,double *nd,
        char norm) */
/* int vec3_equal(FVECT v1,v2) */
/* int point_relative_to_plane(FVECT p,FVECT n, double nd) */
/* int point_in_circle(FVECT p,FVECT p0,FVECT p1) */
/* int intersect_line_plane(FVECT r,FVECT p1,FVECT p2,float *plane) */
/* int point_in_cone(FVECT p,FVECT p1,FVECT p2,FVECT p3,FVECT p4) */   
/* void point_on_sphere(FVECT ps,FVECT p,FVECT c) */
/* int test_point_against_spherical_tri(FVECT v0,FVECT v1,FVECT v2,FVECT p,
       FVECT n,char *nset,char *which,char sides[3]) */
/* int test_single_point_against_spherical_tri(FVECT v0,FVECT v1,FVECT v2,
       FVECT p,char *which )*/
/* int test_vertices_for_tri_inclusion(FVECT tri[3],FVECT pts[3],char *nset,
       FVECT n[3],FVECT avg,char pt_sides[3][3]); */
/* void set_sidedness_tests(FVECT tri[3],FVECT pts[3],char test[3],
        char sides[3][3],char nset,FVECT n[3])
 */
/* int cs_spherical_edge_edge_test(FVECT n[2][3],int i,int j,FVECT avg[2]) */
/* int spherical_tri_tri_intersect(FVECT a1,FVECT a2,FVECT a3,
				 FVECT b1,FVECT b2,FVECT b3) */
   
/* void calculate_view_frustum(FVECT vp,hv,vv,double horiz,vert,near,far,
   FVECT fnear[4],FVECT ffar[4])
 */
/* double triangle_normal_Newell(FVECT v0,FVECT v1,FVECT v2,FVECT n,char n)*/
double tri_normal();
/* double spherical_edge_normal(FVECT v0,FVECT v1,FVECT n,char norm) */
double spherical_edge_normal();
double point_on_sphere();

#define point_in_stri_n(n0,n1,n2,p) \
  ((DOT(n0,p)<=FTINY)&&(DOT(n1,p)<=FTINY)&&(DOT(n2,p)<=FTINY))

#define PT_ON_PLANE(p,peq) (DOT(FP_N(peq),p)+FP_D(peq))

