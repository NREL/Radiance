/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

#ifdef  SMLFLT
#define  FLOAT		float
#define  FTINY		(1e-3)
#else
#define  FLOAT		double
#define  FTINY		(1e-6)
#endif
#define  FHUGE		(1e10)

typedef FLOAT  FVECT[3];

#define  VCOPY(v1,v2)	((v1)[0]=(v2)[0],(v1)[1]=(v2)[1],(v1)[2]=(v2)[2])
#define  DOT(v1,v2)	((v1)[0]*(v2)[0]+(v1)[1]*(v2)[1]+(v1)[2]*(v2)[2])
#define  VLEN(v)	sqrt(DOT(v,v))
#define  VADD(vr,v1,v2)	((vr)[0]=(v1)[0]+(v2)[0], \
				(vr)[1]=(v1)[1]+(v2)[1], \
				(vr)[2]=(v1)[2]+(v2)[2])
#define  VSUB(vr,v1,v2)	((vr)[0]=(v1)[0]-(v2)[0], \
				(vr)[1]=(v1)[1]-(v2)[1], \
				(vr)[2]=(v1)[2]-(v2)[2])
#define  VSUM(vr,v1,v2,f)	((vr)[0]=(v1)[0]+(f)*(v2)[0], \
				(vr)[1]=(v1)[1]+(f)*(v2)[1], \
				(vr)[2]=(v1)[2]+(f)*(v2)[2])
#define  VCROSS(vr,v1,v2) \
			((vr)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1], \
			(vr)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2], \
			(vr)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])

extern double  fdot(), dist2(), dist2lseg(), dist2line(), normalize();
