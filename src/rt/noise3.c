/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  noise3.c - noise functions for random textures.
 *
 *     Credit for the smooth algorithm goes to Ken Perlin.
 *     (ref. SIGGRAPH Vol 19, No 3, pp 287-96)
 *
 *     4/15/86
 *     5/19/88	Added fractal noise function
 */


#define  A		0
#define  B		1
#define  C		2
#define  D		3

#define  rand3a(x,y,z)	frand(67*(x)+59*(y)+71*(z))
#define  rand3b(x,y,z)	frand(73*(x)+79*(y)+83*(z))
#define  rand3c(x,y,z)	frand(89*(x)+97*(y)+101*(z))
#define  rand3d(x,y,z)	frand(103*(x)+107*(y)+109*(z))

#define  hermite(p0,p1,r0,r1,t)  (	p0*((2.0*t-3.0)*t*t+1.0) + \
					p1*(-2.0*t+3.0)*t*t + \
					r0*((t-2.0)*t+1.0)*t + \
					r1*(t-1.0)*t*t )

double  *noise3(), noise3coef(), argument(), frand();

static long  xlim[3][2];
static double  xarg[3];

#define  EPSILON	.005		/* error allowed in fractal */

#define  frand3(x,y,z)	frand((long)((12.38*(x)-22.30*(y)-42.63*(z))/EPSILON))

double  fnoise3();


double
l_noise3()			/* compute 3-dimensional noise function */
{
	return(noise3coef(D));
}


double
l_noise3a()			/* compute x slope of noise function */
{
	return(noise3coef(A));
}


double
l_noise3b()			/* compute y slope of noise function */
{
	return(noise3coef(B));
}


double
l_noise3c()			/* compute z slope of noise function */
{
	return(noise3coef(C));
}


double
l_fnoise3()			/* compute fractal noise function */
{
	double  x[3];

	x[0] = argument(1);
	x[1] = argument(2);
	x[2] = argument(3);

	return(fnoise3(x));
}


static double
noise3coef(coef)		/* return coefficient of noise function */
int  coef;
{
	double  x[3];

	x[0] = argument(1);
	x[1] = argument(2);
	x[2] = argument(3);

	return(noise3(x)[coef]);
}


double *
noise3(xnew)			/* compute the noise function */
register double  xnew[3];
{
	extern double  floor();
	static double  x[3] = {-100000.0, -100000.0, -100000.0};
	static double  f[4];

	if (x[0]==xnew[0] && x[1]==xnew[1] && x[2]==xnew[2])
		return(f);
	x[0] = xnew[0]; x[1] = xnew[1]; x[2] = xnew[2];
	xlim[0][0] = floor(x[0]); xlim[0][1] = xlim[0][0] + 1;
	xlim[1][0] = floor(x[1]); xlim[1][1] = xlim[1][0] + 1;
	xlim[2][0] = floor(x[2]); xlim[2][1] = xlim[2][0] + 1;
	xarg[0] = x[0] - xlim[0][0];
	xarg[1] = x[1] - xlim[1][0];
	xarg[2] = x[2] - xlim[2][0];
	interpolate(f, 0, 3);
	return(f);
}


static
interpolate(f, i, n)
double  f[4];
register int  i, n;
{
	double  f0[4], f1[4];

	if (n == 0) {
		f[A] = rand3a(xlim[0][i&1],xlim[1][i>>1&1],xlim[2][i>>2]);
		f[B] = rand3b(xlim[0][i&1],xlim[1][i>>1&1],xlim[2][i>>2]);
		f[C] = rand3c(xlim[0][i&1],xlim[1][i>>1&1],xlim[2][i>>2]);
		f[D] = rand3d(xlim[0][i&1],xlim[1][i>>1&1],xlim[2][i>>2]);
	} else {
		n--;
		interpolate(f0, i, n);
		interpolate(f1, i | 1<<n, n);
		f[A] = (1.0-xarg[n])*f0[A] + xarg[n]*f1[A];
		f[B] = (1.0-xarg[n])*f0[B] + xarg[n]*f1[B];
		f[C] = (1.0-xarg[n])*f0[C] + xarg[n]*f1[C];
		f[D] = hermite(f0[D], f1[D], f0[n], f1[n], xarg[n]);
	}
}


double
frand(s)			/* get random number from seed */
register long  s;
{
	s = s<<13 ^ s;
	return(1.0-((s*(s*s*15731+789221)+1376312589)&0x7fffffff)/1073741824.0);
}


double
l_hermite()			/* library call for hermite interpolation */
{
	double  t;
	
	t = argument(5);
	return( hermite(argument(1), argument(2),
			argument(3), argument(4), t) );
}


double
fnoise3(p)			/* compute fractal noise function */
register double  p[3];
{
	double  floor();
	double  v[3], beg[3], fval[8], s, fc;
	int  closing, branch;
	register int  i, j;
						/* get starting cube */
	for (i = 0; i < 3; i++)
		beg[i] = floor(p[i]);
	for (j = 0; j < 8; j++) {
		for (i = 0; i < 3; i++) {
			v[i] = beg[i];
			if (j & 1<<i)
				v[i] += 1.0;
		}
		fval[j] = frand3(v[0],v[1],v[2]);
	}
	s = 1.0;
						/* compute fractal */
	for ( ; ; ) {
		s *= 0.5;
		branch = 0;
		closing = 0;
		for (i = 0; i < 3; i++) {	/* do center */
			v[i] = beg[i] + s;
			if (p[i] > v[i]) {
				branch |= 1<<i;
				if (p[i] - v[i] > EPSILON)
					closing++;
			} else if (v[i] - p[i] > EPSILON)
				closing++;
		}
		fc = 0.0;
		for (j = 0; j < 8; j++)
			fc += fval[j];
		fc = 0.125*fc + s*frand3(v[0],v[1],v[2]);
		if (closing == 0)
			return(fc);		/* close enough */
		fval[~branch & 7] = fc;
		for (i = 0; i < 3; i++) {	/* do faces */
			if (branch & 1<<i)
				v[i] += s;
			else
				v[i] -= s;
			fc = 0.0;
			for (j = 0; j < 8; j++)
				if (~(j^branch) & 1<<i)
					fc += fval[j];
			fc = 0.25*fc + s*frand3(v[0],v[1],v[2]);
			fval[~(branch^1<<i) & 7] = fc;
			v[i] = beg[i] + s;
		}
		for (i = 0; i < 3; i++) {	/* do edges */
			j = (i+1)%3;
			if (branch & 1<<j)
				v[j] += s;
			else
				v[j] -= s;
			j = (i+2)%3;
			if (branch & 1<<j)
				v[j] += s;
			else
				v[j] -= s;
			fc = fval[branch & ~(1<<i)];
			fc += fval[branch | 1<<i];
			fc = 0.5*fc + s*frand3(v[0],v[1],v[2]);
			fval[branch^1<<i] = fc;
			j = (i+1)%3;
			v[j] = beg[j] + s;
			j = (i+2)%3;
			v[j] = beg[j] + s;
		}
		for (i = 0; i < 3; i++)		/* new cube */
			if (branch & 1<<i)
				beg[i] += s;
	}
}
